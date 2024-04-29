/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef TISPEAKER_PROT
#define TISPEAKER_PROT

#include "Device.h"
#include "sp_vi.h"
#include "sp_rx.h"
#include <tinyalsa/asoundlib.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include<vector>
#include "apm_api.h"

class Device;

#define TI_SMARTPA

#ifdef ENABLE_TAS_SPK_PROT
#include "SecPalDefs.h"
#ifndef SEC_AUDIO_COMMON
#define PAL_PARAM_ID_SEPAKER_AMP_RUN_CAL 3000
#define PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_RCV 3001
#define PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_SPK 3002
#define PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE_RESET 3003
#define PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE 3004

#define NUM_OF_SPEAKER 2
#define NUM_OF_BIGDATA_ITEM 5 /* max_temp temp_over max_excu excu_over keep_max_temp */

typedef struct pal_param_amp_bigdata {
    int value[NUM_OF_SPEAKER * NUM_OF_BIGDATA_ITEM];
} pal_param_amp_bigdata_t;

enum bigdata_ids{
    MAX_TEMP_L, MAX_TEMP_R,
    TEMP_OVER_L, TEMP_OVER_R,
    MAX_EXCU_L, MAX_EXCU_R,
    EXCU_OVER_L, EXCU_OVER_R,
    KEEP_MAX_TEMP_L, KEEP_MAX_TEMP_R, /*do not reset*/
};

typedef struct pal_param_amp_ssrm {
    int temperature;
} pal_param_amp_ssrm_t;
#endif

typedef struct {
        int Excursion_Max; //2.x
        int Temperature_Max; //No Q
        int Excursion_over_count; //No Q
        int Temprature_over_count; //No Q

        /* additional params */
        int current_excursion; //2.x
        int max_excursion_perDuration; //2.x
        int current_temperature; //No Q
        int max_temperature_perDuration; //No Q
} tismartpa_exc_temp_stats;

typedef enum tisa_calib_data_type {
    INT,
    FLOAT,
    STRING,
    HEX
} tisa_calib_data_type_t;

typedef struct
{
    int num_ch;
    int r0_q19[2];
    int t0[2];
} spCalibData;

class SpeakerProtectionTI : public Device
{
protected :
    bool spkrProtEnable;
    int minIdleTime;
    static bool isSpkrInUse;
    static struct timespec spkrLastTimeUsed;
    static struct mixer *virtMixer;
    static struct mixer *hwMixer;
    static struct pcm *rxPcm;
    static struct pcm *txPcm;
    static int numberOfChannels;

    struct pal_device mDeviceAttr;
    std::vector<int> pcmDevIdTx;
    static int numberOfRequest;
    static struct pal_device_info vi_device;

    static pal_param_amp_bigdata_t bigdata;
    static pal_param_amp_ssrm_t ssrm;

private :

public:
    std::mutex deviceMutex;
    std::mutex calMutex;

    void spkrCalibrationThread();
    void speakerProtectionInit();
    void speakerProtectionDeinit();
    static void spkrProtSetSpkrStatus(bool enable);
    static int setConfig(int type, int tag, int tagValue, int devId, const char *aif);

    SpeakerProtectionTI(struct pal_device *device,
                      std::shared_ptr<ResourceManager> Rm);
    ~SpeakerProtectionTI();

    int32_t start();
    int32_t stop();

    int32_t setParameter(uint32_t param_id, void *param) override;
    int32_t getParameter(uint32_t param_id, void **param) override;

    int32_t spkrProtProcessingMode(bool flag);
    void updateSPcustomPayload(int param_id, void *spConfig);
    void disconnectFeandBe(std::vector<int> pcmDevIds, std::string backEndName);

    int32_t runTICalibration();
    int get_data_from_file(const char* fname, tisa_calib_data_type_t dtype, void* pdata);
    int save_data_to_file(const char* fname, tisa_calib_data_type_t dtype, void *pdata);
    int get_calib_data(spCalibData *ptical);
    void smartpa_get_bigdata();
    void smartpa_get_temp(void *param, int ch_idx);

    int32_t getMixerControl(struct mixer_ctl **ctl, unsigned *miid,
            char *mixer_str);
    void printBigdata(int ch_idx);

};
#endif // ENABLE_TAS_SPK_PROT
#endif //TISPEAKER_PROT
