/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdint.h>
#include <string.h>

#include "esp_afe_doa.h"
#include "esp_log.h"
#include "esp_gmf_payload.h"
#include "esp_gmf_doa.h"
#include "esp_gmf_audio_element.h"
#include "esp_gmf_err.h"
#include "esp_gmf_job.h"
#include "esp_gmf_node.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_cache.h"
#include "esp_gmf_cap.h"
#include "esp_gmf_caps_def.h"

#define ESP_DOA_BYTES_PER_SAMPLE (sizeof(uint16_t))  // ESP DOA processes data at 16 kHz, 16-bit, and single-channel

/**
 * @brief  Internal state of the DOA GMF audio element
 */
typedef struct {
  esp_gmf_audio_element_t parent; /*!< GMF audio element base */
  doa_handle_t *doa_handle;       /*!< ESP-SR DOA algorithm handle */
  esp_gmf_cache_t *cache;         /*!< Input frame cache for fixed-size blocks */
  esp_gmf_payload_t *in_load;     /*!< Current acquired input payload from port */
  int16_t *mic_left;              /*!< Left microphone PCM after channel extraction */
  int16_t *mic_right;             /*!< Right microphone PCM after channel extraction */
  uint32_t bytes_per_iter;        /*!< Interleaved PCM bytes per DOA result, derived from frame_ms */
  uint8_t m_idx[2];               /*!< Channel indices of the two 'M' slots in input_format */
} gmf_doa_t;

static const char *TAG = "GMF_DOA";

static inline void extract_channels(gmf_doa_t *gmf_doa, int16_t *in_buf) {
  esp_gmf_doa_cfg_t *doa_cfg = OBJ_GET_CFG(gmf_doa);
  const char *input_format = doa_cfg->input_format;
  size_t ch_num = strlen(input_format);

  size_t samples = gmf_doa->bytes_per_iter / sizeof(int16_t);
  int idx = 0;
  for (size_t i = 0; i < samples; i += ch_num, idx++) {
    gmf_doa->mic_left[idx] = in_buf[i + gmf_doa->m_idx[0]];
    gmf_doa->mic_right[idx] = in_buf[i + gmf_doa->m_idx[1]];
  };
}

static esp_gmf_err_t gmf_doa_new(void *cfg, esp_gmf_obj_handle_t *handle) {
  ESP_GMF_NULL_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;
  esp_gmf_doa_cfg_t *doa_cfg = (esp_gmf_doa_cfg_t *) cfg;
  esp_gmf_obj_handle_t new_obj = NULL;
  ESP_LOGI(TAG, "gmf_doa_new - Config - Sample rate: %d, Resolution: %f, d_mics: %f", doa_cfg->sample_rate,
           doa_cfg->resolution, doa_cfg->d_mics);
  esp_gmf_err_t ret = esp_gmf_doa_init(doa_cfg, &new_obj);
  if (ret != ESP_GMF_ERR_OK) {
    return ret;
  }
  *handle = (void *) new_obj;
  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t gmf_doa_received_event_handler(esp_gmf_event_pkt_t *evt, void *ctx) {
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
      esp_gmf_element_set_state(self, ESP_GMF_EVENT_STATE_INITIALIZED);
    }
  }
  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t gmf_doa_destroy(esp_gmf_audio_element_handle_t self) {
  ESP_GMF_NULL_CHECK(TAG, self, { return ESP_GMF_ERR_INVALID_ARG; });

  gmf_doa_t *gmf_doa = (gmf_doa_t *) self;

  if (OBJ_GET_CFG(self)) {
    esp_gmf_oal_free(OBJ_GET_CFG(self));
  }
  if (gmf_doa->doa_handle) {
    esp_doa_destroy(gmf_doa->doa_handle);
    gmf_doa->doa_handle = NULL;
  }
  if (gmf_doa->cache) {
    esp_gmf_cache_delete(gmf_doa->cache);
    gmf_doa->cache = NULL;
  }
  if (gmf_doa->mic_left) {
    esp_gmf_oal_free(gmf_doa->mic_left);
    gmf_doa->mic_left = NULL;
  }
  if (gmf_doa->mic_right) {
    esp_gmf_oal_free(gmf_doa->mic_right);
    gmf_doa->mic_right = NULL;
  }
  ESP_LOGD(TAG, "Destroyed");
  esp_gmf_audio_el_deinit(self);
  esp_gmf_oal_free(self);

  return ESP_GMF_ERR_OK;
}

static inline esp_gmf_job_err_t doa_validate_microphone_format_check(const char *input_format, size_t format_len,
                                                                     uint8_t *m_idx) {
  ESP_GMF_NULL_CHECK(TAG, input_format, { return ESP_GMF_JOB_ERR_FAIL; });
  ESP_GMF_NULL_CHECK(TAG, m_idx, { return ESP_GMF_JOB_ERR_FAIL; });

  int mic_count = 0;
  for (size_t i = 0; i < format_len; i++) {
    if (input_format[i] == 'M') {
      if (mic_count < 2) {
        m_idx[mic_count] = (uint8_t) i;
      }
      mic_count++;
    }
  }
  if (mic_count != 2) {
    ESP_LOGE(TAG, "Input format is not valid - expected exactly 2 microphones, found %d", mic_count);
    return ESP_GMF_JOB_ERR_FAIL;
  }
  return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_job_err_t gmf_doa_open(esp_gmf_audio_element_handle_t self, void *para) {
  int ret = ESP_GMF_JOB_ERR_OK;
  gmf_doa_t *gmf_doa = (gmf_doa_t *) self;
  esp_gmf_doa_cfg_t *doa_cfg = OBJ_GET_CFG(self);

  const char *input_format = doa_cfg->input_format;
  const size_t format_len = strlen(input_format);

  if ((doa_cfg->frame_ms == 0) || (doa_cfg->sample_rate <= 0) || (format_len == 0)) {
    ESP_LOGE(TAG, "Invalid config: frame_ms=%u, sample_rate=%d, format_len=%zu", doa_cfg->frame_ms,
             doa_cfg->sample_rate, format_len);
    return ESP_GMF_JOB_ERR_FAIL;
  }
  const uint32_t samples_per_channel = (uint32_t) doa_cfg->frame_ms * (uint32_t) doa_cfg->sample_rate / 1000U;
  const uint32_t bytes_per_channel = samples_per_channel * (uint32_t) ESP_DOA_BYTES_PER_SAMPLE;
  gmf_doa->bytes_per_iter = bytes_per_channel * (uint32_t) format_len;

  ret = doa_validate_microphone_format_check(input_format, format_len, gmf_doa->m_idx);
  if (ret != ESP_GMF_JOB_ERR_OK) {
    return ret;
  }
  gmf_doa->doa_handle =
      esp_doa_create(doa_cfg->sample_rate, doa_cfg->resolution, doa_cfg->d_mics, (int) samples_per_channel);
  if (gmf_doa->doa_handle == NULL) {
    ESP_LOGE(TAG, "Failed to create DOA handle");
    return ESP_GMF_JOB_ERR_FAIL;
  }
  esp_gmf_cache_new(gmf_doa->bytes_per_iter, &gmf_doa->cache);
  ESP_GMF_NULL_CHECK(TAG, gmf_doa->cache, {
    ret = ESP_GMF_JOB_ERR_FAIL;
    goto __quit;
  });

  gmf_doa->mic_left = (int16_t *) esp_gmf_oal_calloc(1, bytes_per_channel);
  ESP_GMF_NULL_CHECK(TAG, gmf_doa->mic_left, {
    ret = ESP_GMF_JOB_ERR_FAIL;
    goto __quit;
  });
  gmf_doa->mic_right = (int16_t *) esp_gmf_oal_calloc(1, bytes_per_channel);
  ESP_GMF_NULL_CHECK(TAG, gmf_doa->mic_right, {
    ret = ESP_GMF_JOB_ERR_FAIL;
    goto __quit;
  });

  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, gmf_doa->bytes_per_iter);
  esp_gmf_info_sound_t snd_info = {0};
  snd_info.sample_rates = doa_cfg->sample_rate;
  snd_info.bits = 16;
  snd_info.channels = format_len;
  esp_gmf_element_notify_snd_info(self, &snd_info);
  return ESP_GMF_JOB_ERR_OK;

__quit:
  if (gmf_doa->doa_handle) {
    esp_doa_destroy(gmf_doa->doa_handle);
    gmf_doa->doa_handle = NULL;
  }
  if (gmf_doa->cache) {
    esp_gmf_cache_delete(gmf_doa->cache);
    gmf_doa->cache = NULL;
  }
  if (gmf_doa->mic_left) {
    esp_gmf_oal_free(gmf_doa->mic_left);
    gmf_doa->mic_left = NULL;
  }
  if (gmf_doa->mic_right) {
    esp_gmf_oal_free(gmf_doa->mic_right);
    gmf_doa->mic_right = NULL;
  }
  return ret;
}

static esp_gmf_job_err_t gmf_doa_process(esp_gmf_audio_element_handle_t self, void *para) {
  int ret = ESP_GMF_JOB_ERR_OK;
  gmf_doa_t *gmf_doa = (gmf_doa_t *) self;
  esp_gmf_doa_cfg_t *doa_cfg = OBJ_GET_CFG(self);
  esp_gmf_port_handle_t in_port = ESP_GMF_ELEMENT_GET(self)->in;
  esp_gmf_payload_t *cache_load = NULL;
  esp_gmf_err_io_t load_ret = 0;
  bool need_load = false;

  esp_gmf_cache_ready_for_load(gmf_doa->cache, &need_load);
  if (need_load) {
    load_ret = esp_gmf_port_acquire_in(in_port, &gmf_doa->in_load, gmf_doa->bytes_per_iter, in_port->wait_ticks);
    ESP_GMF_PORT_ACQUIRE_IN_CHECK(TAG, load_ret, ret, { goto __quit; });
    esp_gmf_cache_load(gmf_doa->cache, gmf_doa->in_load);
  }
  esp_gmf_cache_acquire(gmf_doa->cache, gmf_doa->bytes_per_iter, &cache_load);
  if (cache_load->valid_size != gmf_doa->bytes_per_iter) {
    if (cache_load->is_done == true) {
      ret = ESP_GMF_JOB_ERR_DONE;
    } else {
      ret = ESP_GMF_JOB_ERR_CONTINUE;
    }
    ESP_LOGW(TAG, "Return Continue, size:%u", cache_load->valid_size);
    if (gmf_doa->in_load && (ret != ESP_GMF_JOB_ERR_TRUNCATE)) {
      esp_gmf_port_release_in(in_port, gmf_doa->in_load, ESP_GMF_MAX_DELAY);
      gmf_doa->in_load = NULL;
    }
    if (cache_load) {
      esp_gmf_cache_release(gmf_doa->cache, cache_load);
    }
    return ret;
  }

  extract_channels(gmf_doa, (int16_t *) cache_load->buf);

  float doa_result = esp_doa_process(gmf_doa->doa_handle, gmf_doa->mic_left, gmf_doa->mic_right);
  ESP_LOGD(TAG, "raw doa result: %f", doa_result);
  if (doa_cfg->result_callback) {
    doa_cfg->result_callback(doa_result, doa_cfg->ctx);
  }
  esp_gmf_cache_ready_for_load(gmf_doa->cache, &need_load);
  if (need_load == false) {
    ret = ESP_GMF_JOB_ERR_TRUNCATE;
  } else {
    ret = (cache_load->is_done == true ? ESP_GMF_JOB_ERR_DONE : ESP_GMF_JOB_ERR_OK);
  }
__quit:
  if (gmf_doa->in_load && (ret != ESP_GMF_JOB_ERR_TRUNCATE)) {
    esp_gmf_port_release_in(in_port, gmf_doa->in_load, ESP_GMF_MAX_DELAY);
    gmf_doa->in_load = NULL;
  }
  if (cache_load) {
    esp_gmf_cache_release(gmf_doa->cache, cache_load);
  }
  return ret;
}

static esp_gmf_job_err_t gmf_doa_close(esp_gmf_audio_element_handle_t self, void *para) {
  gmf_doa_t *gmf_doa = (gmf_doa_t *) self;
  if (gmf_doa->doa_handle) {
    esp_doa_destroy(gmf_doa->doa_handle);
    gmf_doa->doa_handle = NULL;
  }
  if (gmf_doa->cache) {
    esp_gmf_cache_delete(gmf_doa->cache);
    gmf_doa->cache = NULL;
  }
  if (gmf_doa->mic_left) {
    esp_gmf_oal_free(gmf_doa->mic_left);
    gmf_doa->mic_left = NULL;
  }
  if (gmf_doa->mic_right) {
    esp_gmf_oal_free(gmf_doa->mic_right);
    gmf_doa->mic_right = NULL;
  }
  gmf_doa->in_load = NULL;
  return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_err_t _load_doa_caps_func(esp_gmf_element_handle_t handle) {
  esp_gmf_cap_t *caps = NULL;
  esp_gmf_cap_t doa_caps = {0};
  doa_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_DOA;
  doa_caps.attr_fun = NULL;
  int ret = esp_gmf_cap_append(&caps, &doa_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create capability");

  esp_gmf_element_t *el = (esp_gmf_element_t *) handle;
  el->caps = caps;
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_doa_init(esp_gmf_doa_cfg_t *config, esp_gmf_obj_handle_t *handle) {
  esp_gmf_err_t ret = ESP_GMF_ERR_OK;

  ESP_GMF_NULL_CHECK(TAG, config, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;
  gmf_doa_t *gmf_doa = esp_gmf_oal_calloc(1, sizeof(gmf_doa_t));
  ESP_GMF_MEM_VERIFY(
      TAG, gmf_doa, { return ESP_GMF_ERR_MEMORY_LACK; }, "doa", sizeof(gmf_doa_t));
  esp_gmf_obj_t *obj = (esp_gmf_obj_t *) gmf_doa;
  obj->new_obj = gmf_doa_new;
  obj->del_obj = gmf_doa_destroy;

  esp_gmf_doa_cfg_t *obj_cfg = esp_gmf_oal_calloc(1, sizeof(esp_gmf_doa_cfg_t));
  ESP_GMF_NULL_CHECK(TAG, obj_cfg, {
    ret = ESP_GMF_ERR_MEMORY_LACK;
    goto __failed;
  });
  memcpy(obj_cfg, config, sizeof(esp_gmf_doa_cfg_t));

  ESP_LOGI(TAG, "esp_gmf_doa_init - Config - Sample rate: %d, Resolution: %f, d_mics: %f", obj_cfg->sample_rate,
           obj_cfg->resolution, obj_cfg->d_mics);
  ret = esp_gmf_obj_set_config(obj, obj_cfg, sizeof(esp_gmf_doa_cfg_t));
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ configuration");
  ret = esp_gmf_obj_set_tag(obj, "ai_doa");
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ tag");
  esp_gmf_element_cfg_t el_cfg = {0};
  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(el_cfg.in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
  el_cfg.dependency = true;
  ret = esp_gmf_audio_el_init(gmf_doa, &el_cfg);
  if (ret != ESP_GMF_ERR_OK) {
    return ret;
  }

  ESP_GMF_ELEMENT_GET(gmf_doa)->ops.open = gmf_doa_open;
  ESP_GMF_ELEMENT_GET(gmf_doa)->ops.process = gmf_doa_process;
  ESP_GMF_ELEMENT_GET(gmf_doa)->ops.close = gmf_doa_close;
  ESP_GMF_ELEMENT_GET(gmf_doa)->ops.event_receiver = gmf_doa_received_event_handler;
  ESP_GMF_ELEMENT_GET(gmf_doa)->ops.load_caps = _load_doa_caps_func;

  *handle = obj;
  return ESP_GMF_ERR_OK;
__failed:
  esp_gmf_obj_delete(obj);
  return ret;
}

esp_gmf_err_t esp_gmf_doa_set_result_cb(esp_gmf_element_handle_t self, esp_gmf_doa_result_callback_t result_callback,
                                        void *ctx) {
  ESP_GMF_NULL_CHECK(TAG, self, { return ESP_GMF_ERR_INVALID_ARG; });
  esp_gmf_doa_cfg_t *cfg = OBJ_GET_CFG(self);
  ESP_GMF_NULL_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
  cfg->result_callback = result_callback;
  cfg->ctx = ctx;
  return ESP_GMF_ERR_OK;
}
