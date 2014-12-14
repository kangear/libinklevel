LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := ink
LOCAL_C_INCLUDES += \
	external/libieee1284/ \
	external/libieee1284/include \
	external/libinklevel
#LOCAL_LDLIBS   += -lieee1284 -linklevel
LOCAL_SRC_FILES := $(call all-subdir-c-files)
LOCAL_STATIC_LIBRARIES += \
	libinklevel

LOCAL_STATIC_LIBRARIES += \
	libieee1284
	
include $(BUILD_EXECUTABLE)
