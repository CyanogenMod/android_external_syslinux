ifneq ($(strip $(TARGET_NO_BOOTLOADER)),true)
ifeq ($(TARGET_USE_SYSLINUX),true)

include $(all-subdir-makefiles)

endif
endif
