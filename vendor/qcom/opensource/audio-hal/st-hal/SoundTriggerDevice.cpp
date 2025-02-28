/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "STHAL: SoundTriggerDevice"

#define ATRACE_TAG (ATRACE_TAG_AUDIO | ATRACE_TAG_HAL)

#define LOG_NDEBUG 0

#include "SoundTriggerDevice.h"
#include "SoundTriggerCommon.h"

#include <log/log.h>
#include <cutils/atomic.h>
#include <utils/Trace.h>

#include "PalApi.h"

#define XSTR(x) STR(x)
#define STR(x) #x

static int stdev_ref_cnt = 0;
static struct sound_trigger_properties_extended_1_3 hw_properties_extended;

/*
 * Current API version used by STHAL. Queried by AHAL
 * for compatibility check with STHAL
 */
extern "C" const unsigned int sthal_prop_api_version =
    STHAL_PROP_API_CURRENT_VERSION;

std::shared_ptr<SoundTriggerDevice> SoundTriggerDevice::stdev_ = nullptr;
std::shared_ptr<sound_trigger_hw_device> SoundTriggerDevice::device_ = nullptr;

#ifdef LSM_HIDL_ENABLED
#include <hidl/HidlTransportSupport.h>
#include <hidl/LegacySupport.h>

#include <lsm_server_wrapper.h>

#include <vendor/qti/hardware/ListenSoundModel/1.0/IListenSoundModel.h>
using vendor::qti::hardware::ListenSoundModel::V1_0::IListenSoundModel;
using vendor::qti::hardware::ListenSoundModel::V1_0::implementation::ListenSoundModel;
using android::hardware::defaultPassthroughServiceImplementation;
using android::sp;
using namespace android::hardware;
using android::OK;
#endif

static int stdev_close(hw_device_t *device)
{
    int status = 0;
    std::shared_ptr<SoundTriggerDevice> st_device = nullptr;

    STHAL_DBG(LOG_TAG, "Enter");
    ATRACE_BEGIN("sthal: stdev_close");

    st_device = SoundTriggerDevice::GetInstance(device);
    if (!st_device) {
        STHAL_ERR(LOG_TAG, "error, GetInstance failed");
        status = -EINVAL;
        goto exit;
    }

    if (--stdev_ref_cnt != 0) {
        goto exit;
    }

exit:
    ATRACE_END();
    STHAL_DBG(LOG_TAG, "Exit, status = %d",status);

    return status;
}

static int stdev_get_properties(const struct sound_trigger_hw_device *dev,
    struct sound_trigger_properties *properties)
{
    int status = 0;
    std::shared_ptr<SoundTriggerDevice> st_device = nullptr;
    struct pal_st_properties *qstp = nullptr;
    size_t size = 0;

    STHAL_VERBOSE(LOG_TAG, "Enter");

    if (!dev || !properties) {
        STHAL_ERR(LOG_TAG, "invalid inputs");
        return -EINVAL;
    }

    st_device = SoundTriggerDevice::GetInstance(dev);
    if (!st_device) {
        STHAL_ERR(LOG_TAG, "error, GetInstance failed");
        return -EINVAL;
    }

    status =  pal_get_param(PAL_PARAM_ID_GET_SOUND_TRIGGER_PROPERTIES,
                  (void **)&qstp, &size, nullptr);
    if (status || !qstp || size < sizeof(struct pal_st_properties)) {
        STHAL_ERR(LOG_TAG, "query properties from pal failed, status %d", status);
        goto exit;
    }

    memcpy(properties, qstp, sizeof(struct sound_trigger_properties));

    STHAL_VERBOSE(LOG_TAG, "version=0x%x recognition_modes=%d, capture_transition=%d, "
           "concurrent_capture=%d",properties->version,
           properties->recognition_modes, properties->capture_transition,
           properties->concurrent_capture);
    hw_properties_extended.header.version = SOUND_TRIGGER_DEVICE_API_VERSION_1_0;

exit:
    if (qstp)
        free(qstp);

    STHAL_VERBOSE(LOG_TAG, "Exit, status = %d", status);
    return status;
}

static int stdev_load_sound_model(const struct sound_trigger_hw_device *dev,
                                  struct sound_trigger_sound_model *sound_model,
                                  sound_model_callback_t callback __unused,
                                  void *cookie __unused,
                                  sound_model_handle_t *handle)
{
    int status = 0;
    std::shared_ptr<SoundTriggerDevice> st_device = nullptr;
    SoundTriggerSession* st_session = nullptr;

    STHAL_VERBOSE(LOG_TAG, "Enter");
    ATRACE_BEGIN("sthal: stdev_load_sound_model");

    st_device = SoundTriggerDevice::GetInstance(dev);
    if (!st_device) {
        STHAL_ERR(LOG_TAG, "error, GetInstance failed");
        status = -EINVAL;
        goto exit;
    }

    // assign sound model handle
    *handle = android_atomic_inc(&st_device->session_id_);

    // create sound trigger stream
    st_session = new SoundTriggerSession(*handle, st_device->ahal_callback_);
    status = st_device->RegisterSession(st_session);
    if (status) {
        STHAL_ERR(LOG_TAG, "error, failed to register session");
        goto exit;
    }

    // load sound model
    status = st_session->LoadSoundModel(sound_model);
    if (status)
        STHAL_ERR(LOG_TAG, "error, Failed to load sound model, status = %d", status);

exit:
    ATRACE_END();
    STHAL_VERBOSE(LOG_TAG, "Exit, status = %d", status);

    return status;
}

static int stdev_unload_sound_model(const struct sound_trigger_hw_device *dev,
                                    sound_model_handle_t handle)
{
    int status = 0;
    std::shared_ptr<SoundTriggerDevice> st_device = nullptr;
    SoundTriggerSession* st_session = nullptr;

    STHAL_VERBOSE(LOG_TAG, "Enter");
    ATRACE_BEGIN("sthal: stdev_unload_sound_model");

    st_device = SoundTriggerDevice::GetInstance(dev);
    if (!st_device) {
        STHAL_ERR(LOG_TAG, "error, GetInstance failed");
        status = -EINVAL;
        goto exit;
    }

    st_session = st_device->GetSession(handle);
    if (!st_session) {
        STHAL_ERR(LOG_TAG, "error, failed to get st stream by handle %d", handle);
        status = -EINVAL;
        goto exit;
    }

    // deregister sound model handle
    status = st_session->UnloadSoundModel();
    if (status) {
        STHAL_ERR(LOG_TAG, "error, failed to unload sound model, status = %d", status);
        goto exit;
    }

    status = st_device->DeregisterSession(st_session);
    if (status) {
        STHAL_ERR(LOG_TAG, "error, failed to deregister session");
        goto exit;
    }

exit:
    ATRACE_END();
    STHAL_VERBOSE(LOG_TAG, "Exit, status = %d", status);

    return status;
}

static int stdev_start_recognition
(
    const struct sound_trigger_hw_device *dev,
    sound_model_handle_t sound_model_handle,
    const struct sound_trigger_recognition_config *config,
    recognition_callback_t callback,
    void *cookie)
{
    int status = 0;
    std::shared_ptr<SoundTriggerDevice> st_device = nullptr;
    SoundTriggerSession* st_session = nullptr;

    STHAL_VERBOSE(LOG_TAG, "Enter");
    ATRACE_BEGIN("sthal: stdev_start_recognition");

    st_device = SoundTriggerDevice::GetInstance(dev);
    if (!st_device) {
        STHAL_ERR(LOG_TAG, "error, GetInstance failed");
        status = -EINVAL;
        goto exit;
    }

    st_session = st_device->GetSession(sound_model_handle);
    if (!st_session) {
        STHAL_ERR(LOG_TAG, "error, failed to get st stream by handle %d",
              sound_model_handle);
        status = -EINVAL;
        goto exit;
    }

    status = st_session->StartRecognition(config,
        callback, cookie, hw_properties_extended.header.version);
    if (status) {
        STHAL_ERR(LOG_TAG, "error, failed to start recognition, status = %d",
              status);
        goto exit;
    }

exit:
    ATRACE_END();
    STHAL_VERBOSE(LOG_TAG, "Exit, status = %d", status);

    return status;
}

static int stdev_stop_recognition(const struct sound_trigger_hw_device *dev,
                                  sound_model_handle_t sound_model_handle)
{
    int status = 0;
    std::shared_ptr<SoundTriggerDevice> st_device = nullptr;
    SoundTriggerSession* st_session = nullptr;

    STHAL_VERBOSE(LOG_TAG, "Enter");
    ATRACE_BEGIN("sthal: stdev_stop_recognition");

    st_device = SoundTriggerDevice::GetInstance(dev);
    if (!st_device) {
        STHAL_ERR(LOG_TAG, "error, GetInstance failed");
        status = -EINVAL;
        goto exit;
    }

    st_session = st_device->GetSession(sound_model_handle);
    if (!st_session) {
        STHAL_ERR(LOG_TAG, "error, Failed to get st stream by handle %d",
              sound_model_handle);
        status = -EINVAL;
        goto exit;
    }

    // deregister sound model handle
    status = st_session->StopRecognition();
    if (status) {
        STHAL_ERR(LOG_TAG, "error, failed to stop recognition, status = %d",
              status);
        goto exit;
    }

exit:
    ATRACE_END();
    STHAL_VERBOSE(LOG_TAG, "Exit, status = %d", status);

    return status;
}

#ifdef ST_SUPPORT_GET_MODEL_STATE
static int stdev_get_model_state(const struct sound_trigger_hw_device *dev,
    sound_model_handle_t handle)
{
    return 0;
}
#endif

static int stdev_start_recognition_extended
(
    const struct sound_trigger_hw_device *dev,
    sound_model_handle_t sound_model_handle,
    const struct sound_trigger_recognition_config_header *config,
    recognition_callback_t callback,
    void *cookie
)
{
    return stdev_start_recognition(dev, sound_model_handle,
           &((struct sound_trigger_recognition_config_extended_1_3 *)config)->base,
           callback, cookie);
}

static int stdev_stop_all_recognitions(const struct sound_trigger_hw_device* dev __unused)
{
    STHAL_VERBOSE(LOG_TAG, "unsupported API");
    return -ENOSYS;
}

static int stdev_get_parameter(const struct sound_trigger_hw_device *dev __unused,
    sound_model_handle_t sound_model_handle __unused,
    sound_trigger_model_parameter_t model_param __unused, int32_t* value __unused)
{
    STHAL_VERBOSE(LOG_TAG, "unsupported API");
    return -EINVAL;
}

static int stdev_set_parameter(const struct sound_trigger_hw_device *dev __unused,
    sound_model_handle_t sound_model_handle __unused,
    sound_trigger_model_parameter_t model_param __unused, int32_t value __unused)
{
    STHAL_VERBOSE(LOG_TAG, "unsupported API");
    return -EINVAL;
}

static int stdev_query_parameter(const struct sound_trigger_hw_device *dev __unused,
    sound_model_handle_t sound_model_handle __unused,
    sound_trigger_model_parameter_t  model_param __unused,
    sound_trigger_model_parameter_range_t* param_range)
{
    if (param_range)
        param_range->is_supported = false;
    return 0;
}

static const struct sound_trigger_properties_header*
stdev_get_properties_extended(const struct sound_trigger_hw_device *dev)
{
    int status = 0;
    struct sound_trigger_properties_header *prop_hdr = NULL;
    std::shared_ptr<SoundTriggerDevice> st_device = nullptr;
    SoundTriggerSession* st_session = nullptr;

    STHAL_VERBOSE(LOG_TAG, "Enter");
    if (!dev) {
        STHAL_WARN(LOG_TAG, "invalid sound_trigger_hw_device received");
        return nullptr;
    }

    st_device = SoundTriggerDevice::GetInstance(dev);
    if (!st_device) {
        STHAL_ERR(LOG_TAG, "error, GetInstance failed");
        return nullptr;
    }

    prop_hdr = (struct sound_trigger_properties_header *)&hw_properties_extended;
    status = stdev_get_properties(dev, &hw_properties_extended.base);
    if (status) {
        STHAL_WARN(LOG_TAG, "Failed to initialize the stdev properties");
        return nullptr;
    }
    hw_properties_extended.header.size = sizeof(struct sound_trigger_properties_extended_1_3);
    hw_properties_extended.audio_capabilities = 0;
    hw_properties_extended.header.version = SOUND_TRIGGER_DEVICE_API_VERSION_1_3;

    // TODO: update first stage module version after confirm with Venky
    st_session = new SoundTriggerSession(0, nullptr);
    status = st_session->GetModuleVersion(hw_properties_extended.supported_model_arch);
    delete st_session;
    if (status) {
        STHAL_ERR(LOG_TAG, "Failed to get module version, status = %d", status);
    }

    return prop_hdr;
}

std::shared_ptr<SoundTriggerDevice> SoundTriggerDevice::GetInstance()
{
    if (!stdev_) {
        stdev_ = std::shared_ptr<SoundTriggerDevice>
            (new SoundTriggerDevice());
        device_ = std::shared_ptr<sound_trigger_hw_device>
            (new sound_trigger_hw_device());
    }

    return stdev_;
}

std::shared_ptr<SoundTriggerDevice> SoundTriggerDevice::GetInstance(
    const hw_device_t *device)
{
    if (device == (hw_device_t *)&(device_.get()->common))
        return SoundTriggerDevice::stdev_;
    else
        return nullptr;
}

std::shared_ptr<SoundTriggerDevice> SoundTriggerDevice::GetInstance(
    const struct sound_trigger_hw_device *st_device)
{
    if (st_device == (struct sound_trigger_hw_device *)device_.get())
        return SoundTriggerDevice::stdev_;
    else
        return nullptr;
}

int SoundTriggerDevice::Init(hw_device_t **device, const hw_module_t *module)
{
    int status = 0;

    STHAL_DBG(LOG_TAG, "Enter");

    if (stdev_ref_cnt != 0) {
        *device = &device_->common;
        stdev_ref_cnt++;
        STHAL_DBG(LOG_TAG, "returning existing stdev instance, exit");
        return status;
    }

#ifdef LSM_HIDL_ENABLED
    /* Register LSM Lib HIDL service */
    STHAL_DBG(LOG_TAG, "Register LSM HIDL service");
    sp<IListenSoundModel> service = new ListenSoundModel();
    if(android::OK !=  service->registerAsService())
        STHAL_WARN(LOG_TAG, "Could not register LSM HIDL service");
#endif

    // load audio hal
    status = LoadAudioHal();
    if (status) {
        STHAL_ERR(LOG_TAG, "failed to load audio hal, status = %d", status);
        goto exit;
    }

    // platform init
    status = PlatformInit();
    if (status) {
        STHAL_ERR(LOG_TAG, "failed to do platform init, status = %d", status);
        goto exit;
    }

    // assign function pointers
    device_->common.tag = HARDWARE_DEVICE_TAG;
    device_->common.version = SOUND_TRIGGER_DEVICE_API_VERSION_1_3;
    device_->common.module = (struct hw_module_t *)module;
    device_->common.close = stdev_close;
    device_->get_properties = stdev_get_properties;
    device_->load_sound_model = stdev_load_sound_model;
    device_->unload_sound_model = stdev_unload_sound_model;
    device_->start_recognition = stdev_start_recognition;
    device_->stop_recognition = stdev_stop_recognition;
    device_->get_properties_extended = stdev_get_properties_extended;
    device_->start_recognition_extended = stdev_start_recognition_extended;
    device_->stop_all_recognitions = stdev_stop_all_recognitions;
    device_->get_parameter = stdev_get_parameter;
    device_->set_parameter = stdev_set_parameter;
    device_->query_parameter = stdev_query_parameter;
#ifdef ST_SUPPORT_GET_MODEL_STATE
    device_->get_model_state = stdev_get_model_state;
#endif
    *device = &device_->common;

    hw_properties_extended.header.size =
        sizeof(struct sound_trigger_properties_extended_1_3);
    hw_properties_extended.audio_capabilities = 0;
    hw_properties_extended.header.version =
        SOUND_TRIGGER_DEVICE_API_VERSION_1_3;

    available_devices_ = AUDIO_DEVICE_IN_BUILTIN_MIC;
    session_id_ = 1;
    stdev_ref_cnt++;

exit:
    STHAL_VERBOSE(LOG_TAG, "Exit, status = %d", status);

    return status;
}

int SoundTriggerDevice::LoadAudioHal()
{
    int status = 0;
    char audio_hal_lib[100];
    void *apiVersion = nullptr;
    audio_extn_hidl_init initAudioExtn = nullptr;

    STHAL_DBG(LOG_TAG, "Enter");

    snprintf(audio_hal_lib, sizeof(audio_hal_lib), "%s/%s.%s.so",
             AUDIO_HAL_LIBRARY_PATH1, AUDIO_HAL_NAME_PREFIX,
             XSTR(SOUND_TRIGGER_PLATFORM));
    if (access(audio_hal_lib, R_OK)) {
        snprintf(audio_hal_lib, sizeof(audio_hal_lib), "%s/%s.%s.so",
                 AUDIO_HAL_LIBRARY_PATH2, AUDIO_HAL_NAME_PREFIX,
                 XSTR(SOUND_TRIGGER_PLATFORM));
        if (access(audio_hal_lib, R_OK)) {
            STHAL_ERR(LOG_TAG, "ERROR. %s not found", audio_hal_lib);
            status = -ENOENT;
            goto error;
        }
    }

    ahal_handle_ = dlopen(audio_hal_lib, RTLD_NOW);
    if (!ahal_handle_) {
        STHAL_ERR(LOG_TAG, "ERROR. %s", dlerror());
        status = -ENODEV;
        goto error;
    }

    ahal_callback_ = (audio_hw_call_back_t)dlsym(ahal_handle_,
                                                 "audio_hw_call_back");
    if (!ahal_callback_) {
        STHAL_ERR(LOG_TAG, "error, failed to get symbol for audio_hw_call_back");
        status = -ENODEV;
        goto error;
    }
    apiVersion = dlsym(ahal_handle_, "sthal_prop_api_version");
    if (!apiVersion) {
        sthal_prop_api_version_ = 0;
        status  = 0;  // passthru for backward compability
    } else {
        sthal_prop_api_version_ = *(int*)apiVersion;
        if (MAJOR_VERSION(sthal_prop_api_version_) !=
            MAJOR_VERSION(sthal_prop_api_version)) {
            STHAL_ERR(LOG_TAG, "Incompatible API versions sthal:0x%x != ahal:0x%x",
                  STHAL_PROP_API_CURRENT_VERSION,
                  sthal_prop_api_version_);
            goto error;
        }
        STHAL_DBG(LOG_TAG, "ahal is using API version 0x%04x",
              sthal_prop_api_version_);
    }

    initAudioExtn = (audio_extn_hidl_init)dlsym(ahal_handle_, "check_init_audio_extension");
    if (!initAudioExtn) {
        STHAL_WARN(LOG_TAG, "error, failed to get symbol for initAudioExtn");
    } else {
        initAudioExtn();
    }
    return status;

error:
    if (ahal_handle_) {
        dlclose(ahal_handle_);
        ahal_handle_ = nullptr;
    }
    STHAL_VERBOSE(LOG_TAG, "Exit, status = %d", status);

    return status;
}

int SoundTriggerDevice::PlatformInit()
{
    int status = 0;
    // load platform related config
    conc_capture_supported_ = false;

    return status;
}

SoundTriggerSession* SoundTriggerDevice::GetSession(sound_model_handle_t handle)
{
    for (int i = 0; i < session_list_.size(); i++) {
        if (handle == session_list_[i]->GetSoundModelHandle())
            return session_list_[i];
    }

    return nullptr;
}

int SoundTriggerDevice::RegisterSession(SoundTriggerSession* session)
{
    int status = 0;

    mutex_.lock();
    session_list_.push_back(session);
    mutex_.unlock();

    return status;
}

int SoundTriggerDevice::DeregisterSession(SoundTriggerSession* session)
{
    int status = 0;
    SoundTriggerSession *st_session = nullptr;

    mutex_.lock();
    typename std::vector<SoundTriggerSession *>::iterator iter =
        std::find(session_list_.begin(), session_list_.end(), session);
    if (iter != session_list_.end()) {
        st_session = *iter;
        if (!st_session) {
            delete st_session;
            st_session = nullptr;
        }
        session_list_.erase(iter);
    } else {
        status = -ENOENT;
        STHAL_ERR(LOG_TAG, "session found in session list");
    }
    mutex_.unlock();

    return status;
}

static int stdev_open(const hw_module_t* module, const char* name,
                      hw_device_t** device)
{
    int status = 0;
    std::shared_ptr<SoundTriggerDevice> st_device = nullptr;

    STHAL_DBG(LOG_TAG, "Enter");
    ATRACE_BEGIN("sthal: stdev_open");

    if (strcmp(name, SOUND_TRIGGER_HARDWARE_INTERFACE) != 0) {
        STHAL_ERR(LOG_TAG, "ERROR. wrong interface");
        status = -EINVAL;
        goto exit;
    }

#ifdef SEC_AUDIO_BOOT_ON_ERR
#ifdef SEC_FACTORY_INTERPOSER
    STHAL_ERR(LOG_TAG, "skip stdev_open on secondary bin (not support audio/sthal)");
    status = -EINVAL;
    goto exit;
#else
    if (property_get_bool("vendor.audio.use.primary.default", false)) {
        STHAL_ERR(LOG_TAG, "skip stdev_open because sndcard is not active");
        status = -EINVAL;
        goto exit;
    }
#endif
#endif

    st_device = SoundTriggerDevice::GetInstance();
    if (!st_device) {
        STHAL_ERR(LOG_TAG, "error, GetInstance failed");
        goto exit;
    }

    status = st_device->Init(device, module);
    if (status || (*device == nullptr))
        STHAL_ERR(LOG_TAG, "error, audio device init failed, ret(%d), *device(%p)",
              status, *device);

exit:
    ATRACE_END();
    STHAL_VERBOSE(LOG_TAG, "Exit, status = %d", status);

    return status;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = stdev_open,
};

struct sound_trigger_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = SOUND_TRIGGER_MODULE_API_VERSION_1_0,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = SOUND_TRIGGER_HARDWARE_MODULE_ID,
        .name = "Sound trigger HAL",
        .author = "QUALCOMM Technologies, Inc",
        .methods = &hal_module_methods,
    },
};
