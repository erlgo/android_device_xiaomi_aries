/*
 * Copyright (C) 2015, The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "audio_amplifier"
//#define LOG_NDEBUG 0

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <cutils/log.h>

#include <hardware/audio_amplifier.h>

#include <pthread.h>
#include <cutils/properties.h>

#include <system/audio.h>
#include <platform.h>
#include <audio_hw.h>
#include <sound/es310.h>

#define UNUSED __attribute__((unused))

#define ES310_DEVICE "/dev/audience_es310"

typedef struct aries_device {
    amplifier_device_t amp_dev;
    uint32_t current_input_devices;
    uint32_t current_output_devices;
    audio_mode_t current_mode;
} aries_device_t;

static aries_device_t *aries_dev = NULL;

static int mES310Fd = -1;
static pthread_mutex_t mES310Mutex;
static enum ES310_PathID dwOldPath = ES310_PATH_SUSPEND;
static unsigned int dwOldPreset = -1;

static audio_mode_t mMode = AUDIO_MODE_NORMAL;
static uint32_t in_snd_device = SND_DEVICE_NONE;
static uint32_t out_snd_device = SND_DEVICE_NONE;

static const char *es310_getNameByPresetID(int presetID)
{
	switch (presetID) {
	case ES310_PRESET_HANDSET_INCALL_NB:
		return "ES310_PRESET_HANDSET_INCALL_NB";
	case ES310_PRESET_HEADSET_INCALL_NB:
		return "ES310_PRESET_HEADSET_INCALL_NB";
	case ES310_PRESET_HANDSET_INCALL_NB_1MIC:
		return "ES310_PRESET_HANDSET_INCALL_NB_1MIC";
	case ES310_PRESET_HANDSFREE_INCALL_NB:
		return "ES310_PRESET_HANDSFREE_INCALL_NB";
	case ES310_PRESET_HANDSET_INCALL_WB:
		return "ES310_PRESET_HANDSET_INCALL_WB";
	case ES310_PRESET_HEADSET_INCALL_WB:
		return "ES310_PRESET_HEADSET_INCALL_WB";
	case ES310_PRESET_AUDIOPATH_DISABLE:
		return "ES310_PRESET_AUDIOPATH_DISABLE";
	case ES310_PRESET_HANDSFREE_INCALL_WB:
		return "ES310_PRESET_HANDSFREE_INCALL_WB";
	case ES310_PRESET_HANDSET_VOIP_WB:
		return "ES310_PRESET_HANDSET_VOIP_WB";
	case ES310_PRESET_HEADSET_VOIP_WB:
		return "ES310_PRESET_HEADSET_VOIP_WB";
	case ES310_PRESET_HANDSFREE_REC_WB:
		return "ES310_PRESET_HANDSFREE_REC_WB";
	case ES310_PRESET_HANDSFREE_VOIP_WB:
		return "ES310_PRESET_HANDSFREE_VOIP_WB";
	case ES310_PRESET_VOICE_RECOGNIZTION_WB:
		return "ES310_PRESET_VOICE_RECOGNIZTION_WB";
	case ES310_PRESET_HANDSET_INCALL_VOIP_WB_1MIC:
		return "ES310_PRESET_HANDSET_INCALL_VOIP_WB_1MIC";
	case ES310_PRESET_ANALOG_BYPASS:
		return "ES310_PRESET_ANALOG_BYPASS";
	case ES310_PRESET_HEADSET_MIC_ANALOG_BYPASS:
		return "ES310_PRESET_HEADSET_MIC_ANALOG_BYPASS";
	default:
		return "Unknown";
	}
	return "Unknown";
}

static const char *es310_getNameByPathID(int pathID)
{
	switch (pathID) {
	case ES310_PATH_SUSPEND:
		return "ES310_PATH_SUSPEND";
	case ES310_PATH_HANDSET:
		return "ES310_PATH_HANDSET";
	case ES310_PATH_HEADSET:
		return "ES310_PATH_HEADSET";
	case ES310_PATH_HANDSFREE:
		return "ES310_PATH_HANDSFREE";
	case ES310_PATH_BACKMIC:
		return "ES310_PATH_BACKMIC";
	default:
		return "Unknown";
	}
	return "Unknown";
}

static int es310_init(void)
{
	int rc = -1;

	ALOGD("%s\n", __func__);

	// open
	mES310Fd = open(ES310_DEVICE, O_RDWR | O_NONBLOCK, 0);
	if (mES310Fd < 0) {
		ALOGE("%s: unable to open es310 device!", __func__);
		return rc;
	}
	// reset
	rc = ioctl(mES310Fd, ES310_RESET_CMD);
	if (rc) {
		ALOGE("%s: ES310_RESET_CMD fail, rc=%d", __func__, rc);
		close(mES310Fd);
		mES310Fd = -1;
		return rc;
	}
	// sync
	rc = ioctl(mES310Fd, ES310_SYNC_CMD, NULL);
	if (rc) {
		ALOGE("%s: ES310_SYNC_CMD fail, rc=%d", __func__, rc);
		close(mES310Fd);
		mES310Fd = -1;
		return rc;
	}

	return rc;
}

static int es310_wakeup(void)
{
	int rc = -1;

	ALOGD("%s\n", __func__);

	if (mES310Fd < 0) {
		ALOGE("%s: codec not initialized.\n", __func__);
		return rc;
	}

	int retry = 4;
	do {
		ALOGV("%s: ioctl ES310_SET_CONFIG retry:%d", __func__,
		      4 - retry);
		rc = ioctl(mES310Fd, ES310_WAKEUP_CMD, NULL);
		if (!rc)
			break;
		else
			ALOGE("%s: ES310_SET_CONFIG fail rc=%d", __func__, rc);
	} while (--retry);

	return rc;
}

static int es310_do_route(void)
{
	int rc = -1;
	char cVNRMode[255] = "2";
	int VNRMode = 2;
	enum ES310_PathID dwNewPath = ES310_PATH_SUSPEND;
	unsigned int dwNewPreset = -1;

	if (mES310Fd < 0) {
		ALOGE("%s: codec not initialized.\n", __func__);
		return rc;
	}

	pthread_mutex_lock(&mES310Mutex);

	// original usecase: suspend if we don't use a mic
	// if we don't use any input device codec config doesn't matter.
	// and since we'll get NONE as device even if there are active devices
	// we just don't do anything in this case so we're not going to select wrong routes
	if (in_snd_device == SND_DEVICE_NONE) {
		ALOGD("%s: Normal mode, RX no routing\n", __func__);
		goto unlock;

		// we have no way to detect if mic routes are disabled right now
		// I don't know if this is even needed (power usage?)
		// dwNewPath = ES310_PATH_SUSPEND;
		// goto route;
	}
	// 1mic mode
	property_get("persist.audio.vns.mode", cVNRMode, "2");
	if (!strncmp("1", cVNRMode, 1))
		VNRMode = 1;
	else
		VNRMode = 2;

	if (mMode == AUDIO_MODE_IN_CALL || mMode == AUDIO_MODE_IN_COMMUNICATION) {
		// this needs a modification in platform.c, otherwise non-voice ucm rules will be selected
		bool is_voip = (mMode == AUDIO_MODE_IN_COMMUNICATION);

		switch (in_snd_device) {
		// speaker, hdmi
		case SND_DEVICE_IN_SPEAKER_MIC:
		case SND_DEVICE_IN_SPEAKER_MIC_AEC:
		case SND_DEVICE_IN_VOICE_SPEAKER_MIC:
#ifdef AUDIO_CAF
		case SND_DEVICE_IN_VOICE_SPEAKER_DMIC:
		case SND_DEVICE_IN_VOICE_SPEAKER_QMIC:
#else
		case SND_DEVICE_IN_VOICE_SPEAKER_DMIC_EF:
		case SND_DEVICE_IN_VOICE_SPEAKER_DMIC_BS:
#endif
		case SND_DEVICE_IN_HDMI_MIC:
			dwNewPath = ES310_PATH_HANDSFREE;
			dwNewPreset =
			    is_voip ? ES310_PRESET_HANDSFREE_VOIP_WB :
			    ES310_PRESET_HANDSFREE_INCALL_NB;
			break;

		// headset
		case SND_DEVICE_IN_HEADSET_MIC:
		case SND_DEVICE_IN_HEADSET_MIC_AEC:
		case SND_DEVICE_IN_VOICE_HEADSET_MIC:
		case SND_DEVICE_IN_VOICE_TTY_FULL_HEADSET_MIC:
		case SND_DEVICE_IN_VOICE_TTY_HCO_HEADSET_MIC:
			dwNewPath = ES310_PATH_HEADSET;
			dwNewPreset =
			    is_voip ? ES310_PRESET_HEADSET_VOIP_WB :
			    ES310_PRESET_HEADSET_INCALL_NB;
			break;

		// handset, dmic, voice-rec, bluetooth
		case SND_DEVICE_IN_HANDSET_MIC:
		case SND_DEVICE_IN_HANDSET_MIC_AEC:
		case SND_DEVICE_IN_VOICE_TTY_VCO_HANDSET_MIC:
#ifdef AUDIO_CAF
		case SND_DEVICE_IN_VOICE_DMIC:
#else
		case SND_DEVICE_IN_VOICE_DMIC_EF:
		case SND_DEVICE_IN_VOICE_DMIC_BS:
		case SND_DEVICE_IN_VOICE_DMIC_EF_TMUS:
#endif
		case SND_DEVICE_IN_VOICE_REC_MIC:
#ifdef AUDIO_CAF
		case SND_DEVICE_IN_VOICE_REC_DMIC:
		case SND_DEVICE_IN_VOICE_REC_DMIC_FLUENCE:
#else
		case SND_DEVICE_IN_VOICE_REC_DMIC_EF:
		case SND_DEVICE_IN_VOICE_REC_DMIC_BS:
		case SND_DEVICE_IN_VOICE_REC_DMIC_EF_FLUENCE:
		case SND_DEVICE_IN_VOICE_REC_DMIC_BS_FLUENCE:
#endif
		case SND_DEVICE_IN_BT_SCO_MIC:
		case SND_DEVICE_IN_BT_SCO_MIC_WB:
		default:
			dwNewPath = ES310_PATH_HANDSET;
			dwNewPreset =
			    is_voip ? ES310_PRESET_HANDSET_VOIP_WB :
			    ES310_PRESET_HANDSET_INCALL_NB;
			break;
		}
	} else {
		switch (in_snd_device) {
		// HDMI
		case SND_DEVICE_IN_HDMI_MIC:
			dwNewPath = ES310_PATH_HANDSET;
			dwNewPreset = ES310_PRESET_HANDSFREE_REC_WB;
			break;

		// bluetooth
		case SND_DEVICE_IN_BT_SCO_MIC:
		case SND_DEVICE_IN_BT_SCO_MIC_WB:
			dwNewPath = ES310_PATH_HEADSET;
			dwNewPreset = ES310_PRESET_HANDSFREE_REC_WB;
			break;

		// voice-rec-*
		case SND_DEVICE_IN_VOICE_REC_MIC:
#ifdef AUDIO_CAF
		case SND_DEVICE_IN_VOICE_REC_DMIC:
		case SND_DEVICE_IN_VOICE_REC_DMIC_FLUENCE:
#else
		case SND_DEVICE_IN_VOICE_REC_DMIC_EF:
		case SND_DEVICE_IN_VOICE_REC_DMIC_BS:
		case SND_DEVICE_IN_VOICE_REC_DMIC_EF_FLUENCE:
		case SND_DEVICE_IN_VOICE_REC_DMIC_BS_FLUENCE:
#endif
			dwNewPath = ES310_PATH_HANDSET;
			dwNewPreset = ES310_PRESET_VOICE_RECOGNIZTION_WB;
			break;

		// headset-mic
		case SND_DEVICE_IN_HEADSET_MIC:
		case SND_DEVICE_IN_HEADSET_MIC_AEC:
			dwNewPath = ES310_PATH_HEADSET;
			dwNewPreset = ES310_PRESET_HEADSET_MIC_ANALOG_BYPASS;
			break;

		// handset-mic, camcorder-mic, speaker-mic
		case SND_DEVICE_IN_HANDSET_MIC:
		case SND_DEVICE_IN_HANDSET_MIC_AEC:
		case SND_DEVICE_IN_CAMCORDER_MIC:
		case SND_DEVICE_IN_SPEAKER_MIC:
		case SND_DEVICE_IN_SPEAKER_MIC_AEC:
		default:
			dwNewPath = ES310_PATH_HANDSET;
			dwNewPreset = ES310_PRESET_ANALOG_BYPASS;
			break;
		}
	}

route:
	if (VNRMode == 1) {
		ALOGV("%s: Switch to 1-Mic Solution", __func__);
		if (dwNewPreset == ES310_PRESET_HANDSET_INCALL_NB) {
			dwNewPreset = ES310_PRESET_HANDSET_INCALL_NB_1MIC;
		}
		if (dwNewPreset == ES310_PRESET_HANDSET_VOIP_WB) {
			dwNewPreset = ES310_PRESET_HANDSET_INCALL_VOIP_WB_1MIC;
		}
	}
	// still the same path and preset
	if (dwOldPath == dwNewPath && dwOldPreset == dwNewPreset) {
		rc = 0;
		goto unlock;
	}

	ALOGD("%s: path=%s->%s, preset=%s->%s", __func__,
	      es310_getNameByPathID(dwOldPath),
	      es310_getNameByPathID(dwNewPath),
	      es310_getNameByPresetID(dwOldPreset),
	      es310_getNameByPresetID(dwNewPreset));

	// wakeup if suspended
	if (dwOldPath == ES310_PATH_SUSPEND) {
		rc = es310_wakeup();
		if (rc) {
			ALOGE("%s: es310_wakeup() failed rc=%d\n", __func__,
			      rc);
			goto recover;
		}
	}
	// path
	if (dwOldPath != dwNewPath) {
		int retry = 4;
		do {
			ALOGE("%s: ioctl ES310_SET_CONFIG newPath:%d, retry:%d",
			      __func__, dwNewPath, (4 - retry));
			rc = ioctl(mES310Fd, ES310_SET_CONFIG, &dwNewPath);

			if (!rc) {
				dwOldPath = dwNewPath;
				break;
			} else {
				ALOGE("%s: ES310_SET_CONFIG rc=%d", __func__,
				      rc);
			}
		} while (--retry);

		if (rc)
			goto recover;
	}
	// preset
	if (dwNewPath != ES310_PATH_SUSPEND && (dwOldPreset != dwNewPreset)) {
		int retry = 4;
		do {
			ALOGE
			    ("%s: ioctl ES310_SET_PRESET newPreset:0x%x, retry:%d",
			     __func__, dwNewPreset, (4 - retry));
			rc = ioctl(mES310Fd, ES310_SET_PRESET, &dwNewPreset);

			if (!rc) {
				dwOldPreset = dwNewPreset;
				break;
			} else {
				ALOGE("%s: ES310_SET_PRESET rc=%d", __func__,
				      rc);
			}
		} while (--retry);

		if (rc)
			goto recover;
	}

recover:
	if (rc < 0) {
		ALOGE("%s: ES310 do hard reset to recover from error!\n",
		      __func__);

		// close device first
		close(mES310Fd);
		mES310Fd = -1;

		// re-init
		rc = es310_init();
		if (!rc) {
			// set new config
			rc = ioctl(mES310Fd, ES310_SET_CONFIG, &dwNewPath);
			if (rc) {
				ALOGE("%s: RECOVERY: ES310_SET_CONFIG rc=%d",
				      __func__, rc);
				goto unlock;
			}

			dwOldPath = dwNewPath;
		} else {
			ALOGE("%s: RECOVERY: es310_init() failed rc=%d\n",
			      __func__, rc);
			goto unlock;
		}
	}

unlock:
	pthread_mutex_unlock(&mES310Mutex);
	return rc;
}

static int amp_set_mode(amplifier_device_t *device, audio_mode_t mode)
{
	int ret = 0;
	aries_device_t *dev = (aries_device_t *) device;

	if ((mode < AUDIO_MODE_CURRENT) || (mode >= AUDIO_MODE_CNT)) {
		ALOGW("%s: invalid mode=%d\n", __func__, mode);
		ret = -1;
	} else {
		if (mMode != mode || dev->current_mode != mode) {
			ALOGD("%s: mode: %d->%d\n", __func__, dev->current_mode, mode);
			// TODO: Fix redundancy
			mMode = mode;
			dev->current_mode = mode;
			es310_do_route();
		}
	}

	return ret;
}

static int amp_set_input_devices(amplifier_device_t *device, uint32_t devices)
{
	aries_device_t *dev = (aries_device_t *) device;

	if (devices != 0 && devices != in_snd_device) {
		// TODO: Fix redundancy
		in_snd_device = devices;
		dev->current_input_devices = devices;

		es310_do_route();
	}

	return 0;
}

static int amp_set_output_devices(amplifier_device_t *device, uint32_t devices)
{
	aries_device_t *dev = (aries_device_t *) device;

	if (devices != 0 && devices != out_snd_device) {
		// TODO: Fix redundancy
		out_snd_device = devices;
		dev->current_output_devices = devices;

		es310_do_route();
	}

	return 0;
}

static int amp_dev_close(hw_device_t *device)
{
	close(mES310Fd);
	mES310Fd = -1;
	aries_device_t *dev = (aries_device_t *) device;

	free(dev);

	return 0;
}

static int amp_module_open(const hw_module_t *module, UNUSED const char *name,
        hw_device_t **device)
{
	if (aries_dev) {
		ALOGE("%s:%d: Unable to open second instance of ES310 amplifier\n",
			__func__, __LINE__);
		return -EBUSY;
	}

	aries_dev = calloc(1, sizeof(aries_device_t));
	if (!aries_dev) {
		ALOGE("%s:%d: Unable to allocate memory for amplifier device\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	aries_dev->amp_dev.common.tag = HARDWARE_DEVICE_TAG;
	aries_dev->amp_dev.common.module = (hw_module_t *) module;
	aries_dev->amp_dev.common.version = HARDWARE_DEVICE_API_VERSION(1, 0);
	aries_dev->amp_dev.common.close = amp_dev_close;

	aries_dev->amp_dev.set_input_devices = amp_set_input_devices;
	aries_dev->amp_dev.set_output_devices = amp_set_output_devices;
	aries_dev->amp_dev.enable_input_devices = NULL;
	aries_dev->amp_dev.enable_output_devices = NULL;
	aries_dev->amp_dev.set_mode = amp_set_mode;
	aries_dev->amp_dev.output_stream_start = NULL;
	aries_dev->amp_dev.input_stream_start = NULL;
	aries_dev->amp_dev.output_stream_standby = NULL;
	aries_dev->amp_dev.input_stream_standby = NULL;

	aries_dev->current_input_devices = SND_DEVICE_NONE;
	aries_dev->current_output_devices = SND_DEVICE_NONE;
	aries_dev->current_mode = AUDIO_MODE_NORMAL;

	*device = (hw_device_t *) aries_dev;

        // mutex init
	pthread_mutex_init(&mES310Mutex, NULL);

	return es310_init();
}

static struct hw_module_methods_t hal_module_methods = {
    .open = amp_module_open,
};

amplifier_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AMPLIFIER_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AMPLIFIER_HARDWARE_MODULE_ID,
        .name = "MI2 audio amplifier HAL",
        .author = "The CyanogenMod Open Source Project",
        .methods = &hal_module_methods,
    },
};
