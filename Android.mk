# this is now the default Inklevel build for Android
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng
# compile in ARM mode, since the glyph loader/renderer is a hotspot
# when loading complex pages in the browser
#
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	bjnp-debug.c \
	bjnp-io.c \
	canon.c \
	d4lib.c \
	epson_new.c \
	hp_new.c \
	libinklevel.c \
	linux.c \
	opensolaris.c \
	util.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/ \
	$(LOCAL_PATH)/include \
	external/libieee1284/include

LOCAL_CFLAGS += -W -Wall
#LOCAL_CFLAGS += -fPIC -DPIC
#LOCAL_CFLAGS += "-DDARWIN_NO_CARBON"
#LOCAL_CFLAGS += "-DFT2_BUILD_LIBRARY"

# the following is for testing only, and should not be used in final builds
# of the product
#LOCAL_CFLAGS += "-DTT_CONFIG_OPTION_BYTECODE_INTERPRETER"

LOCAL_CFLAGS += -O2

LOCAL_MODULE:= libinklevel

LOCAL_STATIC_LIBRARIES := \
	libieee1284
LOCAL_SHARED_LIBRARIES := liblog

include $(BUILD_STATIC_LIBRARY)

#############################################################
# Build the ink tool
#

# ink
#include $(LOCAL_PATH)/ink/Android.mk
