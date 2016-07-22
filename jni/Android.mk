LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LS_CPP = $(subst $(1)/,,$(wildcard $(1)/$(2)/*.cpp))

LOCAL_MODULE     := BulletTime
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/Sources
LOCAL_SRC_FILES  := JNI.cpp                                    \
                    $(call LS_CPP,$(LOCAL_PATH),Sources)       \
                    $(call LS_CPP,$(LOCAL_PATH),Sources/Level) \
                    $(call LS_CPP,$(LOCAL_PATH),Sources/Frame) \
                    $(call LS_CPP,$(LOCAL_PATH),Sources/Wifi)  \
                    $(call LS_CPP,$(LOCAL_PATH),Sources/Video) \
                    $(call LS_CPP,$(LOCAL_PATH),Sources/Share)
LOCAL_CPPFLAGS         := -D__ANDROID__ -fPIC -fexceptions -Wmultichar -ffunction-sections -fdata-sections -std=c++98 \
                          -std=gnu++98 -fno-rtti
LOCAL_STATIC_LIBRARIES := boost_system boost_thread boost_math_c99f boost_regex boost_filesystem
LOCAL_SHARED_LIBRARIES := libogg libvorbis openal libeng gstreamer_android 
LOCAL_LDLIBS           := -llog -landroid -lEGL -lGLESv2
LOCAL_LDFLAGS          := -Wl -gc-sections

include $(BUILD_SHARED_LIBRARY)

GSTREAMER_SDK_ROOT        := $(GSTREAMER_SDK_ROOT_ANDROID)
GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_SDK_ROOT)/share/gst-android/ndk-build
GSTREAMER_PLUGINS         := coreelements matroska videoconvert vpx jpeg multifile x264 isomp4 playback videorate libav \
                             vorbis audioconvert ogg wavparse wavenc audioresample voaacenc faad
GSTREAMER_EXTRA_DEPS      := gstreamer-video-1.0

include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk

$(call import-module, boost_1_53_0)
$(call import-module, libogg-vorbis)
$(call import-module, openal-1_15_1)

$(call import-add-path, /home/pascal/workspace)
$(call import-module, libeng)
