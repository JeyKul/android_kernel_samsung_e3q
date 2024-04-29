/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TIPAYLOAD_BUILDER_H_
#define TIPAYLOAD_BUILDER_H_


#define TAS_SA_GET_F0          3810
#define TAS_SA_GET_Q           3811
#define TAS_SA_GET_TV          3812
#define TAS_SA_GET_RE          3813
#define TAS_SA_CALIB_INIT      3814
#define TAS_SA_CALIB_DEINIT    3815
#define TAS_SA_SET_RE          3816
#define TAS_SA_F0_TEST_INIT    3817
#define TAS_SA_F0_TEST_DEINIT  3818
#define TAS_SA_SET_PROFILE     3819
#define TAS_SA_GET_STATUS      3821
#define TAS_SA_SET_SPKID       3822
#define TAS_SA_SET_TCAL        3823
#define TAS_SA_EXC_TEMP_STAT   3824

#define TAS_SA_IV_VBAT_FMT     3825

#define TAS_SA_VALID_INIT      3831
#define TAS_SA_VALID_DEINIT    3832
#define TAS_SA_GET_VALID_STATUS 3833
#define TAS_SA_SET_BYPASS_MODE  3834
#define TAS_SA_GET_OP_MODE      3835
#define TAS_SA_SET_INTERFACE_MODE 3836
#define TAS_PCM_CHANNEL_MAPPING 3837
#define TAS_SA_GET_RE_RANGE   3838
#define TAS_SA_DIGITAL_GAIN   3839
/* New Param ID Added to send IV Width and VBat info to Algo Library */
#define TAS_SA_IV_WIDTH_VBAT_MON 3840

#define TAS_SA_LE_FLAG_STATS     3850
#define TAS_SA_SET_SKIN_TEMP    3853
#define TAS_SA_SET_DRV_OP_MODE  3854

#define TAS_SA_SET_CALIB_DATA   3859

#define TAS_CALC_PARAM_IDX(I, LEN, CH)    ((I) | ((LEN) << 16) | ((CH) << 28))
#define TAS_DECODE_LEN_PARAM_IDX(VAL)     (((VAL) >> 16) & 0xFFF)
#define TAS_DECODE_PARAM_ID_FROM_PARAM_IDX(VAL)     ((VAL) & 0xFFFF)

#define TI_PARAM_ID_SP_CALIB_DATA TAS_CALC_PARAM_IDX(TAS_SA_SET_CALIB_DATA, 5, 1)
#define TI_PARAM_ID_SP_BIG_DATA_L TAS_CALC_PARAM_IDX(TAS_SA_EXC_TEMP_STAT, 8, 1)
#define TI_PARAM_ID_SP_BIG_DATA_R TAS_CALC_PARAM_IDX(TAS_SA_EXC_TEMP_STAT, 8, 2)
#define TI_PARAM_ID_SP_GET_RE_RANGE_L TAS_CALC_PARAM_IDX(TAS_SA_GET_RE_RANGE, 2, 1)
#define TI_PARAM_ID_SP_GET_RE_RANGE_R TAS_CALC_PARAM_IDX(TAS_SA_GET_RE_RANGE, 2, 2)
#define TI_PARAM_ID_SP_CALIB_INIT_L TAS_CALC_PARAM_IDX(TAS_SA_CALIB_INIT, 1, 1)
#define TI_PARAM_ID_SP_CALIB_INIT_R TAS_CALC_PARAM_IDX(TAS_SA_CALIB_INIT, 1, 2)
#define TI_PARAM_ID_SP_CALIB_DEINIT_L TAS_CALC_PARAM_IDX(TAS_SA_CALIB_DEINIT, 1, 1)
#define TI_PARAM_ID_SP_CALIB_DEINIT_R TAS_CALC_PARAM_IDX(TAS_SA_CALIB_DEINIT, 1, 2)
#define TI_PARAM_ID_SP_GET_RE_L TAS_CALC_PARAM_IDX(TAS_SA_GET_RE, 1, 1)
#define TI_PARAM_ID_SP_GET_RE_R TAS_CALC_PARAM_IDX(TAS_SA_GET_RE, 1, 2)
#define TI_PARAM_ID_SP_SET_VBAT_MON TAS_CALC_PARAM_IDX(TAS_SA_IV_WIDTH_VBAT_MON, 1, 1)
#define TI_PARAM_ID_SP_SET_DRV_OP_MODE TAS_CALC_PARAM_IDX(TAS_SA_SET_DRV_OP_MODE, 1, 1)
#define TI_PARAM_ID_SP_GET_TV_L TAS_CALC_PARAM_IDX(TAS_SA_GET_TV, 1, 1)
#define TI_PARAM_ID_SP_GET_TV_R TAS_CALC_PARAM_IDX(TAS_SA_GET_TV, 1, 2)

#endif /* TIPAYLOAD_BUILDER_H_ */