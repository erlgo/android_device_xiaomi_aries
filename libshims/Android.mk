LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    	boringssl-compat/bio_b64.c \
	boringssl-compat/p_dec.c \
	boringssl-compat/p_enc.c \
	boringssl-compat/p_open.c \
	boringssl-compat/p_seal.c \
	boringssl-compat/rsa_pss.c 
LOCAL_SHARED_LIBRARIES := liblog libcrypto
LOCAL_MODULE := libshim_boringssl
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

# camera
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
     gui/SensorManager.cpp \
     ui/GraphicBuffer.cpp \
     ui/GraphicBufferAllocator.cpp \
     ui/GraphicBufferMapper.cpp \
     MemoryHeapPmem.cpp \
     MemoryBase.c \
     SharedBuffer.cpp \
     VectorImpl.cpp

LOCAL_C_INCLUDES += external/safe-iop/include
LOCAL_C_INCLUDES += system/core/libutils

LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware libui libgui libbinder libutils libsync

LOCAL_MODULE := libshim_camera
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

# libshim_ril.so
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	ril_shim.cpp

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libutils \
	libril \
	libdl \
	libbinder

LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libril

LOCAL_MODULE := libshim_ril
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
