TOUCH_DLKM_ENABLE := false
ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
        ifeq ($(TARGET_KERNEL_DLKM_TOUCH_OVERRIDE), false)
                TOUCH_DLKM_ENABLE := false
        endif
endif

ifeq ($(TOUCH_DLKM_ENABLE),  true)
        ifeq ($(TARGET_BOARD_PLATFORM), monaco)
                PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/pt_ts.ko \
                        $(KERNEL_MODULES_OUT)/pt_i2c.ko \
                        $(KERNEL_MODULES_OUT)/pt_device_access.ko \
                        $(KERNEL_MODULES_OUT)/raydium_ts.ko
        else ifeq ($(TARGET_BOARD_PLATFORM), kona)
                PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/focaltech_fts.ko
        else ifeq ($(TARGET_BOARD_PLATFORM), pineapple)
                PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/nt36xxx-i2c.ko \
                        $(KERNEL_MODULES_OUT)/goodix_ts.ko \
                        $(KERNEL_MODULES_OUT)/atmel_mxt_ts.ko
        else ifeq ($(TARGET_BOARD_PLATFORM), kalama)
                PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/nt36xxx-i2c.ko \
                        $(KERNEL_MODULES_OUT)/goodix_ts.ko \
                        $(KERNEL_MODULES_OUT)/atmel_mxt_ts.ko
        else ifeq ($(TARGET_BOARD_PLATFORM), blair)
                PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/focaltech_fts.ko \
                        $(KERNEL_MODULES_OUT)/nt36xxx-i2c.ko \
                        $(KERNEL_MODULES_OUT)/synaptics_tcm_ts.ko \
                        $(KERNEL_MODULES_OUT)/goodix_ts.ko
        else ifeq ($(TARGET_BOARD_PLATFORM), crow)
                PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/goodix_ts.ko
        else ifeq ($(TARGET_BOARD_PLATFORM), bengal)
                PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/synaptics_tcm_ts.ko \
                        $(KERNEL_MODULES_OUT)/nt36xxx-i2c.ko
        else ifeq ($(TARGET_BOARD_PLATFORM), trinket)
                PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/synaptics_tcm_ts.ko
        else
                PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/nt36xxx-i2c.ko \
                        $(KERNEL_MODULES_OUT)/goodix_ts.ko \
                        $(KERNEL_MODULES_OUT)/atmel_mxt_ts.ko \
                        $(KERNEL_MODULES_OUT)/synaptics_tcm_ts.ko
        endif
endif
