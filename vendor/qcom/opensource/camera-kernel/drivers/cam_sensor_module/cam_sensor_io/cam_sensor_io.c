// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "cam_sensor_io.h"
#include "cam_sensor_i2c.h"
#include "cam_sensor_i3c.h"
#include <linux/pm_runtime.h>
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
#include "cam_sensor_mipi.h"
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#if defined(CONFIG_CAMERA_CDR_TEST)
#include "cam_clock_data_recovery.h"
#endif
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT) && defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
extern char rear_i2c_rfinfo[30];
static void camera_io_rear_i2c_rfinfo(void)
{
	struct cam_cp_noti_cell_infos cell_infos;

	get_rf_info(&cell_infos);
	CAM_INFO(CAM_CSIPHY,
		"[RF_MIPI_DBG] rat : %d, band : %d, channel : %d",
		cell_infos.cell_list[0].rat,
		cell_infos.cell_list[0].band,
		cell_infos.cell_list[0].channel);
	sprintf(rear_i2c_rfinfo, "%d,%d,%d\n", cell_infos.cell_list[0].rat, cell_infos.cell_list[0].band, cell_infos.cell_list[0].channel);
}
#endif
#endif

int32_t camera_io_dev_poll(struct camera_io_master *io_master_info,
	uint32_t addr, uint16_t data, uint32_t data_mask,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	uint32_t delay_ms)
{
	int16_t mask = data_mask & 0xFF;
	int32_t rc = 0;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	switch (io_master_info->master_type) {
	case CCI_MASTER:
		rc = cam_cci_i2c_poll(io_master_info->cci_client,
			addr, data, mask, data_type, addr_type, delay_ms);
		break;

	case I2C_MASTER:
		rc = cam_qup_i2c_poll(io_master_info->client,
			addr, data, data_mask, addr_type, data_type, delay_ms);
		break;

	case I3C_MASTER:
		rc = cam_qup_i3c_poll(io_master_info->i3c_client,
			addr, data, data_mask, addr_type, data_type, delay_ms);
		break;

	default:
		rc = -EINVAL;
		CAM_ERR(CAM_SENSOR, "Invalid Master Type: %d", io_master_info->master_type);
		break;
	}

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (rc < 0) {
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT) && defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
		camera_io_rear_i2c_rfinfo();
#endif
		sec_abc_send_event("MODULE=camera@WARN=i2c_fail");
#if defined(CONFIG_CAMERA_CDR_TEST)
		cam_clock_data_recovery_set_result(CDR_ERROR_I2C);
#endif
	}
#endif

	return rc;
}

int32_t camera_io_dev_erase(struct camera_io_master *io_master_info,
	uint32_t addr, uint32_t size)
{
	int32_t rc = 0;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	if (size == 0)
		return 0;

	switch (io_master_info->master_type) {
	case SPI_MASTER:
		CAM_DBG(CAM_SENSOR, "Calling SPI Erase");
		rc = cam_spi_erase(io_master_info, addr, CAMERA_SENSOR_I2C_TYPE_WORD, size);
		break;

	case I2C_MASTER:
	case CCI_MASTER:
	case I3C_MASTER:
		CAM_ERR(CAM_SENSOR, "Erase not supported on Master Type: %d",
			io_master_info->master_type);
		rc = -EINVAL;
		break;

	default:
		rc = -EINVAL;
		CAM_ERR(CAM_SENSOR, "Invalid Master Type: %d", io_master_info->master_type);
		break;
	}

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (rc < 0) {
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT) && defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
		camera_io_rear_i2c_rfinfo();
#endif
		sec_abc_send_event("MODULE=camera@WARN=i2c_fail");
#if defined(CONFIG_CAMERA_CDR_TEST)
		cam_clock_data_recovery_set_result(CDR_ERROR_I2C);
#endif
	}
#endif

	return rc;
}

int32_t camera_io_dev_read(struct camera_io_master *io_master_info,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	bool is_probing)
{
	int32_t rc = 0;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	switch (io_master_info->master_type) {
	case SPI_MASTER:
		rc = cam_spi_read(io_master_info, addr, data, addr_type, data_type);
		break;

	case I2C_MASTER:
		rc = cam_qup_i2c_read(io_master_info->client,
			addr, data, addr_type, data_type);
		break;

	case CCI_MASTER:
		rc = cam_cci_i2c_read(io_master_info->cci_client,
			addr, data, addr_type, data_type, is_probing);
		break;

	case I3C_MASTER:
		rc = cam_qup_i3c_read(io_master_info->i3c_client,
			addr, data, addr_type, data_type);
		break;

	default:
		rc = -EINVAL;
		CAM_ERR(CAM_SENSOR, "Invalid Master Type: %d", io_master_info->master_type);
		break;
	}

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (rc < 0) {
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT) && defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
		camera_io_rear_i2c_rfinfo();
#endif
		sec_abc_send_event("MODULE=camera@WARN=i2c_fail");
#if defined(CONFIG_CAMERA_CDR_TEST)
		cam_clock_data_recovery_set_result(CDR_ERROR_I2C);
#endif
	}
#endif

	return rc;
}

int32_t camera_io_dev_read_seq(struct camera_io_master *io_master_info,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type, int32_t num_bytes)
{
	int32_t rc = 0;

	switch (io_master_info->master_type) {
	case CCI_MASTER:
		rc = cam_camera_cci_i2c_read_seq(io_master_info->cci_client,
			addr, data, addr_type, data_type, num_bytes);
		break;

	case I2C_MASTER:
		rc = cam_qup_i2c_read_seq(io_master_info->client,
			addr, data, addr_type, num_bytes);
		break;

	case SPI_MASTER:
		rc = cam_spi_read_seq(io_master_info, addr, data, addr_type, num_bytes);
		break;

	case I3C_MASTER:
		rc = cam_qup_i3c_read_seq(io_master_info->i3c_client,
			addr, data, addr_type, num_bytes);
		break;

	default:
		rc = -EINVAL;
		CAM_ERR(CAM_SENSOR, "Invalid Master Type: %d", io_master_info->master_type);
		break;
	}

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (rc < 0) {
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT) && defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
		camera_io_rear_i2c_rfinfo();
#endif
		sec_abc_send_event("MODULE=camera@WARN=i2c_fail");
#if defined(CONFIG_CAMERA_CDR_TEST)
		cam_clock_data_recovery_set_result(CDR_ERROR_I2C);
#endif
	}
#endif

	return rc;
}

int32_t camera_io_dev_write(struct camera_io_master *io_master_info,
	struct cam_sensor_i2c_reg_setting *write_setting)
{
	int32_t rc = 0;

	if (!write_setting || !io_master_info) {
		CAM_ERR(CAM_SENSOR,
			"Input parameters not valid ws: %pK ioinfo: %pK",
			write_setting, io_master_info);
		return -EINVAL;
	}

	if (!write_setting->reg_setting) {
		CAM_ERR(CAM_SENSOR, "Invalid Register Settings");
		return -EINVAL;
	}

	switch (io_master_info->master_type) {
	case CCI_MASTER:
		rc = cam_cci_i2c_write_table(io_master_info, write_setting);
		break;

	case I2C_MASTER:
		rc = cam_qup_i2c_write_table(io_master_info, write_setting);
		break;

	case SPI_MASTER:
		rc = cam_spi_write_table(io_master_info, write_setting);
		break;

	case I3C_MASTER:
		rc = cam_qup_i3c_write_table(io_master_info, write_setting);
		break;

	default:
		rc = -EINVAL;
		CAM_ERR(CAM_SENSOR, "Invalid Master Type:%d", io_master_info->master_type);
		break;
	}

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (rc < 0) {
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT) && defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
		camera_io_rear_i2c_rfinfo();
#endif
		sec_abc_send_event("MODULE=camera@WARN=i2c_fail");
#if defined(CONFIG_CAMERA_CDR_TEST)
		cam_clock_data_recovery_set_result(CDR_ERROR_I2C);
#endif
	}
#endif

	return rc;
}

int32_t camera_io_dev_write_continuous(struct camera_io_master *io_master_info,
	struct cam_sensor_i2c_reg_setting *write_setting,
	uint8_t cam_sensor_i2c_write_flag)
{
	int32_t rc = 0;

	if (!write_setting || !io_master_info) {
		CAM_ERR(CAM_SENSOR,
			"Input parameters not valid ws: %pK ioinfo: %pK",
			write_setting, io_master_info);
		return -EINVAL;
	}

	if (!write_setting->reg_setting) {
		CAM_ERR(CAM_SENSOR, "Invalid Register Settings");
		return -EINVAL;
	}

	switch (io_master_info->master_type) {
	case CCI_MASTER:
		rc = cam_cci_i2c_write_continuous_table(io_master_info,
			write_setting, cam_sensor_i2c_write_flag);
		break;

	case I2C_MASTER:
		rc = cam_qup_i2c_write_continuous_table(io_master_info,
			write_setting, cam_sensor_i2c_write_flag);
		break;

	case SPI_MASTER:
		rc = cam_spi_write_table(io_master_info, write_setting);
		break;

	case I3C_MASTER:
		rc = cam_qup_i3c_write_continuous_table(io_master_info,
			write_setting, cam_sensor_i2c_write_flag);
		break;

	default:
		rc = -EINVAL;
		CAM_ERR(CAM_SENSOR, "Invalid Master Type:%d", io_master_info->master_type);
		break;
	}

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (rc < 0) {
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT) && defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
		camera_io_rear_i2c_rfinfo();
#endif
		sec_abc_send_event("MODULE=camera@WARN=i2c_fail");
#if defined(CONFIG_CAMERA_CDR_TEST)
		cam_clock_data_recovery_set_result(CDR_ERROR_I2C);
#endif
	}
#endif

	return rc;
}

int32_t camera_io_init(struct camera_io_master *io_master_info)
{
	int rc = 0;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	switch (io_master_info->master_type) {
	case CCI_MASTER:
		io_master_info->cci_client->cci_subdev = cam_cci_get_subdev(
						io_master_info->cci_client->cci_device);
		rc = cam_sensor_cci_i2c_util(io_master_info->cci_client, MSM_CCI_INIT);
		break;

	case I2C_MASTER:
	case I3C_MASTER:
		if ((io_master_info->client != NULL) &&
			(io_master_info->client->adapter != NULL)) {
			CAM_DBG(CAM_SENSOR, "%s:%d: Calling get_sync",
				__func__, __LINE__);
			rc = pm_runtime_get_sync(io_master_info->client->adapter->dev.parent);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Failed to get sync rc: %d", rc);
				return -EINVAL;
			}
		}
		rc = 0;
		break;

	case SPI_MASTER: return 0;
		rc = 0;
		break;

	default:
		rc = -EINVAL;
		CAM_ERR(CAM_SENSOR, "Invalid Master Type:%d", io_master_info->master_type);
		break;
	}

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (rc < 0) {
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT) && defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
		camera_io_rear_i2c_rfinfo();
#endif
		sec_abc_send_event("MODULE=camera@WARN=i2c_fail");
#if defined(CONFIG_CAMERA_CDR_TEST)
		cam_clock_data_recovery_set_result(CDR_ERROR_I2C);
#endif
	}
#endif

	return rc;
}

int32_t camera_io_release(struct camera_io_master *io_master_info)
{
	int32_t rc = 0;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	switch (io_master_info->master_type) {
	case CCI_MASTER:
		rc = cam_sensor_cci_i2c_util(io_master_info->cci_client, MSM_CCI_RELEASE);
		break;

	case I2C_MASTER:
	case I3C_MASTER:
		if ((io_master_info->client != NULL) &&
			(io_master_info->client->adapter != NULL)) {
			CAM_DBG(CAM_SENSOR, "%s:%d: Calling put_sync",
				__func__, __LINE__);
			pm_runtime_put_sync(io_master_info->client->adapter->dev.parent);
		}
		rc = 0;
		break;

	case SPI_MASTER: return 0;
		rc = 0;
		break;

	default:
		rc = -EINVAL;
		CAM_ERR(CAM_SENSOR, "Invalid Master Type:%d", io_master_info->master_type);
		break;
	}

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (rc < 0) {
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT) && defined(CONFIG_CAMERA_ADAPTIVE_MIPI) && defined(CONFIG_CAMERA_RF_MIPI)
		camera_io_rear_i2c_rfinfo();
#endif
		sec_abc_send_event("MODULE=camera@WARN=i2c_fail");
#if defined(CONFIG_CAMERA_CDR_TEST)
		cam_clock_data_recovery_set_result(CDR_ERROR_I2C);
#endif
	}
#endif

	return rc;
}

#if defined(CONFIG_SAMSUNG_CAMERA)
#define INDIRECT_ADDR_INVALID	0xFFFF
#define INDIRECT_ADDR_LSI		0x6F12
int camera_io_get_indirect_address(struct cam_sensor_ctrl_t *s_ctrl)
{
	int sensor_id = s_ctrl->sensordata->slave_info.sensor_id;
	uint32_t indirect_addr = INDIRECT_ADDR_INVALID;

	CAM_DBG(CAM_SENSOR, "sensor id %d", sensor_id);
	switch (sensor_id) {
		case SENSOR_ID_IMX258:
		case SENSOR_ID_IMX374:
		case SENSOR_ID_IMX471:
		case SENSOR_ID_IMX564:
		case SENSOR_ID_IMX754:
		case SENSOR_ID_IMX854:
			break;
		case SENSOR_ID_S5K2LD:
		case SENSOR_ID_S5K3J1:
		case SENSOR_ID_S5K3K1:
		case SENSOR_ID_S5K3LU:
		case SENSOR_ID_S5KGN3:
		case SENSOR_ID_S5KHP2:
			indirect_addr = INDIRECT_ADDR_LSI;
			break;
		default:
			CAM_ERR(CAM_SENSOR,"Invaild Sensor id : %d",sensor_id);
			break;
	}
	return indirect_addr;
}

int camera_io_dev_write_continuous_split(struct i2c_settings_list *i2c_list,
	struct camera_io_master *io_master_info)
{
	int rc = 0,i , k , chunk_num,start_addr;
	struct i2c_settings_list *i2c_list_chunk;
	struct i2c_settings_list chunk;
	struct cam_sensor_ctrl_t *s_ctrl = container_of(io_master_info, struct cam_sensor_ctrl_t, io_master_info);
	uint32_t indirect_addr = camera_io_get_indirect_address(s_ctrl);

	i2c_list_chunk = &chunk;
	memcpy(i2c_list_chunk, i2c_list,sizeof(struct i2c_settings_list));
	chunk_num = (i2c_list->i2c_settings.size + CHUNK_SIZE - 1) / CHUNK_SIZE;
	start_addr = i2c_list->i2c_settings.reg_setting->reg_addr;
	for(i = 0; i < chunk_num; i++) {
		i2c_list_chunk->i2c_settings.reg_setting =
			&i2c_list->i2c_settings.reg_setting[i * CHUNK_SIZE];
		i2c_list_chunk->i2c_settings.size = CHUNK_SIZE;
		if(i == (i2c_list->i2c_settings.size / CHUNK_SIZE))
			i2c_list_chunk->i2c_settings.size = i2c_list->i2c_settings.size % CHUNK_SIZE;

		if((indirect_addr == INDIRECT_ADDR_INVALID) || // Sensor doesn't support indirect mode
			(indirect_addr != start_addr)) { // Use direct mode even if sensor supports indirect mode
			i2c_list_chunk->i2c_settings.reg_setting->reg_addr = start_addr + (CHUNK_SIZE * i);
			for(k = 1; k < i2c_list_chunk->i2c_settings.size; k++)
				i2c_list_chunk->i2c_settings.reg_setting[k].reg_addr = i2c_list_chunk->i2c_settings.reg_setting[0].reg_addr;
		}
		CAM_DBG(CAM_SENSOR, "Split start addr : 0x%04X , data : 0x%04X ,size : %d",
			i2c_list_chunk->i2c_settings.reg_setting->reg_addr,
			i2c_list_chunk->i2c_settings.reg_setting->reg_data,
			i2c_list_chunk->i2c_settings.size);
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list_chunk->i2c_settings),
			CAM_SENSOR_I2C_WRITE_BURST);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to burst write I2C settings: %d",
				rc);
			return rc;
		}

	}
	return rc;
}
#endif