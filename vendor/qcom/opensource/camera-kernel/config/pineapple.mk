# SPDX-License-Identifier: GPL-2.0-only

# Localized KCONFIG settings
CONFIG_SPECTRA_ISP := y
CONFIG_SPECTRA_ICP := y
CONFIG_SPECTRA_JPEG := y
CONFIG_SPECTRA_SENSOR := y
CONFIG_SPECTRA_LLCC_STALING := y
CONFIG_SPECTRA_USE_RPMH_DRV_API := y
CONFIG_SPECTRA_USE_CLK_CRM_API := y
CONFIG_DOMAIN_ID_SECURE_CAMERA := y

# Flags to pass into C preprocessor
ccflags-y += -DCONFIG_SPECTRA_ISP=1
ccflags-y += -DCONFIG_SPECTRA_ICP=1
ccflags-y += -DCONFIG_SPECTRA_JPEG=1
ccflags-y += -DCONFIG_SPECTRA_SENSOR=1
ccflags-y += -DCONFIG_SPECTRA_LLCC_STALING=1
ccflags-y += -DCONFIG_SPECTRA_USE_RPMH_DRV_API=1
ccflags-y += -DCONFIG_SPECTRA_USE_CLK_CRM_API=1
ccflags-y += -DCONFIG_DOMAIN_ID_SECURE_CAMERA=1

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),mu1q mu2q mu3q e1q e2q e3q q6q b6q))
CONFIG_SAMSUNG_OIS_MCU_STM32 := y
CONFIG_CAMERA_SYSFS_V2 := y
CONFIG_CAMERA_FRAME_CNT_DBG := y
CONFIG_CAMERA_RF_MIPI := y
CONFIG_SAMSUNG_DEBUG_SENSOR_I2C := y
CONFIG_SAMSUNG_DEBUG_SENSOR_TIMING := y
CONFIG_SAMSUNG_DEBUG_HW_INFO := y
CONFIG_SENSOR_RETENTION := y
CONFIG_CAMERA_ADAPTIVE_MIPI := y
CONFIG_CAMERA_CDR_TEST := y
CONFIG_CAMERA_HW_ERROR_DETECT := y
CONFIG_SAMSUNG_CAMERA := y
ifneq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),b6q))
CONFIG_SAMSUNG_REAR_TRIPLE := y
endif
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),mu1q e1q))
CONFIG_SEC_E1Q_PROJECT := y
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),mu2q e2q))
CONFIG_SEC_E2Q_PROJECT := y
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),mu3q e3q))
CONFIG_SEC_E3Q_PROJECT := y
CONFIG_SAMSUNG_REAR_QUADRA := y
CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING := y
CONFIG_SAMSUNG_READ_BPC_FROM_OTP := y
CONFIG_SAMSUNG_WACOM_NOTIFIER := y
CONFIG_SAMSUNG_SUPPORT_RUMBA_FW_UPDATE := y
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),q6q))
CONFIG_SEC_Q6Q_PROJECT := y
CONFIG_SAMSUNG_FRONT_TOP :=y
CONFIG_SAMSUNG_FRONT_TOP_EEPROM :=y
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),b6q))
CONFIG_SEC_B6Q_PROJECT := y
CONFIG_SEC_GPIO_ENABLED_VREG := y
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),mu1q mu2q mu3q e1q e2q e3q q6q b6q))
ccflags-y += -DCONFIG_SAMSUNG_OIS_MCU_STM32=1
ccflags-y += -DCONFIG_CAMERA_SYSFS_V2=1
ccflags-y += -DCONFIG_CAMERA_FRAME_CNT_DBG=1
ccflags-y += -DCONFIG_CAMERA_FRAME_CNT_CHECK=1
ccflags-y += -DCONFIG_SAMSUNG_FRONT_EEPROM=1
ifneq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),b6q))
ccflags-y += -DCONFIG_SAMSUNG_REAR_DUAL=1
ccflags-y += -DCONFIG_SAMSUNG_REAR_TRIPLE=1
endif
ccflags-y += -DCONFIG_USE_CAMERA_HW_BIG_DATA=1
ccflags-y += -DCONFIG_SAMSUNG_ACTUATOR_READ_HALL_VALUE=1
ccflags-y += -DCONFIG_CAMERA_RF_MIPI=1
ccflags-y += -DCONFIG_SAMSUNG_DEBUG_SENSOR_I2C=1
ccflags-y += -DCONFIG_SAMSUNG_DEBUG_SENSOR_TIMING=1
ccflags-y += -DCONFIG_SAMSUNG_DEBUG_HW_INFO=1
ccflags-y += -DCONFIG_CAMERA_ADAPTIVE_MIPI=1
ccflags-y += -DCONFIG_SENSOR_RETENTION=1
ccflags-y += -DCONFIG_CAMERA_CDR_TEST=1
ccflags-y += -DCONFIG_CAMERA_HW_ERROR_DETECT=1
ccflags-y += -DCONFIG_SAMSUNG_CAMERA=1
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),mu1q e1q))
ccflags-y += -DCONFIG_SEC_E1Q_PROJECT=1
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),mu2q e2q))
ccflags-y += -DCONFIG_SEC_E2Q_PROJECT=1
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),mu3q e3q))
ccflags-y += -DCONFIG_SEC_E3Q_PROJECT=1
ccflags-y += -DCONFIG_SAMSUNG_REAR_QUADRA=1
ccflags-y += -DCONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING=1
ccflags-y += -DCONFIG_SAMSUNG_READ_BPC_FROM_OTP=1
ccflags-y += -DCONFIG_SAMSUNG_WACOM_NOTIFIER=1
ccflags-y += -DCONFIG_SAMSUNG_SUPPORT_RUMBA_FW_UPDATE=1
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),q6q))
ccflags-y += -DCONFIG_SEC_Q6Q_PROJECT=1
ccflags-y += -DCONFIG_SAMSUNG_FRONT_TOP=1
ccflags-y += -DCONFIG_SAMSUNG_FRONT_TOP_EEPROM=1
endif

ifeq ($(PROJECT_NAME), $(filter $(PROJECT_NAME),b6q))
ccflags-y += -DCONFIG_SEC_B6Q_PROJECT=1
ccflags-y += -DCONFIG_SEC_GPIO_ENABLED_VREG=1
endif

# External Dependencies
KBUILD_CPPFLAGS += -DCONFIG_MSM_MMRM=1
ifeq ($(CONFIG_QCOM_VA_MINIDUMP), y)
KBUILD_CPPFLAGS += -DCONFIG_QCOM_VA_MINIDUMP=1
endif