LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := syslinux_libfat

LOCAL_SRC_FILES := \
	cache.c \
	fatchain.c \
	open.c \
	searchdir.c

include $(BUILD_HOST_STATIC_LIBRARY)
