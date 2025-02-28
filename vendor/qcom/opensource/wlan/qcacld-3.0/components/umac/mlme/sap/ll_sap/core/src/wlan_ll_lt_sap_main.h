/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: contains ll_sap_definitions specific to the ll_sap module
 */

#ifndef _WLAN_LL_LT_SAP_MAIN_H_
#define _WLAN_LL_LT_SAP_MAIN_H_

#include "wlan_ll_sap_main.h"
#include "wlan_mlme_public_struct.h"
#include <i_qdf_types.h>
#include <qdf_types.h>
#include "wlan_ll_sap_main.h"
#include "wlan_ll_sap_public_structs.h"

/**
 * ll_lt_sap_is_supported() - Check if ll_lt_sap is supported or not
 *
 * Return: True/False
 */
bool ll_lt_sap_is_supported(void);

/**
 * ll_lt_sap_get_sorted_user_config_acs_ch_list() - API to get sorted user
 * configured channel list
 * @psoc: Pointer to psoc object
 * @vdev_id: Vdev Id
 * @ch_info: Pointer to ch_info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ll_lt_sap_get_sorted_user_config_acs_ch_list(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					struct sap_sel_ch_info *ch_info);
/*
 * ll_lt_sap_init() - Initialize ll_lt_sap infrastructure
 * @vdev: Pointer to vdev
 *
 * Return: QDF_STATUS_SUCCESS if ll_lt_sap infra initialized successfully else
 * error code
 */
QDF_STATUS ll_lt_sap_init(struct wlan_objmgr_vdev *vdev);

/**
 * ll_lt_sap_deinit() - De-initialize ll_lt_sap infrastructure
 * @vdev: Pointer to vdev
 *
 * Return: QDF_STATUS_SUCCESS if ll_lt_sap infra de-initialized successfully
 * else error code
 */
QDF_STATUS ll_lt_sap_deinit(struct wlan_objmgr_vdev *vdev);

/**
 * ll_lt_sap_switch_bearer_to_ble() - Switch audio transport to BLE
 * @psoc: Pointer to psoc
 * @bs_request: Pointer to bearer switch request
 * Return: QDF_STATUS_SUCCESS on successful bearer switch else failure
 */
QDF_STATUS
ll_lt_sap_switch_bearer_to_ble(struct wlan_objmgr_psoc *psoc,
			       struct wlan_bearer_switch_request *bs_request);

/**
 * ll_lt_sap_request_for_audio_transport_switch() - Handls audio transport
 * switch request from userspace
 * @req_type: requested transport switch type
 *
 * Return: True/False
 */
QDF_STATUS ll_lt_sap_request_for_audio_transport_switch(
					enum bearer_switch_req_type req_type);
#endif /* _WLAN_LL_SAP_MAIN_H_ */
