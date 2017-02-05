/*
 * Copyright (C) 2015 The CyanogenMod Project
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
/*#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <utils/Log.h>

#include <telephony/ril.h>

#define LOG_TAG "RIL_SHIM_LIBS"

const RIL_RadioFunctions *RIL_SAP_Init(const struct RIL_Env *env, int argc, char **argv) {
	RLOGD("Need to implentment RIL_SAP_Init\n");
	return RIL_SAP_Init(env, argc, argv);
}*/
#include <stdlib.h>

extern "C" void RIL_onRequestAck() {}
extern "C" void RIL_register_socket() {}

/* status_t Parcel::writeString16 */
extern "C" int _ZN7android6Parcel13writeString16EPKDsj();
extern "C" int _ZN7android6Parcel13writeString16EPKtj() {
    return _ZN7android6Parcel13writeString16EPKDsj();
}
