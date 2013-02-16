LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := OpusRecord

LOCAL_SRC_FILES := OpusRecord.cpp xlog.cpp recorder.cpp opusenc/audio-in.c opusenc/diag_range.c opusenc/opus_header.c opusenc/opusenc.c opusenc/wav_io.c

LOCAL_LDLIBS    += -llog -lOpenSLES  -logg -lopus -L$(LOCAL_PATH)/lib
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

include $(BUILD_SHARED_LIBRARY)



