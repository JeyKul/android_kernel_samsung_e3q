#define LOG_TAG "PAL: SpeakerProtectionTFA"
#define LOG_TAG2 "PAL: SpeakerFeedbackTFA"

#include "SpeakerProtectionTFA.h"
#include "PalAudioRoute.h"
#include "ResourceManager.h"
#include "SessionAlsaUtils.h"
#include "kvh2xml.h"
#include <agm/agm_api.h>

#include <fstream>
#include <sstream>

#ifdef SEC_AUDIO_COMMON
#include "SecPalDefs.h"
#endif

#ifdef ENABLE_TFA98XX_SUPPORT
struct mixer *SpeakerProtectionTFA::virtMixer;
struct mixer *SpeakerProtectionTFA::hwMixer;

std::mutex SpeakerProtectionTFA::cal_mutex;
std::condition_variable SpeakerProtectionTFA::cal_wait_cond;

#define PAL_SP_TFA_VERSION "Goodix_20230324_1"

//#define SET_IPCID_MIXER_CTL
//#define CHECK_STREAM_TYPE

#define TFA2_FW_X_DATA_UM_SCALE (262)
#define TFA2_FW_T_DATA_SCALE (16384)
#define SSRM_DEFAULT_SPK_TEMP (0x7FFFFFFF)
#define TFADSP_DEFAULT_CAL_TEMP (25)
#define TFADSP_DEBUG_MIN_CODE (10000)
#define TFADSP_DEBUG_MAGIC_CODE (12345)

#define PARAM_ID_TFADSP_SET_COMMAND 0x1000B921
#define PARAM_ID_TFADSP_GET_RESULT  0x1000B922
#define PARAM_ID_TFADSP_SET_RE25C   0x1000B926
#define PARAM_ID_TFADSP_CALIBRATE   0x1000B924
#define PARAM_ID_TFADSP_DEBUG_CONTROL 0x1000B928
#define PARAM_ID_TFADSP_GET_STATE 0x1000B929

#define GET_LIB_VERSION         0x000080FD    /* TFADSP library version */
#define GET_DATA_LOGGER         0x0000818D  /* Get Blackbox data */
#define GET_STATUS_CHANGE       0x0000808D  /* Get calibration result */
#define GET_RE25C_CMD           0x00008185
#define SET_RE25C_CMD           0x00008105
#define CUSTOM_PARAM_GET_TSPKR  0x000081A8  /* SB.GetTSpkr */
#define CUSTOM_PARAM_SET_TSURF  0x00008F01

#define PAL_ALIGN_8BYTE(x)    (((x) + 7) & (~7))

#define TFADSP_FLAG_CALIBRATION_DONE (1 << 0)
#define TFADSP_FLAG_DAMAGED_SPEAKER_P (1 << 1)
#define TFADSP_FLAG_DAMAGED_SPEAKER_S (1 << 2)
#define TFADSP_FLAG_DAMAGED_VSENSE_P (1 << 3)
#define TFADSP_FLAG_DAMAGED_VSENSE_S (1 << 4)
#define TFADSP_FLAG_DAMAGED_FRES (1 << 5)

#define SSRM_INTERVAL_MS (10000)
#define SYSFS_NODE_POWER_STATE "/sys/class/tfa/tfa_stc/power_state"
#define SYSFS_NODE_POWER_STATE_R "/sys/class/tfa/tfa_stc/power_state_r"
#define SYSFS_NODE_REF_TEMP "/sys/class/tfa/tfa_cal/ref_temp"
#define SYSFS_NODE_CAL_CONFIG "/sys/class/tfa/tfa_cal/config"

#define CAL_STATUS_CHECK_INTERVAL_MS 100
#define CAL_STATUS_CHECK_TIMEOUT_COUNT 20 // 100ms * 20 = 2 sec

#define DUMMY_CAL_VALUE 6400 // 6400 mOhm
#define DUMMY_CAL_VALUE_R 5800 // 5800 mOhm

#define TFA_SND_CARD_VIRTUAL 100

#define TFA_CAL_STATE_EFS_FILE "/efs/tfa_cal/status"
#define TFA_CAL_RDC_EFS_FILE "/efs/tfa_cal/rdc"
#define TFA_CAL_RDC_R_EFS_FILE "/efs/tfa_cal/rdc_r"
#define TFA_CAL_TEMP_EFS_FILE "/efs/tfa_cal/temp"
#define TFA_CAL_TEMP_R_EFS_FILE "/efs/tfa_cal/temp_r"

#define TFA_SPK_IPC_MIXER_NAME "SPK IPC_ID"

#define TFA_SPK_LEFT    0
#define TFA_SPK_RIGHT   1

std::shared_ptr<Device> SpeakerFeedbackTFA::obj = nullptr;

int32_t SpeakerProtectionTFA::tfa_dsp_get_miid(const char *controlName, mixer_ctl **mixer_ctrl)
{
    int32_t ret = 0;
    std::shared_ptr<Device> dev = nullptr;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activeStreams;
    mixer_ctl *ctrl;
    std::string backEndName;
    int device;

    PAL_DBG(LOG_TAG, "Begin");

    dev = Device::getObject(PAL_DEVICE_OUT_SPEAKER);
    if (NULL == dev) {
        PAL_ERR(LOG_TAG, "dev is NULL");
        ret = -1;
        goto exit;
    }

    ret = rm->getActiveStream_l(activeStreams, dev);
    if ((0 != ret) || (activeStreams.size() == 0)) {
        PAL_ERR(LOG_TAG, "no active stream available");
        goto exit;
    }

    stream = static_cast<Stream *>(activeStreams[0]);
    if (NULL == stream) {
        PAL_ERR(LOG_TAG, "stream is NULL");
        ret = -1;
        goto exit;
    }

    stream->getAssociatedSession(&session);
    if (NULL == session) {
        PAL_ERR(LOG_TAG, "session is NULL");
        ret = -1;
        goto exit;
    }

    ret = rm->getBackendName(PAL_DEVICE_OUT_SPEAKER, backEndName);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain the rx snd device name");
        goto exit;
    }

    PAL_DBG(LOG_TAG, "backEndName.c_str() = %s", backEndName.c_str());
    if (tfadsp_rx_miid == 0) {
        ret = session->getMIID(backEndName.c_str(), MODULE_SP, &tfadsp_rx_miid);
        if (ret) {
            PAL_ERR(LOG_TAG, "Failed to get tag info, ret = %d", ret);
            tfadsp_rx_miid = 0;
            goto exit;
        }
    }
    PAL_DBG(LOG_TAG, "tfadsp_rx_miid = 0x%x", tfadsp_rx_miid);

    ctrl = session->getFEMixerCtl(controlName, &device, PAL_AUDIO_OUTPUT);
    if (NULL == ctrl) {
        PAL_ERR(LOG_TAG, "Failed to get session control");
        ret = -1;
        goto exit;
    }

    *mixer_ctrl = ctrl;

exit:
    PAL_DBG(LOG_TAG, "End - return %d", ret);
    return ret;
}


int32_t SpeakerProtectionTFA::tfa_dsp_msg_write(uint32_t param_id, const uint8_t *buffer, int len)
{
    const char *controlName = "setParam";
    apm_module_param_data_t *header = NULL;
    struct mixer_ctl *ctrl = NULL;
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "msg len %d", len);
    status = tfa_dsp_get_miid(controlName, &ctrl);

    if (ctrl != NULL && buffer != NULL && len > 0 && status == 0) {
        char *payload;
        int pkg_size = PAL_ALIGN_8BYTE(len + sizeof(apm_module_param_data_t));
        PAL_DBG(LOG_TAG, "pkg_size = %d", pkg_size);

        payload = (char *)calloc(1, pkg_size);
        if (payload) {
            header = (apm_module_param_data_t *)payload;
            header->module_instance_id = tfadsp_rx_miid;
            header->param_id = param_id;
            header->param_size = len;
            header->error_code = 0;
            memcpy(payload + sizeof(apm_module_param_data_t), buffer, len);

            status = mixer_ctl_set_array(ctrl, payload, pkg_size);
            if (status)
                PAL_ERR(LOG_TAG, "mixer_ctl_set_array error %d", status);
            usleep(5000);
            free(payload);
        }
    }
    PAL_DBG(LOG_TAG, "End - status %d", status);
    return status;
}

int32_t SpeakerProtectionTFA::tfa_dsp_msg_read(uint8_t *buffer, int len)
{
    const char *controlName = "getParam";
    apm_module_param_data_t *header = NULL;
    struct mixer_ctl *ctrl = NULL;
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "buffer len %d", len);
    status = tfa_dsp_get_miid(controlName, &ctrl);

    if (ctrl != NULL && buffer != NULL && len > 0 && status == 0) {
        char *payload;
        int pkg_size = PAL_ALIGN_8BYTE(len + sizeof(apm_module_param_data_t));

        payload = (char *)calloc(1, pkg_size);
        if (payload) {
            header = (apm_module_param_data_t *)payload;
            header->module_instance_id = tfadsp_rx_miid;
            header->param_id = PARAM_ID_TFADSP_GET_RESULT;
            header->param_size = len;
            header->error_code = 0;

            status = mixer_ctl_set_array(ctrl, payload, pkg_size);
            if (status)
                PAL_ERR(LOG_TAG, "mixer_ctl_set_array error %d", status);

            status = mixer_ctl_get_array(ctrl, payload, pkg_size);
            if (status)
                PAL_ERR(LOG_TAG, "mixer_ctl_get_array error %d", status);

            memcpy(buffer, payload + sizeof(apm_module_param_data_t), len);
            PAL_DBG(LOG_TAG, "read buffer : 0x%x, 0x%x, 0x%x, 0x%x", buffer[0], buffer[1], buffer[2], buffer[3]);
            free(payload);
        }
    }
    PAL_DBG(LOG_TAG, "End - status %d", status);
    return status;
}

int32_t SpeakerProtectionTFA::tfa_dsp_read_state(uint8_t *buffer, int len)
{
    const char *controlName = "getParam";
    apm_module_param_data_t *header = NULL;
    struct mixer_ctl *ctrl = NULL;
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "buffer len %d", len);
    status = tfa_dsp_get_miid(controlName, &ctrl);

    if (ctrl != NULL && buffer != NULL && len > 0 && status == 0) {
        char *payload;
        int pkg_size = PAL_ALIGN_8BYTE(len + sizeof(apm_module_param_data_t));

        payload = (char *)calloc(1, pkg_size);
        if (payload) {
            header = (apm_module_param_data_t *)payload;
            header->module_instance_id = tfadsp_rx_miid;
            header->param_id = PARAM_ID_TFADSP_GET_STATE;
            header->param_size = len;
            header->error_code = 0;

            status = mixer_ctl_set_array(ctrl, payload, pkg_size);
            if (status)
                PAL_ERR(LOG_TAG, "mixer_ctl_set_array error %d", status);

            status = mixer_ctl_get_array(ctrl, payload, pkg_size);
            if (status)
                PAL_ERR(LOG_TAG, "mixer_ctl_get_array error %d", status);

            memcpy(buffer, payload + sizeof(apm_module_param_data_t), len);
            PAL_DBG(LOG_TAG, "read buffer : 0x%x, 0x%x, 0x%x, 0x%x", buffer[0], buffer[1], buffer[2], buffer[3]);
            free(payload);
        }
    }
    PAL_DBG(LOG_TAG, "End - status %d", status);
    return status;
}

int32_t SpeakerProtectionTFA::tfa_dsp_read_lib_version()
{
    int32_t ret = 0;
    uint8_t cmd_buf[64] = {0, }; // 4 * 16(char)
    char str_ver[17] = {0, };
    uint32_t cmd = GET_LIB_VERSION;

    PAL_DBG(LOG_TAG, "Begin");

    memcpy(&cmd_buf[0], (uint8_t *)&cmd, 4);

    ret = tfa_dsp_msg_write(PARAM_ID_TFADSP_SET_COMMAND, (const uint8_t *)cmd_buf, 4);
    if (ret == 0) {
        ret = tfa_dsp_msg_read(cmd_buf, sizeof(cmd_buf));
        for (int i=0, j=0; i < sizeof(cmd_buf); i+=4, j++) {
            str_ver[j] = cmd_buf[i];
        }
        PAL_DBG(LOG_TAG, "library version : %s", str_ver);
    } else {
        PAL_ERR(LOG_TAG,"tfa_dsp_msg_write failed %d", ret);
    }

    return ret;
}

SpeakerProtectionTFA::SpeakerProtectionTFA(struct pal_device *device,
                        std::shared_ptr<ResourceManager> Rm):Device(device, Rm)
{
    int status = 0;
    rm = Rm;
    struct pal_device_info devinfo = {};

    active_state = false;
    exit_ssrm_thread = false;
    ssrm_timer_cnt = 0;

    PAL_DBG(LOG_TAG, "SpeakerProtectionTFA Constructor");

    memset(&mDeviceAttr, 0, sizeof(struct pal_device));
    memcpy(&mDeviceAttr, device, sizeof(struct pal_device));
    PAL_DBG(LOG_TAG, "Device Id %d", device->id);

    rm->getDeviceInfo(PAL_DEVICE_OUT_SPEAKER, PAL_STREAM_PROXY, "", &devinfo);
    PAL_DBG(LOG_TAG, "Number of Channels %d", devinfo.channels);

    status = rm->getVirtualAudioMixer(&virtMixer);
    if (status) {
        PAL_ERR(LOG_TAG,"virt mixer error %d", status);
    }
    status = rm->getHwAudioMixer(&hwMixer);
    if (status) {
        PAL_ERR(LOG_TAG,"hw mixer error %d", status);
    }

    ipc_id_mctl = mixer_get_ctl_by_name(hwMixer, TFA_SPK_IPC_MIXER_NAME);
    if (!ipc_id_mctl) {
        PAL_ERR(LOG_TAG,"ipc mixer_get_ctl_by_name failed");
    }

    load_cal_file();
    cal_temp = get_cal_temp();
#ifdef LAUNCH_SSRM_THREAD
    ssrm_thread = std::thread(SSRM_Thread, std::ref(*this));
#endif
    surft_left = SSRM_DEFAULT_SPK_TEMP;
    surft_right = SSRM_DEFAULT_SPK_TEMP;
    tfadsp_rx_miid = 0;

#if defined(SEC_AUDIO_DUAL_SPEAKER) || defined(SEC_AUDIO_SCREEN_MIRRORING) // { SUPPORT_VOIP_VIA_SMART_VIEW
    curMuteStatus = PAL_DEVICE_SPEAKER_UNMUTE;
#endif // } SUPPORT_VOIP_VIA_SMART_VIEW
}

SpeakerProtectionTFA::~SpeakerProtectionTFA()
{
    PAL_DBG(LOG_TAG, "SpeakerProtectionTFA Destructor");

#ifdef LAUNCH_SSRM_THREAD
    exit_ssrm_thread = true;
    ssrm_wait_cond.notify_one();

    if (ssrm_thread.joinable()) {
        PAL_DBG(LOG_TAG, "Join SSRM thread");
        ssrm_thread.join();
    }
    PAL_DBG(LOG_TAG, "End");
#endif
}

void SpeakerProtectionTFA::SSRM_Thread(SpeakerProtectionTFA& inst) {
    std::unique_lock<std::mutex> lck(inst.ssrm_mutex);
    PAL_DBG(LOG_TAG, "Begin");

    while (!inst.exit_ssrm_thread) {
        inst.ssrm_wait_cond.wait_for(lck, std::chrono::milliseconds(SSRM_INTERVAL_MS));
        if (!inst.exit_ssrm_thread) {
            PAL_DBG(LOG_TAG, "active_state %d, ssrm_timer_cnt %d", inst.active_state, inst.ssrm_timer_cnt);
            if (inst.cal_status == 0) {
                // inst.get_spkt_data(); // write tfa.spkt file. NOT READY
                inst.ssrm_timer_cnt++;
            }
        }
    }
    PAL_DBG(LOG_TAG, "End");
}

int32_t SpeakerProtectionTFA::setParameter(uint32_t param_id, void *param)
{
    int32_t ret = 0;

    PAL_DBG(LOG_TAG, "Set parameters param_id %d", param_id);

#if defined(SEC_AUDIO_DUAL_SPEAKER) || defined(SEC_AUDIO_SCREEN_MIRRORING) // { SUPPORT_VOIP_VIA_SMART_VIEW
    struct audio_route *audioRoute = NULL;
    if (!rm) {
        PAL_ERR(LOG_TAG, "Failed to get the resourcemanager %d", ret);
        return ret;
    }
    ret = rm->getAudioRoute(&audioRoute);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to get the audio_route address status %d", ret);
        return ret;
    }

    if (param_id == PAL_PARAM_ID_SPEAKER_STATUS) {
        pal_param_speaker_status_t* param_speaker_status = (pal_param_speaker_status_t *)param;
        if (curMuteStatus == PAL_DEVICE_SPEAKER_MUTE) { // current : full mute
            if (param_speaker_status->mute_status == PAL_DEVICE_SPEAKER_UNMUTE) {
                disableDevice(audioRoute, "amp-mute");
                curMuteStatus = PAL_DEVICE_SPEAKER_UNMUTE; // full mute -> full unmute
            }
        } else { // current : full unmute, upper mute, upper unmute
            if (param_speaker_status->mute_status == PAL_DEVICE_SPEAKER_MUTE) {
                if (curMuteStatus == PAL_DEVICE_UPPER_SPEAKER_MUTE) {
                    disableDevice(audioRoute, "upper-spk-amp-mute");
                }
                enableDevice(audioRoute, "amp-mute");
                curMuteStatus = PAL_DEVICE_SPEAKER_MUTE; // upper mute or full unmute -> full mute
            } else if (param_speaker_status->mute_status == PAL_DEVICE_UPPER_SPEAKER_MUTE) {
                enableDevice(audioRoute, "upper-spk-amp-mute");
                curMuteStatus = PAL_DEVICE_UPPER_SPEAKER_MUTE; // full unmute -> upper mute
            } else if (param_speaker_status->mute_status == PAL_DEVICE_UPPER_SPEAKER_UNMUTE) {
                disableDevice(audioRoute, "upper-spk-amp-mute");
                curMuteStatus = PAL_DEVICE_SPEAKER_UNMUTE; // upper mute -> full unmute
            }
        }
    }
#endif // } SUPPORT_VOIP_VIA_SMART_VIEW

    switch (param_id) {
        case PAL_PARAM_ID_SEPAKER_AMP_RUN_CAL: // 3000
            PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SEPAKER_AMP_RUN_CAL");
            if (param) {
                pal_param_cal_trigger_t* param_cal_trigger = (pal_param_cal_trigger_t *)param;
                PAL_DBG(LOG_TAG, "param_cal_trigger enable %d", param_cal_trigger->enable);
            }
            ret = run_calibration();
            break;
        case PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_RCV:
            PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_RCV");
            if (param) {
                pal_param_amp_ssrm_t* param_amp_temp = (pal_param_amp_ssrm_t *)param;
                surft_left = param_amp_temp->temperature;
                PAL_DBG(LOG_TAG, "param_amp_temp(left) temperature %d", surft_left);
                set_sknt_control();
            }
            break;
        case PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_SPK:
            PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_SPK");
            if (param) {
                pal_param_amp_ssrm_t* param_amp_temp = (pal_param_amp_ssrm_t *)param;
                surft_right = param_amp_temp->temperature;
                PAL_DBG(LOG_TAG, "param_amp_temp(right) temperature %d", surft_right);
                set_sknt_control();
            }
            break;
        case PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_START:
            PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_START");
            //activate_tfadsp();
            break;
        case PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_STOP:
            PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_STOP");
            if (bigdata_read == false && active_spk_dev_count == 1) {
                bigdata_read = true;
                store_blackbox_data();
            }
            break;
        default:
            PAL_DBG(LOG_TAG, "Unknown param_id %d", param_id);
    }

    return ret;
}

int32_t SpeakerProtectionTFA::getParameter(uint32_t param_id, void **param)
{
    (void ) param;
    PAL_DBG(LOG_TAG, "Get parameters param_id=0x%x", param_id);

    switch (param_id) {
        case PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_RCV:
            PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_RCV");
            get_spkt_data();
            amp_ssrm.temperature = rcv_temp;
            *param = (void *)&amp_ssrm;
            PAL_DBG(LOG_TAG, "TEMPERATURE_RCV return %d", amp_ssrm.temperature);
            break;
        case PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_SPK:
            PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_SPK");
            get_spkt_data();
            amp_ssrm.temperature = spk_temp;
            *param = (void *)&amp_ssrm;
            PAL_DBG(LOG_TAG, "TEMPERATURE_SPK return %d", amp_ssrm.temperature);
            break;
        case PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE_RESET:
        case PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE:
            PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE");
            tfa_bigdata_param = tfa_bigdata;
            *param = (void *)&tfa_bigdata_param;
            print_bigdata();
            if (param_id == PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE_RESET) {
                reset_blackbox_data();
            }
            break;
        default:
            PAL_DBG(LOG_TAG, "Unknown param_id 0x%x", param_id);
    }

    return 0;
}

// dev=0 : Left, dev=1 : Right
int32_t SpeakerProtectionTFA::read_tfa_power_state(uint32_t dev)
{
    int32_t ret = 0;
    FILE *fd = NULL;
    char read_buf[10] = {0, };

    PAL_DBG(LOG_TAG, "dev %d", dev);
    if (dev == TFA_SPK_LEFT)
        fd = fopen(SYSFS_NODE_POWER_STATE, "r");
    else
        fd = fopen(SYSFS_NODE_POWER_STATE_R, "r");

    if (fd) {
        if (fread(read_buf, 1, 9, fd) < 0) {
            PAL_ERR(LOG_TAG, "fread error");
            ret = -EINVAL;
            goto exit;
        }
        PAL_DBG(LOG_TAG, "read %s", read_buf);
        ret = atoi(read_buf);
    } else {
        PAL_ERR(LOG_TAG, "fopen error");
    }
exit:
    if (fd)
        fclose(fd);
    return ret;
}

int SpeakerProtectionTFA::start()
{
    int32_t ret = 0;

    PAL_DBG(LOG_TAG, "Device start [%s]", PAL_SP_TFA_VERSION);

    ssrm_timer_cnt = 0;
    cal_status = 0;

#ifdef CHECK_STREAM_TYPE
    //std::string backEndName;
    Stream *stream = NULL;
    std::shared_ptr<Device> dev = nullptr;
    std::vector<Stream*> activestreams;
    pal_stream_type_t streamType;

    //rm->getBackendName(deviceAttr.id, backEndName);
    //dev = Device::getInstance(&deviceAttr, rm);
    dev = Device::getObject(PAL_DEVICE_OUT_SPEAKER);
    ret = rm->getActiveStream_l(activestreams, dev);
    if ((0 != ret) || (activestreams.size() == 0)) {
        PAL_ERR(LOG_TAG, "no active stream available");
    } else {
        stream = static_cast<Stream *>(activestreams[0]);
        stream->getStreamType(&streamType);
        PAL_DBG(LOG_TAG, "streamType : %d", streamType);
    }
#endif // CHECK_STREAM_TYPE

    Device::start();

    if (active_spk_dev_count == 0) {
        active_state = true;
    #ifdef TFADSP_ACTIVATION_THREAD
        if (activate_tfadsp_thread_running == false) {
            std::thread TfadspActivateThread(&SpeakerProtectionTFA::Activate_Tfadsp_Thread, this);
            TfadspActivateThread.detach();
        } else {
            PAL_ERR(LOG_TAG, "activate_tfadsp_thread is running");
        }
    #else
        activate_tfadsp();
    #endif
        bigdata_read = false;
    }
    PAL_DBG(LOG_TAG, "active_spk_dev_count %d --> %d", active_spk_dev_count, active_spk_dev_count+1);
    active_spk_dev_count++;

#ifdef SET_IPCID_MIXER_CTL
    if (ipc_id_mctl) {
        uint32_t miid = 0;
        uint32_t pcm_id = 0;
        uint32_t pcm_id_snd_card = 0;

        get_miid_pcmid_sc(&miid, &pcm_id);
        // id : 0, 2byte PCM ID, 2byte Mixer Card
        pcm_id_snd_card = (pcm_id<<16) + TFA_SND_CARD_VIRTUAL; // 117,100
        PAL_DBG(LOG_TAG, "pcm_id_snd_card : 0x%x", pcm_id_snd_card);
        ret = mixer_ctl_set_value(ipc_id_mctl, 0, pcm_id_snd_card);
        if (ret)
            PAL_ERR(LOG_TAG, "mixer_ctl_set_value[0] error %d", ret);
        // id : 1, 4byte ACDB Module Instance ID
        ret = mixer_ctl_set_value(ipc_id_mctl, 1, miid);
        if (ret)
            PAL_ERR(LOG_TAG, "mixer_ctl_set_value[1] error %d", ret);
    }
    else {
        PAL_ERR(LOG_TAG, "ipc_id_mctl NULL");
    }
#endif // SET_IPCID_MIXER_CTL

    return ret;
}

int SpeakerProtectionTFA::stop()
{
    PAL_DBG(LOG_TAG, "Device stop, active_spk_dev_count %d --> %d", active_spk_dev_count, active_spk_dev_count-1);

    active_spk_dev_count--;
    if (active_spk_dev_count == 0) {
        if (bigdata_read == false) {
            bigdata_read = true;
            store_blackbox_data();
        }
        activate_tfadsp_thread_running = false;
        active_state = false;
    }
    Device::stop();

    return 0;
}

/*
 *Opening audio session: Activate TFADSP
 *Assume TFADSP is already running ? ready to get message.
 *Use dummy calibration data if it fails (hard-coded).
 *Activate TFADSP to configure the algorithm for audio processing.
 *Return activation success / failure.
*/
int32_t SpeakerProtectionTFA::activate_tfadsp()
{
    // read cal value from the cal file and send it to TFADSP
    int32_t ret = 0;
    uint8_t cmd_buf[12] = {0, };
    uint32_t cmd = SET_RE25C_CMD;
    uint32_t cal_p = 0;
    uint32_t cal_s = 0;
    uint32_t tfadsp_state = 0;

    PAL_DBG(LOG_TAG, "Begin");

#ifdef TFADSP_ACTIVATION_THREAD
    for (int i = 0; i < 10; i++) { // max 50ms polling
        if (activate_tfadsp_thread_running == false)
            break;
        PAL_DBG(LOG_TAG, "tfa_dsp_read_state polling %d", i+1);
        ret = tfa_dsp_read_state(cmd_buf, 4);
        tfadsp_state = *((uint32_t *)cmd_buf);
        if (ret == 0 && tfadsp_state != 0) {
            PAL_DBG(LOG_TAG, "tfa_dsp_read_state success, tfadsp_state %d", tfadsp_state);
            break;
        }
        if (activate_tfadsp_thread_running == false)
            break;
        usleep(1000); // 1ms
    }
#else
    ret = tfa_dsp_read_state(cmd_buf, 4);
    tfadsp_state = *((uint32_t *)cmd_buf);
    PAL_DBG(LOG_TAG, "tfadsp_state %d", tfadsp_state);
#endif

    if (ret != 0 || tfadsp_state == 0) {
        PAL_ERR(LOG_TAG,"tfadsp is not in the active state");
        return ret;
    }

    get_rdc(&cal_val_p, &cal_val_s);
    cal_p = (cal_val_p*65536)/1000;
    cal_s = (cal_val_s*65536)/1000;

    PAL_DBG(LOG_TAG, "cal_val_p %d, 0x%x, cal_val_s %d, 0x%x", cal_val_p, cal_p, cal_val_s, cal_s);

    memcpy(&cmd_buf[0], (uint8_t *)&cmd, 4);
    memcpy(&cmd_buf[4], (uint8_t *)&cal_p, 4);
    memcpy(&cmd_buf[8], (uint8_t *)&cal_s, 4);

    ret = tfa_dsp_msg_write(PARAM_ID_TFADSP_SET_RE25C, (const uint8_t *)cmd_buf, sizeof(cmd_buf));

    return ret;
}

/*
 *Closing audio session: Store blackbox
 *Send a message (GetDataLogger; 4 bytes) with: 0x0000818D
 *Read buffer (32 bytes) to get: channel (2) * 4 * data (4-byte) (for blackbox)
 *The read blackbox data is stored in global variable buffer.
 *Return success / failure in reading blackbox.
*/
int32_t SpeakerProtectionTFA::store_blackbox_data()
{
    int32_t ret = 0;
    uint8_t cmd_buf[32] = {0, };
    uint8_t zero_buf[32] = {0, };
    uint32_t *res_buf;
    uint32_t cmd = GET_DATA_LOGGER;
    int tmp = 0;

    PAL_DBG(LOG_TAG, "Begin");

    memcpy(&cmd_buf[0], (uint8_t *)&cmd, 4);

    ret = tfa_dsp_msg_write(PARAM_ID_TFADSP_SET_COMMAND, (const uint8_t *)cmd_buf, 4);
    if (ret == 0) {
        memset(&cmd_buf[0], 0, 4);
        ret = tfa_dsp_msg_read(cmd_buf, sizeof(cmd_buf));
    }

/*
 - structure of bigdata in SS
    int value[NUM_OF_SPEAKER*NUM_OF_BIGDATA_ITEM] = {
        max_temperature_L, max_temperature_R,
        temperature_over_cnt_L, temperature_over_cnt_R,
        max_excursion_L, max_excursion_R,
        excursion_over_cnt_L, excursion_over_cnt_R,
        spekaer_cal_temperature_L, spekaer_cal_temperature_R,
        spekaer_cal_rdc_L, spekaer_cal_rdc_R,
        keep_max_temperature_L, keep_max_temperature_R,
    };
 - structure of bigdata in Goodix
    int res_buf[NUM_OF_SPEAKER*(NUM_OF_BIGDATA_ITEM - 1)] = {
        max_excursion_L, max_temperature_L,
        excursion_over_cnt_L, temperature_over_cnt_L,
        spekaer_cal_temperature_L, spekaer_cal_rdc_L,
        max_excursion_R, max_temperature_R,
        excursion_over_cnt_R, temperature_over_cnt_R,
        spekaer_cal_temperature_R, spekaer_cal_rdc_R
    };
*/
    if (ret == 0 && (memcmp(cmd_buf, zero_buf, sizeof(zero_buf)) != 0)) {
        res_buf = (uint32_t *)cmd_buf;
        PAL_DBG(LOG_TAG, "Bigdata raw data1 0x%x, 0x%x 0x%x 0x%x", res_buf[0], res_buf[1], res_buf[2], res_buf[3]);
        PAL_DBG(LOG_TAG, "Bigdata raw data2 0x%x, 0x%x 0x%x 0x%x", res_buf[4], res_buf[5], res_buf[6], res_buf[7]);

        //max_temperature
        tmp = (int)(res_buf[1]/TFA2_FW_T_DATA_SCALE);
        if (tmp > tfa_bigdata.value[MAX_TEMP_L])
            tfa_bigdata.value[MAX_TEMP_L] = tmp;
        tmp = (int)(res_buf[5]/TFA2_FW_T_DATA_SCALE);
        if (tmp > tfa_bigdata.value[MAX_TEMP_R])
            tfa_bigdata.value[MAX_TEMP_R] = tmp;

        //temperature_over_cnt
        tfa_bigdata.value[TEMP_OVER_L] += (int)res_buf[3];
        tfa_bigdata.value[TMEP_OVER_R] += (int)res_buf[7];

        //max_excursion
        tmp = (int)(res_buf[0]/TFA2_FW_X_DATA_UM_SCALE);
        if (tmp > tfa_bigdata.value[MAX_EXCU_L])
            tfa_bigdata.value[MAX_EXCU_L] = tmp;
        tmp = (int)(res_buf[4]/TFA2_FW_X_DATA_UM_SCALE);
        if (tmp > tfa_bigdata.value[MAX_EXCU_R])
            tfa_bigdata.value[MAX_EXCU_R] = tmp;

        //excursion_over_cnt
        tfa_bigdata.value[EXCU_OVER_L] += (int)res_buf[2];
        tfa_bigdata.value[EXCU_OVER_R] += (int)res_buf[6];

        //spekaer_cal_temperature (board temperature)
        tfa_bigdata.value[SPK_CAL_TEMP_L] = (int)cal_temp;
        tfa_bigdata.value[SPK_CAL_TEMP_R] = (int)cal_temp;

        //spekaer_cal_rdc
        tfa_bigdata.value[SPK_CAL_RDC_L] = (int)cal_val_p;
        tfa_bigdata.value[SPK_CAL_RDC_R] = (int)cal_val_s;

        //keep_max_temperature
        if (tfa_bigdata.value[MAX_TEMP_L] > tfa_bigdata.value[KEEP_MAX_TEMP_L])
            tfa_bigdata.value[KEEP_MAX_TEMP_L] = tfa_bigdata.value[MAX_TEMP_L];

        if (tfa_bigdata.value[MAX_TEMP_R] > tfa_bigdata.value[KEEP_MAX_TEMP_R])
            tfa_bigdata.value[KEEP_MAX_TEMP_R] = tfa_bigdata.value[MAX_TEMP_R];

        print_bigdata();
    } else {
        PAL_ERR(LOG_TAG,"Bigdata read failed %d", ret);
    }

    return ret;
}

void SpeakerProtectionTFA::print_bigdata()
{
    PAL_DBG(LOG_TAG, "max_temperature_L %d, max_temperature_R %d, temperature_over_cnt_L %d, temperature_over_cnt_R %d",
        tfa_bigdata.value[MAX_TEMP_L] , tfa_bigdata.value[MAX_TEMP_R],
        tfa_bigdata.value[TEMP_OVER_L], tfa_bigdata.value[TMEP_OVER_R]);
    PAL_DBG(LOG_TAG, "max_excursion_L %d, max_excursion_R %d, excursion_over_cnt_L %d, excursion_over_cnt_R %d",
        tfa_bigdata.value[MAX_EXCU_L] , tfa_bigdata.value[MAX_EXCU_R],
        tfa_bigdata.value[EXCU_OVER_L], tfa_bigdata.value[EXCU_OVER_R]);
    PAL_DBG(LOG_TAG, "spekaer_cal_temperature_L %d, spekaer_cal_temperature_R %d, spekaer_cal_rdc_L %d, spekaer_cal_rdc_R %d",
        tfa_bigdata.value[SPK_CAL_TEMP_L] , tfa_bigdata.value[SPK_CAL_TEMP_R],
        tfa_bigdata.value[SPK_CAL_RDC_L], tfa_bigdata.value[SPK_CAL_RDC_R]);
}

/*
 *Closing session: Write default TSpkr (related to Surface temperature control)
 *Set DEFAULT_REF_TEMP (25 degC, hard-coded) at powering down.
 *Write TSpkr data (SPK_TEMP_PRIM, SPK_TEMP_SCND) to TSpkrData file.
 *Return success / failure in resetting speaker temperature with the default value.
*/
int32_t SpeakerProtectionTFA::set_default_spkt_data(uint32_t channel)
{
    PAL_DBG(LOG_TAG, "Begin - channel %d", channel);

    return 0;
}

/*
 *Reset bigdata after reading
 *Return success / failure in returning bigdata.
*/
int32_t SpeakerProtectionTFA::reset_blackbox_data()
{
    int32_t ret = 0;

    PAL_DBG(LOG_TAG, "Begin");

    // reset bigdata data (exclude cal_temperature, cal_rdc, keep_max_temperature)
    memset((void *)tfa_bigdata.value, 0, NUM_OF_SPEAKER * (NUM_OF_BIGDATA_ITEM-3) * sizeof(tfa_bigdata.value[MAX_TEMP_L]));

    return ret;
}

/*
 *Assume TFADSP / TFA98xx device are already running in playback ? ready to get message.
 *Set cal_status to keep status active (write if necessary).
 *Remove CalData file or write information to indicate that calibration is not done ever.
 *Send a custom message (e.g. DoCalibration) to enable TFADSP to load the predefined the configuration for calibration.
 *Write sysfs node (1 > /sys/class/tfa/tfa_cal/config) to configure TFA98xx devices for calibration; stop / restart with calibration profile in kernel.
 *Read sysfs node (/sys/class/tfa/tfa_cal/ref_temp) to get board temperature for calibration.
 *Wait 10 msec first. Loop at period of 10 msec (timeout in 200 msec); in a separate thread or not?
 *Read sysfs node (/sys/class/tfa/tfa_cal/config) to check if TFA98xx devices are configured with register setting for calibration.
 *Activate TFADSP to configure the algorithm for calibration.
 *Return calibration success / failure.
*/
int32_t SpeakerProtectionTFA::run_calibration()
{
    int32_t ret = 0;
    FILE *fd = NULL;

    if (active_state == false) { // spk. audio session is not active
        PAL_ERR(LOG_TAG, "active_state false");
        return 1;
    }
    if (cal_status == 1) { // calibration is on-going
        PAL_ERR(LOG_TAG, "cal_status is ON");
        return 2;
    }

    fd = fopen(TFA_CAL_STATE_EFS_FILE, "w");
    if (fd) {
        PAL_DBG(LOG_TAG, "write Enabled in %s", TFA_CAL_STATE_EFS_FILE);
        // Enabled : Cal. fail or Not calibrated yet
        fwrite("Enabled", 1, strlen("Enabled"), fd);
        fclose(fd);
    } else {
        PAL_ERR(LOG_TAG, "status fopen error");
    }

    std::thread tfaCalThread(&SpeakerProtectionTFA::Calibration_Thread, this);
    tfaCalThread.detach();

    PAL_DBG(LOG_TAG, "End");

    return ret;
}

/*
 *Calibration: Check whether calibration is running: *Triggered by Audio HAL from Calibration application, periodically (100 msec)
 *Send a message (GetStatusChange; 4 bytes) with: 0x0000808d
 *Read buffer (8 bytes) to get: 2 data (4-byte; event=data[0], status=data[1])
 *Return cal_status (1) if status.b0 is zero (calibration is still running).
 *Write sysfs node (0 > /sys/class/tfa/tfa_cal/config) to reset status.
 *Reset cal_status (calibration is not active).
 *Return cal_status (0) to notify to notify that calibration is done.
*/
uint32_t SpeakerProtectionTFA::get_calibration_status()
{
    int32_t ret = 0;
    uint8_t cmd_buf[8] = {0, };
    uint32_t *res_buf;
    uint32_t cmd = GET_STATUS_CHANGE;
    uint32_t status = 0;
    FILE *fd = NULL;
    char str_val[8] = {0, };

    PAL_DBG(LOG_TAG, "Begin");

    if (!cal_status) {
        PAL_ERR(LOG_TAG, "Not calibration state");
        return cal_status;
    }

    memcpy(&cmd_buf[0], (uint8_t *)&cmd, 4);

    ipc_mutex.lock();
    ret = tfa_dsp_msg_write(PARAM_ID_TFADSP_SET_COMMAND, (const uint8_t *)cmd_buf, 4);
    if (ret == 0) {
        ret = tfa_dsp_msg_read(cmd_buf, sizeof(cmd_buf));
    }
    ipc_mutex.unlock();

    if (ret != 0) {
        PAL_ERR(LOG_TAG, "tfa_dsp_msg_write, read failed %d", ret);
        return cal_status;
    }

    res_buf = (uint32_t *)cmd_buf;
    status = res_buf[1];

    PAL_DBG(LOG_TAG, "event 0x%x, status 0x%x", res_buf[0], res_buf[1]);

    if (!(status & TFADSP_FLAG_CALIBRATION_DONE)) {
        return cal_status;
    }

    PAL_DBG(LOG_TAG, "TFADSP_FLAG_CALIBRATION_DONE");

    cal_suc_p = 1; // success
    cal_suc_s = 1; // success

    if (status & TFADSP_FLAG_DAMAGED_SPEAKER_P) {
        PAL_ERR(LOG_TAG, "TFADSP_FLAG_DAMAGED_SPEAKER_P");
        cal_suc_p = 0;
    }
    if (status & TFADSP_FLAG_DAMAGED_SPEAKER_S) {
        PAL_ERR(LOG_TAG, "TFADSP_FLAG_DAMAGED_SPEAKER_S");
        cal_suc_s = 0;
    }

    ret = get_re25c(&cal_val_p, &cal_val_s);

    if (cal_suc_p == 0 || ret != 0)
        cal_val_p = DUMMY_CAL_VALUE;
    if (cal_suc_s == 0 || ret != 0)
        cal_val_s = DUMMY_CAL_VALUE_R;

    if (cal_suc_p == 1 && cal_suc_s == 1 && ret == 0) {
        uint32_t cal_p = 0;
        uint32_t cal_s = 0;
        uint8_t set_re25c_buf[12] = {0, };
        cmd = SET_RE25C_CMD;

        fd = fopen(TFA_CAL_STATE_EFS_FILE, "w");
        if (fd) {
            PAL_DBG(LOG_TAG, "write Disabled in %s", TFA_CAL_STATE_EFS_FILE);
            // Disabled : Cal. success
            fwrite("Disabled", 1, strlen("Disabled"), fd);
            fclose(fd);
        } else {
            PAL_ERR(LOG_TAG, "status fopen error");
        }
        // Send Re25C to tfadsp wrapper in order to fallback to original profile
        cal_p = (cal_val_p*65536)/1000;
        cal_s = (cal_val_s*65536)/1000;
        PAL_DBG(LOG_TAG, "Send Re25C, cal_p 0x%x, cal_s 0x%x", cal_p, cal_s);
        memcpy(&set_re25c_buf[0], (uint8_t *)&cmd, 4);
        memcpy(&set_re25c_buf[4], (uint8_t *)&cal_p, 4);
        memcpy(&set_re25c_buf[8], (uint8_t *)&cal_s, 4);
        tfa_dsp_msg_write(PARAM_ID_TFADSP_SET_RE25C, (const uint8_t *)set_re25c_buf, sizeof(set_re25c_buf));
    }

    if (cal_suc_p == 1 && ret == 0) {
        sprintf(str_val, "%d", cal_val_p);
    } else {
        sprintf(str_val, "%d", 0);
    }
    fd = fopen(TFA_CAL_RDC_EFS_FILE, "w");
    if (fd) {
        PAL_DBG(LOG_TAG, "rdc : %s", str_val);
        fwrite(str_val, 1, strlen(str_val), fd);
        fclose(fd);
    } else {
        PAL_ERR(LOG_TAG, "rdc fopen error");
    }

    memset(str_val, 0, sizeof(str_val));
    if (cal_suc_s == 1 && ret == 0) {
        sprintf(str_val, "%d", cal_val_s);
    } else {
        sprintf(str_val, "%d", 0);
    }
    fd = fopen(TFA_CAL_RDC_R_EFS_FILE, "w");
    if (fd) {
        PAL_DBG(LOG_TAG, "rdc_r : %s", str_val);
        fwrite(str_val, 1, strlen(str_val), fd);
        fclose(fd);
    } else {
        PAL_ERR(LOG_TAG, "rdc_r fopen error");
    }

    memset(str_val, 0, sizeof(str_val));
    sprintf(str_val, "%d", cal_temp);
    fd = fopen(TFA_CAL_TEMP_EFS_FILE, "w");
    if (fd) {
        PAL_DBG(LOG_TAG, "temp : %s", str_val);
        fwrite(str_val, 1, strlen(str_val), fd);
        fclose(fd);
    } else {
        PAL_ERR(LOG_TAG, "temp fopen error");
    }

    fd = fopen(TFA_CAL_TEMP_R_EFS_FILE, "w");
    if (fd) {
        PAL_DBG(LOG_TAG, "temp_r : %s", str_val);
        fwrite(str_val, 1, strlen(str_val), fd);
        fclose(fd);
    } else {
        PAL_ERR(LOG_TAG, "temp_r fopen error");
    }

    cal_status = 0;
    PAL_DBG(LOG_TAG, "Calibration status 0");

    return cal_status;
}

int32_t SpeakerProtectionTFA::get_re25c(uint32_t *mOhm_P, uint32_t *mOhm_S)
{
    int32_t ret = 0;
    uint8_t cmd_buf[8] = {0, };
    uint32_t *res_buf;
    uint32_t cmd = GET_RE25C_CMD;

    PAL_DBG(LOG_TAG, "Begin");

    memcpy(&cmd_buf[0], (uint8_t *)&cmd, 4);

    ipc_mutex.lock();
    ret = tfa_dsp_msg_write(PARAM_ID_TFADSP_SET_COMMAND, (const uint8_t *)cmd_buf, 4);
    if (ret == 0) {
        ret = tfa_dsp_msg_read(cmd_buf, sizeof(cmd_buf));
    }
    ipc_mutex.unlock();

    if (ret == 0) {
        res_buf = (uint32_t *)cmd_buf;

        PAL_DBG(LOG_TAG, "Primary 0x%x, Secondary 0x%x", res_buf[0], res_buf[1]);
        *mOhm_P = (res_buf[0]*1000)/65536;
        *mOhm_S = (res_buf[1]*1000)/65536;
        PAL_DBG(LOG_TAG, "%d mOhm, %d mOhm", *mOhm_P, *mOhm_S);
    } else {
        *mOhm_P = 0;
        *mOhm_S = 0;
        cal_suc_p = 0;
        cal_suc_s = 0;
        PAL_ERR(LOG_TAG, "tfa_dsp_msg_write, read failed %d", ret);
    }

    return ret;
}

int32_t SpeakerProtectionTFA::get_cal_temp()
{
    int32_t ret = 0;
    FILE *fd = NULL;
    char read_buf[10] = {0, };

    // read board temperature
    PAL_DBG(LOG_TAG, "Begin");
    fd = fopen(SYSFS_NODE_REF_TEMP, "r");
    if (fd) {
        if (fread(read_buf, 1, 9, fd) < 0) {
            PAL_ERR(LOG_TAG, "fread error");
            ret = TFADSP_DEFAULT_CAL_TEMP;
            goto exit;
        }
        PAL_DBG(LOG_TAG, "read %s", read_buf);
        ret = atoi(read_buf);
    } else {
        PAL_ERR(LOG_TAG, "fopen error");
        ret = TFADSP_DEFAULT_CAL_TEMP;
    }
exit:
    if (fd)
        fclose(fd);
    PAL_DBG(LOG_TAG, "Board_Temp %d", ret);
    return ret;
}

/*
 *Read R_DC when calibration is done
 *Get device index for the given device.
 *Read status (PASS / FAIL) for the given device, from CalData file.
 *Return ?no_data? (or invalid data, e.g. -1) if FAIL is read for the given device.
 *Return RE_MOHM.
*/
int32_t SpeakerProtectionTFA::get_rdc(uint32_t *mOhm_P, uint32_t *mOhm_S)
{
    int32_t ret = 0;
    FILE *fd = NULL;
    char str_cal[8] = {0, };

    PAL_DBG(LOG_TAG, "Begin");

    *mOhm_P = DUMMY_CAL_VALUE;
    *mOhm_S = DUMMY_CAL_VALUE_R;

    fd = fopen(TFA_CAL_RDC_EFS_FILE, "r");
    if (fd) {
        if (fread(str_cal, 1, sizeof(str_cal), fd) > 0) {
            sscanf(str_cal, "%d", mOhm_P);
        }
        fclose(fd);
    }  else {
        PAL_ERR(LOG_TAG, "%s fopen error", TFA_CAL_RDC_EFS_FILE);
    }
    memset(str_cal, 0, sizeof(str_cal));

    fd = fopen(TFA_CAL_RDC_R_EFS_FILE, "r");
    if (fd) {
        if (fread(str_cal, 1, sizeof(str_cal), fd) > 0) {
            sscanf(str_cal, "%d", mOhm_S);
        }
        fclose(fd);
    } else {
        PAL_ERR(LOG_TAG, "%s fopen error", TFA_CAL_RDC_R_EFS_FILE);
    }

    PAL_DBG(LOG_TAG, "*mOhm_P = %d, *mOhm_S = %d", *mOhm_P, *mOhm_S);
    return ret;
}

/*
 *Read temperature when calibration is done
 *Get device index for the given device.
 *Read status (PASS / FAIL) for the given device, from CalData file.
 *Return ?no_data? (or invalid data, e.g. -1) if FAIL is read for the given device.
 *Return BOARD_TEMP.
*/
void SpeakerProtectionTFA::get_temp(int32_t *temp_l, int32_t *temp_r)
{
    FILE *fd = NULL;
    char str_temp[8] = {0, };

    PAL_DBG(LOG_TAG, "Begin");

    *temp_l = TFADSP_DEFAULT_CAL_TEMP;
    *temp_r = TFADSP_DEFAULT_CAL_TEMP;

    fd = fopen(TFA_CAL_TEMP_EFS_FILE, "r");
    if (fd) {
        if (fread(str_temp, 1, sizeof(str_temp), fd) > 0) {
            sscanf(str_temp, "%d", temp_l);
        }
        fclose(fd);
    } else {
        PAL_ERR(LOG_TAG, "%s fopen error", TFA_CAL_TEMP_EFS_FILE);
    }
    memset(str_temp, 0, sizeof(str_temp));

    fd = fopen(TFA_CAL_TEMP_R_EFS_FILE, "r");
    if (fd) {
        if (fread(str_temp, 1, sizeof(str_temp), fd) > 0) {
            sscanf(str_temp, "%d", temp_r);
        }
        fclose(fd);
    } else {
        PAL_ERR(LOG_TAG, "%s fopen error", TFA_CAL_TEMP_R_EFS_FILE);
    }

    PAL_DBG(LOG_TAG, "*temp_l = %d, *temp_r = %d", *temp_l, *temp_r);
}

/*
 *Surface temperature control: Write TSurf
 *Get device index for the given device and TSurf
 *Read sysfs node (/sys/class/tfa/tfa_stc/power_state or /sys/class/tfa/tfa_stc/power_state_r) to get power state of TFA98xx devices.
 *Return if power state is not active.
 *Wait until TSurf is filled for each device (mono vs. stereo) with channel.
 *Send a message (CustomSetTSurf; 12 bytes) with: 0x00008f01, TSURF_PRIM (4-byte), TSURF_SCND (4-byte; if stereo)
 *Return success / failure in writing surface temperature.
*/
void SpeakerProtectionTFA::set_sknt_control()
{
    uint8_t cmd_buf[12] = {0, };
    uint32_t cmd = CUSTOM_PARAM_SET_TSURF;

    PAL_DBG(LOG_TAG, "surft_left %d, surft_right %d", surft_left, surft_right);

    /* only for tfadsp debuuging */
    if (surft_left == TFADSP_DEBUG_MAGIC_CODE && surft_right != SSRM_DEFAULT_SPK_TEMP && surft_right >= TFADSP_DEBUG_MIN_CODE) {
        cmd = PARAM_ID_TFADSP_DEBUG_CONTROL;
        memcpy(&cmd_buf[0], (uint8_t *)&cmd, 4);
        memcpy(&cmd_buf[4], (uint8_t *)&surft_right, 4);
        PAL_DBG(LOG_TAG, "send PARAM_ID_TFADSP_DEBUG_CONTROL %d", surft_right);
        tfa_dsp_msg_write(PARAM_ID_TFADSP_SET_COMMAND, (const uint8_t *)cmd_buf, sizeof(cmd_buf));
        surft_left = SSRM_DEFAULT_SPK_TEMP;
        surft_right = SSRM_DEFAULT_SPK_TEMP;
        return;
    }
    /*****************************/

    if (surft_left != SSRM_DEFAULT_SPK_TEMP && surft_right != SSRM_DEFAULT_SPK_TEMP) {
        memcpy(&cmd_buf[0], (uint8_t *)&cmd, 4);
        memcpy(&cmd_buf[4], (uint8_t *)&surft_left, 4);
        memcpy(&cmd_buf[8], (uint8_t *)&surft_right, 4);
        tfa_dsp_msg_write(PARAM_ID_TFADSP_SET_COMMAND, (const uint8_t *)cmd_buf, sizeof(cmd_buf));
        surft_left = SSRM_DEFAULT_SPK_TEMP;
        surft_right = SSRM_DEFAULT_SPK_TEMP;
    } else {
        PAL_DBG(LOG_TAG, "skip to send command as both channel data is not ready");
    }

    return;
}

/*
 *Surface temperature control: Read TSpkr: *Triggered by a separate thread in Goodix routine, periodically (10 sec)
 *Get device index to select the given device.
 *Read sysfs node (/sys/class/tfa/tfa_stc/power_state or /sys/class/tfa/tfa_stc/power_state_r) to get power state of TFA98xx devices.
 *Set DEFAULT_REF_TEMP (25 degC, hard-coded) if power state is not active.
 *Send a message (GetTSpkr; 4 bytes) with: 0x000081a8
 *Read buffer (8 bytes) to get: channel (2) * data (4-byte) (for speaker temperature)
 *Convert to temperature format with / TFA2_FW_T_DATA_SCALE (16384)
 *Write TSpkr data (SPK_TEMP_PRIM, SPK_TEMP_SCND) to TSpkrData file.
 *Return success / failure in reading speaker temperature.
*/
void SpeakerProtectionTFA::get_spkt_data()
{
    int32_t ret = 0;
    uint8_t cmd_buf[8] = {0, };
    uint32_t *res_buf;
    uint32_t cmd = CUSTOM_PARAM_GET_TSPKR;
    int32_t power_state, power_state_r;

    rcv_temp = SSRM_DEFAULT_SPK_TEMP;
    spk_temp = SSRM_DEFAULT_SPK_TEMP;

    PAL_DBG(LOG_TAG, "Begin");

    power_state = read_tfa_power_state(TFA_SPK_LEFT);
    power_state_r = read_tfa_power_state(TFA_SPK_RIGHT);

    PAL_DBG(LOG_TAG, "power_state %d, power_state_r %d, cal_status %d", power_state, power_state_r, cal_status);
    if (cal_status == 0 && (power_state == 0 || power_state_r == 0)) {
        memcpy(&cmd_buf[0], (uint8_t *)&cmd, 4);

        ipc_mutex.lock();
        ret = tfa_dsp_msg_write(PARAM_ID_TFADSP_SET_COMMAND, (const uint8_t *)cmd_buf, 4);
        if (ret == 0) {
            ret = tfa_dsp_msg_read(cmd_buf, sizeof(cmd_buf));
        }
        ipc_mutex.unlock();

        if (ret == 0) {
            res_buf = (uint32_t *)cmd_buf;
            PAL_DBG(LOG_TAG, "raw PriT %d, raw SecT %d", res_buf[0], res_buf[1]);
            rcv_temp = (int)(res_buf[0] / TFA2_FW_T_DATA_SCALE);
            spk_temp = (int)(res_buf[1] / TFA2_FW_T_DATA_SCALE);
        } else {
          PAL_ERR(LOG_TAG, "tfa_dsp_msg_write, read failed %d", ret);
        }
    }

    if (power_state != 0 || ret != 0)
        rcv_temp = SSRM_DEFAULT_SPK_TEMP;
    if (power_state_r != 0 || ret != 0)
        spk_temp = SSRM_DEFAULT_SPK_TEMP;

    PAL_DBG(LOG_TAG, "RcvT %d (in degC), SpkT %d (in degC)", rcv_temp, spk_temp);

    return;
}

void SpeakerProtectionTFA::load_cal_file()
{
    FILE *fd = NULL;
    char str_cal[8] = {0, };

    PAL_DBG(LOG_TAG, "Begin");

    cal_val_p = DUMMY_CAL_VALUE;
    cal_val_s = DUMMY_CAL_VALUE;

    fd = fopen(TFA_CAL_RDC_EFS_FILE, "r");
    if (fd) {
        fread(str_cal, 1, sizeof(str_cal), fd);
        fclose(fd);
        sscanf(str_cal, "%d", &cal_val_p);
        PAL_DBG(LOG_TAG, "str_cal = %s, cal_val_p = %d", str_cal, cal_val_p);
        if (cal_val_p == 0) {
            PAL_ERR(LOG_TAG, "cal_val_p is 0 --> set dummy value");
            cal_val_p = DUMMY_CAL_VALUE;
        }
    } else {
        PAL_ERR(LOG_TAG, "%s fopen error", TFA_CAL_RDC_EFS_FILE);
    }

    fd = fopen(TFA_CAL_RDC_R_EFS_FILE, "r");
    if (fd) {
        fread(str_cal, 1, sizeof(str_cal), fd);
        fclose(fd);
        sscanf(str_cal, "%d", &cal_val_s);
        PAL_DBG(LOG_TAG, "str_cal = %s, cal_val_s = %d", str_cal, cal_val_s);
        if (cal_val_s == 0) {
            PAL_ERR(LOG_TAG, "cal_val_s is 0 --> set dummy value");
            cal_val_s = DUMMY_CAL_VALUE;
        }
    } else {
        PAL_ERR(LOG_TAG, "%s fopen error", TFA_CAL_RDC_R_EFS_FILE);
    }

    return;
}

#ifdef SET_IPCID_MIXER_CTL
int32_t SpeakerProtectionTFA::get_miid_pcmid_sc(uint32_t *miid, uint32_t *pcm_dev)
{
    int32_t ret = 0;
    int32_t devId = 0;
    char *pcmDeviceName = NULL;
    std::shared_ptr<Device> dev = nullptr;
    std::vector<Stream*> activeStreams;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<int> pcmDevRxIds;
    struct pal_stream_attributes sAttr;
    std::string backEndName;

    dev = Device::getObject(PAL_DEVICE_OUT_SPEAKER);
    if (NULL == dev) {
        PAL_ERR(LOG_TAG, "dev is NULL");
        goto exit;
    }

    ret = rm->getActiveStream_l(activeStreams, dev);
    if (ret || (activeStreams.size() == 0)) {
        PAL_ERR(LOG_TAG, "no active stream available");
        goto exit;
    }

    stream = static_cast<Stream *>(activeStreams[0]);
    if (NULL == stream) {
        PAL_ERR(LOG_TAG, "stream is NULL");
        goto exit;
    }
    stream->getAssociatedSession(&session);
    if (NULL == session) {
        PAL_ERR(LOG_TAG, "session is NULL");
        goto exit;
    }

    ret = stream->getStreamAttributes(&sAttr);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "getStreamAttributes failed");
        goto exit;
    }

    pcmDevRxIds = rm->allocateFrontEndIds(sAttr, RX_HOSTLESS);
    if (!pcmDevRxIds.size()) {
        PAL_ERR(LOG_TAG, "pcmDevRxIds.size is 0");
        goto exit;
    }
    devId = pcmDevRxIds.at(0);
    PAL_DBG(LOG_TAG, "device = %d", devId);

    ret = rm->getBackendName(PAL_DEVICE_OUT_SPEAKER, backEndName);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain the rx snd device name");
        goto exit;
    }

    PAL_DBG(LOG_TAG, "backEndName.c_str() = %s", backEndName.c_str());
    ret = session->getMIID(backEndName.c_str(), MODULE_SP, miid);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to get MIID");
        goto exit;
    }
    PAL_DBG(LOG_TAG, "miid = 0x%x", *miid);

    pcmDeviceName = rm->getDeviceNameFromID(devId);
    if (pcmDeviceName) {
        PAL_DBG(LOG_TAG, "pcmDeviceName = %s", pcmDeviceName);
        sscanf(pcmDeviceName, "PCM%d", pcm_dev);
    }
    PAL_DBG(LOG_TAG, "pcm_dev = %d", *pcm_dev);
exit:
    return ret;
}
#endif

void SpeakerProtectionTFA::Calibration_Thread()
{
    uint32_t count = 0;
    std::unique_lock<std::mutex> lck2(cal_mutex);

    uint8_t cmd_buf[12] = {0, };
    uint32_t do_cal = 0x1;
    FILE *fd = NULL;
    char read_buf[10] = {0, };

    PAL_DBG(LOG_TAG, "Begin");

    cal_status = 1; // calibration is on-going

    fd = fopen(SYSFS_NODE_CAL_CONFIG, "w");
    if (fd) {
        char config_buf[2] = {'1', 0};
        fwrite(config_buf, 1, sizeof(config_buf), fd);
        fclose(fd);
        for (int i=0; i < 10; i++) { // polling for 100ms
            usleep(10000);
            fd = fopen(SYSFS_NODE_CAL_CONFIG, "r");
            if (fd) {
                fread(read_buf, 1, 9, fd);
                fclose(fd);
                PAL_DBG(LOG_TAG, "cal_config read : %s", read_buf);
            }
            if (!memcmp(read_buf, "configure", 9))
                break;
        }
    } else {
        PAL_ERR(LOG_TAG, "cal_config fopen_w error");
    }

    cal_temp = get_cal_temp();
    memcpy(&cmd_buf[0], (uint8_t *)&do_cal, 4);
    memcpy(&cmd_buf[4], (uint8_t *)&cal_temp, 4);
    memcpy(&cmd_buf[8], (uint8_t *)&cal_temp, 4);

    tfa_dsp_msg_write(PARAM_ID_TFADSP_CALIBRATE, (const uint8_t *)cmd_buf, sizeof(cmd_buf));

    PAL_DBG(LOG_TAG, "Calibration status checking");

    while (active_state) {
        cal_wait_cond.wait_for(lck2, std::chrono::milliseconds(CAL_STATUS_CHECK_INTERVAL_MS));
        if (active_state) {
            count++;
            PAL_DBG(LOG_TAG, "calibration status check count %d", count);
            get_calibration_status();
            if (!cal_status)
                break;
        }
        if (count >= CAL_STATUS_CHECK_TIMEOUT_COUNT) {
            PAL_ERR(LOG_TAG,"Calibration Timeout Error");
            break;
        }
    }

    fd = fopen(SYSFS_NODE_CAL_CONFIG, "w");
    if (fd) {
        char config_buf[2] = {'0', 0};
        PAL_DBG(LOG_TAG, "Write 0 in tfa_cal/config");
        fwrite(config_buf, 1, sizeof(config_buf), fd);
        fclose(fd);
    } else {
        PAL_ERR(LOG_TAG, "cal_config fopen_w error");
    }

    cal_status = 0;

    PAL_DBG(LOG_TAG, "End");
}

void SpeakerProtectionTFA::Activate_Tfadsp_Thread()
{
    PAL_DBG(LOG_TAG, "begin");
    activate_tfadsp_thread_running = true;
    activate_tfadsp();
    activate_tfadsp_thread_running = false;
    PAL_DBG(LOG_TAG, "end");
}

SpeakerFeedbackTFA::SpeakerFeedbackTFA(struct pal_device *device,
                                std::shared_ptr<ResourceManager> Rm):Device(device, Rm)
{
    struct pal_device_info devinfo = {};

    memset(&mDeviceAttr, 0, sizeof(struct pal_device));
    memcpy(&mDeviceAttr, device, sizeof(struct pal_device));
    rm = Rm;
    rm->getDeviceInfo(mDeviceAttr.id, PAL_STREAM_PROXY, mDeviceAttr.custom_config.custom_key, &devinfo);
}

SpeakerFeedbackTFA::~SpeakerFeedbackTFA()
{
}

int32_t SpeakerFeedbackTFA::start()
{
    PAL_DBG(LOG_TAG2," Feedback start\n");

    Device::start();

    std::shared_ptr<Device> instance = Device::getObject(PAL_DEVICE_OUT_SPEAKER);
    instance->setParameter(PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_START, nullptr);

    return 0;
}

int32_t SpeakerFeedbackTFA::stop()
{
    PAL_DBG(LOG_TAG2," Feedback stop\n");

    std::shared_ptr<Device> instance = Device::getObject(PAL_DEVICE_OUT_SPEAKER);
    instance->setParameter(PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_STOP, nullptr);

    Device::stop();
    return 0;
}

std::shared_ptr<Device> SpeakerFeedbackTFA::getInstance(struct pal_device *device,
                                                     std::shared_ptr<ResourceManager> Rm)
{
    PAL_DBG(LOG_TAG2," Feedback getInstance\n");
    if (!obj) {
        std::shared_ptr<Device> sp(new SpeakerFeedbackTFA(device, Rm));
        obj = sp;
    }
    return obj;
}

std::shared_ptr<Device> SpeakerFeedbackTFA::getObject()
{
    return obj;
}
#endif
