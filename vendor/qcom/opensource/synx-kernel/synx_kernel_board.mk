# Build synx kernel driver

TARGET_SYNX_ENABLE := false
ifeq ($(TARGET_KERNEL_DLKM_DISABLE),true)
	ifeq ($(TARGET_KERNEL_DLKM_SYNX_OVERRIDE),true)
		TARGET_SYNX_ENABLE := true
	endif
else
TARGET_SYNX_ENABLE := true
endif
#
ifeq ($(TARGET_SYNX_ENABLE), true)
ifeq ($(call is-board-platform-in-list,$(TARGET_BOARD_PLATFORM)),true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/synx-driver.ko
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/ipclite.ko
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/ipclite_test.ko
BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/synx-driver.ko
BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/ipclite.ko
#BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD += $(KERNEL_MODULES_OUT)/synx-driver.ko
endif
endif
