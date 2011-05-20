ifeq ($(TARGET_BOOTIMAGE_TYPE),syslinux)

include $(all-subdir-makefiles)

endif # TARGET_BOOTIMAGE_TYPE==syslinux
