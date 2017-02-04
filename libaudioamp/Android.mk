LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
	liblog libutils libcutils libdl

LOCAL_SRC_FILES := \
	audio_amplifier.c

LOCAL_MODULE := libaudioamp

LOCAL_MODULE_TAGS := optional

# kernel includes
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include

# audio HAL includes
#LOCAL_C_INCLUDES += \
#	external/tinyalsa/include \
#	external/tinycompress/include \
#	$(call include-path-for, audio-route) \
#	$(call include-path-for, audio-effects) \
#	$(call project-path-for,qcom-audio)/hal \
#	$(call project-path-for,qcom-audio)/hal/$(TARGET_BOARD_PLATFORM) \
#	$(call project-path-for,qcom-audio)/hal/audio_extn
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_C_INCLUDES += hardware/qcom/audio-caf/msm8960/alsa_sound
LOCAL_C_INCLUDES += hardware/qcom/audio-caf/msm8960/libalsa-intf

ifeq ($(strip $(BOARD_USES_QCOM_HARDWARE)),true)
    LOCAL_CFLAGS += -DAUDIO_CAF
endif

include $(BUILD_SHARED_LIBRARY)
