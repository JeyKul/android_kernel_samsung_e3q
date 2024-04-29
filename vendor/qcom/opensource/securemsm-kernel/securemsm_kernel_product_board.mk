#Build ssg kernel driver

ENABLE_SECUREMSM_DLKM := true
ENABLE_SECUREMSM_QTEE_DLKM := true

ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
  ifeq ($(TARGET_KERNEL_DLKM_SECURE_MSM_OVERRIDE), false)
      ENABLE_SECUREMSM_DLKM := false
  endif
  ifeq ($(TARGET_KERNEL_DLKM_SECUREMSM_QTEE_OVERRIDE), false)
      ENABLE_SECUREMSM_QTEE_DLKM := false
  endif
endif

ifeq ($(ENABLE_SECUREMSM_DLKM), true)
PRODUCT_PACKAGES += qcedev-mod_dlkm.ko
PRODUCT_PACKAGES += qce50_dlkm.ko
PRODUCT_PACKAGES += qcrypto-msm_dlkm.ko
PRODUCT_PACKAGES += hdcp_qseecom_dlkm.ko
PRODUCT_PACKAGES += qrng_dlkm.ko

ifeq ($(TARGET_USES_SMMU_PROXY), true)
PRODUCT_PACKAGES += smmu_proxy_dlkm.ko
endif
endif #ENABLE_SECUREMSM_DLKM

ifeq ($(ENABLE_SECUREMSM_QTEE_DLKM), true)
PRODUCT_PACKAGES += smcinvoke_dlkm.ko
PRODUCT_PACKAGES += tz_log_dlkm.ko
#Enable Qseecom if TARGET_ENABLE_QSEECOM or TARGET_BOARD_AUTO is set to true
ifneq (, $(filter true, $(TARGET_ENABLE_QSEECOM) $(TARGET_BOARD_AUTO)))
PRODUCT_PACKAGES += qseecom_dlkm.ko
endif #TARGET_ENABLE_QSEECOM OR TARGET_BOARD_AUTO
endif #ENABLE_SECUREMSM_QTEE_DLKM


