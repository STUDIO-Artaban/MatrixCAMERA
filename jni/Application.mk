APP_PLATFORM := android-13
APP_STL      := gnustl_static
APP_ABI      := armeabi #armeabi-v7a
APP_OPTIM    := release #debug
APP_CFLAGS   := -DNDEBUG -UDEBUG #-DDEBUG -UNDEBUG

APP_MODULES  := libogg libvorbis openal libeng gstreamer_android BulletTime 
