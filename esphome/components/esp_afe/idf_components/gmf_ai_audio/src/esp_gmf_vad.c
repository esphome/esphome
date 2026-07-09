/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <sdkconfig.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_gmf_audio_element.h"
#include "esp_gmf_cache.h"
#include "esp_gmf_cap.h"
#include "esp_gmf_caps_def.h"
#include "esp_gmf_err.h"
#include "esp_gmf_job.h"
#include "esp_gmf_node.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_payload.h"
#include "esp_gmf_vad.h"

#if CONFIG_SR_VADN_VADNET1_MEDIUM
#include "esp_vadn_models.h"
#include "model_path.h"
#endif

#define ESP_VAD_BYTES_PER_SAMPLE (sizeof(int16_t))

/**
 * @brief  Internal state of the VAD GMF audio element
 */
typedef struct {
  esp_gmf_audio_element_t parent; /*!< GMF audio element base */
#if CONFIG_SR_VADN_VADNET1_MEDIUM
  const esp_vadn_iface_t *vadn_iface; /*!< VADNet model interface */
  model_iface_data_t *vadn_model;     /*!< VADNet model instance */
  srmodel_list_t *models;             /*!< List of loaded speech models from flash partition */
#else
  vad_handle_t vad_handle; /*!< WebRTC VAD algorithm handle */
#endif
  uint16_t frame_samples;     /*!< PCM samples per VAD frame */
  uint16_t frame_bytes;       /*!< Input bytes per VAD frame */
  bool state_valid;           /*!< True after the first detection result */
  vad_state_t last_state;     /*!< Previous VAD state for change detection */
  esp_gmf_cache_t *cache;     /*!< Input frame cache for fixed-size blocks */
  esp_gmf_payload_t *in_load; /*!< Current acquired input payload from port */
} gmf_vad_t;

static const char *TAG = "GMF_VAD";

static inline bool gmf_vad_is_supported_sample_rate(int sample_rate) {
  return (sample_rate == 8000) || (sample_rate == 16000) || (sample_rate == 32000);
}

#if !CONFIG_SR_VADN_VADNET1_MEDIUM
static inline bool gmf_vad_is_supported_frame_ms(int frame_ms) {
  return (frame_ms == 10) || (frame_ms == 20) || (frame_ms == 30);
}
#endif

#if CONFIG_SR_VADN_VADNET1_MEDIUM
static inline const char *gmf_vad_get_partition_label(const esp_gmf_vad_cfg_t *cfg) {
  if ((cfg == NULL) || (cfg->partition_label == NULL) || (cfg->partition_label[0] == '\0')) {
    return "model";
  }
  return cfg->partition_label;
}

static inline int gmf_vad_get_min_speech_ms(const esp_gmf_vad_cfg_t *cfg) {
  if ((cfg == NULL) || (cfg->min_speech_ms <= 0)) {
    return 32;
  }
  return cfg->min_speech_ms;
}

static inline int gmf_vad_get_min_noise_ms(const esp_gmf_vad_cfg_t *cfg) {
  if ((cfg == NULL) || (cfg->min_noise_ms <= 0)) {
    return 64;
  }
  return cfg->min_noise_ms;
}

static const char *gmf_vad_get_model_name(gmf_vad_t *gmf_vad, const esp_gmf_vad_cfg_t *cfg) {
  if ((cfg != NULL) && (cfg->model_name != NULL) && (cfg->model_name[0] != '\0')) {
    return cfg->model_name;
  }
  if ((gmf_vad == NULL) || (gmf_vad->models == NULL)) {
    return NULL;
  }
  return esp_srmodel_filter(gmf_vad->models, ESP_VADN_PREFIX, NULL);
}
#endif

static esp_gmf_err_t gmf_vad_check_config(const esp_gmf_vad_cfg_t *cfg) {
  ESP_GMF_NULL_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
  if (gmf_vad_is_supported_sample_rate(cfg->sample_rate) == false) {
    ESP_LOGE(TAG, "Unsupported sample rate: %d", cfg->sample_rate);
    return ESP_GMF_ERR_INVALID_ARG;
  }
#if CONFIG_SR_VADN_VADNET1_MEDIUM
  if (cfg->frame_ms < 0) {
    ESP_LOGE(TAG, "Invalid frame duration: %d ms", cfg->frame_ms);
    return ESP_GMF_ERR_INVALID_ARG;
  }
#else
  if (gmf_vad_is_supported_frame_ms(cfg->frame_ms) == false) {
    ESP_LOGE(TAG, "Unsupported frame duration: %d ms", cfg->frame_ms);
    return ESP_GMF_ERR_INVALID_ARG;
  }
#endif
  if ((cfg->vad_mode < VAD_MODE_0) || (cfg->vad_mode > VAD_MODE_4)) {
    ESP_LOGE(TAG, "Unsupported VAD mode: %d", cfg->vad_mode);
    return ESP_GMF_ERR_INVALID_ARG;
  }
  return ESP_GMF_ERR_OK;
}

static inline void gmf_vad_release_backend(gmf_vad_t *gmf_vad) {
  if (gmf_vad == NULL) {
    return;
  }
#if CONFIG_SR_VADN_VADNET1_MEDIUM
  if ((gmf_vad->vadn_model != NULL) && (gmf_vad->vadn_iface != NULL) && (gmf_vad->vadn_iface->destroy != NULL)) {
    gmf_vad->vadn_iface->destroy(gmf_vad->vadn_model);
    gmf_vad->vadn_model = NULL;
  }
  if (gmf_vad->models != NULL) {
    esp_srmodel_deinit(gmf_vad->models);
    gmf_vad->models = NULL;
  }
  gmf_vad->vadn_iface = NULL;
#else
  if (gmf_vad->vad_handle) {
    vad_destroy(gmf_vad->vad_handle);
    gmf_vad->vad_handle = NULL;
  }
#endif
}

static esp_gmf_err_t gmf_vad_new(void *cfg, esp_gmf_obj_handle_t *handle) {
  ESP_GMF_NULL_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;
  esp_gmf_vad_cfg_t *vad_cfg = (esp_gmf_vad_cfg_t *) cfg;
  esp_gmf_obj_handle_t new_obj = NULL;
  esp_gmf_err_t ret = esp_gmf_vad_init(vad_cfg, &new_obj);
  if (ret != ESP_GMF_ERR_OK) {
    return ret;
  }
  *handle = (void *) new_obj;
  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t gmf_vad_received_event_handler(esp_gmf_event_pkt_t *evt, void *ctx) {
  ESP_GMF_NULL_CHECK(TAG, evt, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, ctx, { return ESP_GMF_ERR_INVALID_ARG; });
  esp_gmf_element_handle_t self = (esp_gmf_element_handle_t) ctx;
  esp_gmf_element_handle_t el = evt->from;
  esp_gmf_event_state_t state = ESP_GMF_EVENT_STATE_NONE;
  esp_gmf_element_get_state(self, &state);
  esp_gmf_element_handle_t prev = NULL;
  esp_gmf_element_get_prev_el(self, &prev);
  if ((state == ESP_GMF_EVENT_STATE_NONE) || (prev == el)) {
    if (evt->sub == ESP_GMF_INFO_SOUND) {
      esp_gmf_info_sound_t info = {0};
      if ((evt->payload == NULL) || (evt->payload_size < sizeof(info))) {
        ESP_LOGE(TAG, "Invalid payload size: %u, expected: %zu", evt->payload_size, sizeof(info));
        return ESP_GMF_ERR_INVALID_ARG;
      }
      memcpy(&info, evt->payload, sizeof(info));
      ESP_LOGD(TAG, "RECV info, from: %s-%p, next: %p, self: %s-%p, type: %x, state: %s, rate: %d, ch: %d, bits: %d",
               OBJ_GET_TAG(el), el, esp_gmf_node_for_next((esp_gmf_node_t *) el), OBJ_GET_TAG(self), self, evt->type,
               esp_gmf_event_get_state_str(state), info.sample_rates, info.channels, info.bits);
      esp_gmf_vad_cfg_t *cfg = OBJ_GET_CFG(self);
      if (info.sample_rates != cfg->sample_rate || info.bits != 16 || info.channels != 1) {
        ESP_LOGE(TAG, "Unsupported format, rate: %d, bits: %d, channels: %d", info.sample_rates, info.bits,
                 info.channels);
        return ESP_GMF_ERR_NOT_SUPPORT;
      }
      esp_gmf_element_set_state(self, ESP_GMF_EVENT_STATE_INITIALIZED);
    }
  }
  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t gmf_vad_destroy(esp_gmf_audio_element_handle_t self) {
  ESP_GMF_NULL_CHECK(TAG, self, { return ESP_GMF_ERR_INVALID_ARG; });

  gmf_vad_t *gmf_vad = (gmf_vad_t *) self;

  if (OBJ_GET_CFG(self)) {
    esp_gmf_oal_free(OBJ_GET_CFG(self));
  }
  gmf_vad_release_backend(gmf_vad);
  if (gmf_vad->cache) {
    esp_gmf_cache_delete(gmf_vad->cache);
    gmf_vad->cache = NULL;
  }
  ESP_LOGD(TAG, "Destroyed");
  esp_gmf_audio_el_deinit(self);
  esp_gmf_oal_free(self);

  return ESP_GMF_ERR_OK;
}

static esp_gmf_job_err_t gmf_vad_open(esp_gmf_audio_element_handle_t self, void *para) {
  gmf_vad_t *gmf_vad = (gmf_vad_t *) self;
  esp_gmf_vad_cfg_t *cfg = OBJ_GET_CFG(self);
  esp_gmf_err_t cfg_ret = gmf_vad_check_config(cfg);
  if (cfg_ret != ESP_GMF_ERR_OK) {
    return ESP_GMF_JOB_ERR_FAIL;
  }

#if CONFIG_SR_VADN_VADNET1_MEDIUM
  const char *partition_label = gmf_vad_get_partition_label(cfg);
  const int min_speech_ms = gmf_vad_get_min_speech_ms(cfg);
  const int min_noise_ms = gmf_vad_get_min_noise_ms(cfg);
  gmf_vad->models = esp_srmodel_init(partition_label);
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->models, { return ESP_GMF_JOB_ERR_FAIL; });
  const char *model_name = gmf_vad_get_model_name(gmf_vad, cfg);
  if (model_name == NULL) {
    ESP_LOGE(TAG, "Failed to find VADNet model in partition `%s`", partition_label);
    goto __failed;
  }
  if (esp_srmodel_exists(gmf_vad->models, (char *) model_name) < 0) {
    ESP_LOGE(TAG, "VADNet model not found in partition `%s`: %s", partition_label, model_name);
    goto __failed;
  }

  gmf_vad->vadn_iface = esp_vadn_handle_from_name(model_name);
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vadn_iface, { goto __failed; });
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vadn_iface->create, { goto __failed; });
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vadn_iface->get_samp_chunksize, { goto __failed; });
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vadn_iface->get_samp_rate, { goto __failed; });
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vadn_iface->detect, { goto __failed; });

  ESP_LOGI(TAG, "VADNet config: model_name=%s, vad_mode=%d, min_speech_ms=%d, min_noise_ms=%d", model_name,
           cfg->vad_mode, min_speech_ms, min_noise_ms);
  gmf_vad->vadn_model = gmf_vad->vadn_iface->create(model_name, cfg->vad_mode, 1, min_speech_ms, min_noise_ms);
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vadn_model, { goto __failed; });

  int sample_rate = gmf_vad->vadn_iface->get_samp_rate(gmf_vad->vadn_model);
  if (sample_rate != cfg->sample_rate) {
    ESP_LOGE(TAG, "VADNet sample rate mismatch, cfg: %d, model: %d", cfg->sample_rate, sample_rate);
    goto __failed;
  }
  if ((gmf_vad->vadn_iface->get_channel_num != NULL) &&
      (gmf_vad->vadn_iface->get_channel_num(gmf_vad->vadn_model) != 1)) {
    ESP_LOGE(TAG, "Only single-channel VADNet input is supported");
    goto __failed;
  }

  gmf_vad->frame_samples = gmf_vad->vadn_iface->get_samp_chunksize(gmf_vad->vadn_model);
  if (gmf_vad->frame_samples == 0) {
    ESP_LOGE(TAG, "Invalid VADNet frame size");
    goto __failed;
  }
  int backend_frame_ms = (int) ((gmf_vad->frame_samples * 1000U) / (uint32_t) sample_rate);
  cfg->frame_ms = backend_frame_ms;
#else
  gmf_vad->vad_handle = vad_create(cfg->vad_mode);
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vad_handle, { return ESP_GMF_JOB_ERR_FAIL; });

  gmf_vad->frame_samples = cfg->sample_rate * cfg->frame_ms / 1000;
#endif
  gmf_vad->frame_bytes = gmf_vad->frame_samples * ESP_VAD_BYTES_PER_SAMPLE;
  gmf_vad->last_state = VAD_SILENCE;
  gmf_vad->state_valid = false;

  esp_gmf_cache_new(gmf_vad->frame_bytes, &gmf_vad->cache);
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->cache, { goto __failed; });

  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, gmf_vad->frame_bytes);
  ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                    ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, gmf_vad->frame_bytes);
#if CONFIG_SR_VADN_VADNET1_MEDIUM
  ESP_LOGI(TAG, "GMF VAD open, backend: vadnet, sample_rate: %d, frame_ms: %d, frame_bytes: %lu", cfg->sample_rate,
           cfg->frame_ms, (unsigned long) gmf_vad->frame_bytes);
#else
  ESP_LOGI(TAG, "GMF VAD open, backend: webrtc, sample_rate: %d, frame_ms: %d, frame_bytes: %lu", cfg->sample_rate,
           cfg->frame_ms, (unsigned long) gmf_vad->frame_bytes);
#endif

  esp_gmf_info_sound_t snd_info = {0};
  snd_info.sample_rates = cfg->sample_rate;
  snd_info.bits = 16;
  snd_info.channels = 1;
  esp_gmf_element_notify_snd_info(self, &snd_info);
  return ESP_GMF_JOB_ERR_OK;

__failed:
  gmf_vad_release_backend(gmf_vad);
  return ESP_GMF_JOB_ERR_FAIL;
}

static esp_gmf_job_err_t gmf_vad_process(esp_gmf_audio_element_handle_t self, void *para) {
  int ret = ESP_GMF_JOB_ERR_OK;
  gmf_vad_t *gmf_vad = (gmf_vad_t *) self;
  esp_gmf_vad_cfg_t *cfg = OBJ_GET_CFG(self);
  esp_gmf_port_handle_t in_port = ESP_GMF_ELEMENT_GET(self)->in;
  esp_gmf_port_handle_t out_port = ESP_GMF_ELEMENT_GET(self)->out;
  esp_gmf_payload_t *out_load = NULL;
  esp_gmf_payload_t *cache_load = NULL;
  esp_gmf_err_io_t load_ret = 0;
  bool need_load = false;

  ESP_GMF_NULL_CHECK(TAG, in_port, { return ESP_GMF_JOB_ERR_FAIL; });
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->cache, { return ESP_GMF_JOB_ERR_FAIL; });
#if CONFIG_SR_VADN_VADNET1_MEDIUM
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vadn_iface, { return ESP_GMF_JOB_ERR_FAIL; });
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vadn_model, { return ESP_GMF_JOB_ERR_FAIL; });
#else
  ESP_GMF_NULL_CHECK(TAG, gmf_vad->vad_handle, { return ESP_GMF_JOB_ERR_FAIL; });
#endif

  esp_gmf_cache_ready_for_load(gmf_vad->cache, &need_load);
  if (need_load) {
    load_ret = esp_gmf_port_acquire_in(in_port, &gmf_vad->in_load, gmf_vad->frame_bytes, in_port->wait_ticks);
    ESP_GMF_PORT_ACQUIRE_IN_CHECK(TAG, load_ret, ret, { goto __quit; });
    esp_gmf_cache_load(gmf_vad->cache, gmf_vad->in_load);
  }
  esp_gmf_cache_acquire(gmf_vad->cache, gmf_vad->frame_bytes, &cache_load);
  if (cache_load->valid_size != gmf_vad->frame_bytes) {
    if (cache_load->is_done == true) {
      ret = ESP_GMF_JOB_ERR_DONE;
    } else {
      ret = ESP_GMF_JOB_ERR_CONTINUE;
    }
    ESP_LOGD(TAG, "Return Continue, size:%u", cache_load->valid_size);
    goto __quit;
  }

#if CONFIG_SR_VADN_VADNET1_MEDIUM
  vad_state_t vad_state = gmf_vad->vadn_iface->detect(gmf_vad->vadn_model, (int16_t *) cache_load->buf);
#else
  vad_state_t vad_state =
      vad_process(gmf_vad->vad_handle, (int16_t *) cache_load->buf, cfg->sample_rate, cfg->frame_ms);
#endif /* CONFIG_SR_VADN_VADNET1_MEDIUM */
  if ((cfg->result_callback) && ((gmf_vad->state_valid == false) || (vad_state != gmf_vad->last_state))) {
    cfg->result_callback(vad_state, cfg->ctx);
  }
  gmf_vad->last_state = vad_state;
  gmf_vad->state_valid = true;

  if (out_port) {
    load_ret = esp_gmf_port_acquire_out(out_port, &out_load, gmf_vad->frame_bytes, ESP_GMF_MAX_DELAY);
    ESP_GMF_PORT_ACQUIRE_OUT_CHECK(TAG, load_ret, ret, { goto __quit; });
    if (out_load->buf_length < gmf_vad->frame_bytes) {
      ret = ESP_GMF_JOB_ERR_FAIL;
      ESP_LOGE(TAG, "Output buffer is not enough");
      goto __quit;
    }
    memcpy(out_load->buf, cache_load->buf, gmf_vad->frame_bytes);
    out_load->valid_size = gmf_vad->frame_bytes;
    out_load->is_done = cache_load->is_done;
  }

  esp_gmf_cache_ready_for_load(gmf_vad->cache, &need_load);
  if (need_load == false) {
    ret = ESP_GMF_JOB_ERR_TRUNCATE;
  } else {
    ret = (cache_load->is_done == true ? ESP_GMF_JOB_ERR_DONE : ESP_GMF_JOB_ERR_OK);
  }

__quit:
  if (out_load) {
    esp_gmf_port_release_out(out_port, out_load, ESP_GMF_MAX_DELAY);
  }
  if (gmf_vad->in_load && (ret != ESP_GMF_JOB_ERR_TRUNCATE)) {
    esp_gmf_port_release_in(in_port, gmf_vad->in_load, ESP_GMF_MAX_DELAY);
    gmf_vad->in_load = NULL;
  }
  if (cache_load) {
    esp_gmf_cache_release(gmf_vad->cache, cache_load);
  }
  return ret;
}

static esp_gmf_job_err_t gmf_vad_close(esp_gmf_audio_element_handle_t self, void *para) {
  gmf_vad_t *gmf_vad = (gmf_vad_t *) self;
  gmf_vad_release_backend(gmf_vad);
  if (gmf_vad->cache) {
    esp_gmf_cache_delete(gmf_vad->cache);
    gmf_vad->cache = NULL;
  }
  gmf_vad->in_load = NULL;
  gmf_vad->last_state = VAD_SILENCE;
  gmf_vad->state_valid = false;
  return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_err_t _load_vad_caps_func(esp_gmf_element_handle_t handle) {
  esp_gmf_cap_t *caps = NULL;
  esp_gmf_cap_t vad_caps = {0};
  vad_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_VAD;
  vad_caps.attr_fun = NULL;
  int ret = esp_gmf_cap_append(&caps, &vad_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create capability");

  esp_gmf_element_t *el = (esp_gmf_element_t *) handle;
  el->caps = caps;
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_vad_init(esp_gmf_vad_cfg_t *config, esp_gmf_obj_handle_t *handle) {
  esp_gmf_err_t ret = ESP_GMF_ERR_OK;
  ret = gmf_vad_check_config(config);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Invalid VAD configuration");
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;

  gmf_vad_t *gmf_vad = esp_gmf_oal_calloc(1, sizeof(gmf_vad_t));
  ESP_GMF_MEM_VERIFY(
      TAG, gmf_vad, { return ESP_GMF_ERR_MEMORY_LACK; }, "vad", sizeof(gmf_vad_t));
  esp_gmf_obj_t *obj = (esp_gmf_obj_t *) gmf_vad;
  obj->new_obj = gmf_vad_new;
  obj->del_obj = gmf_vad_destroy;

  esp_gmf_vad_cfg_t *obj_cfg = esp_gmf_oal_calloc(1, sizeof(esp_gmf_vad_cfg_t));
  ESP_GMF_NULL_CHECK(TAG, obj_cfg, {
    ret = ESP_GMF_ERR_MEMORY_LACK;
    goto __failed;
  });
  memcpy(obj_cfg, config, sizeof(esp_gmf_vad_cfg_t));
  ret = esp_gmf_obj_set_config(obj, obj_cfg, sizeof(esp_gmf_vad_cfg_t));
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ configuration");
  ret = esp_gmf_obj_set_tag(obj, "ai_vad");
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ tag");
  esp_gmf_element_cfg_t el_cfg = {0};
  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(el_cfg.in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
  ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(el_cfg.out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                    ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
  el_cfg.dependency = true;
  ret = esp_gmf_audio_el_init(gmf_vad, &el_cfg);
  if (ret != ESP_GMF_ERR_OK) {
    return ret;
  }

  ESP_GMF_ELEMENT_GET(gmf_vad)->ops.open = gmf_vad_open;
  ESP_GMF_ELEMENT_GET(gmf_vad)->ops.process = gmf_vad_process;
  ESP_GMF_ELEMENT_GET(gmf_vad)->ops.close = gmf_vad_close;
  ESP_GMF_ELEMENT_GET(gmf_vad)->ops.event_receiver = gmf_vad_received_event_handler;
  ESP_GMF_ELEMENT_GET(gmf_vad)->ops.load_caps = _load_vad_caps_func;

  *handle = obj;
  return ESP_GMF_ERR_OK;
__failed:
  esp_gmf_obj_delete(obj);
  return ret;
}

esp_gmf_err_t esp_gmf_vad_set_result_cb(esp_gmf_element_handle_t self, esp_gmf_vad_result_callback_t result_callback,
                                        void *ctx) {
  ESP_GMF_NULL_CHECK(TAG, self, { return ESP_GMF_ERR_INVALID_ARG; });
  esp_gmf_vad_cfg_t *cfg = OBJ_GET_CFG(self);
  ESP_GMF_NULL_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
  cfg->result_callback = result_callback;
  cfg->ctx = ctx;
  return ESP_GMF_ERR_OK;
}
