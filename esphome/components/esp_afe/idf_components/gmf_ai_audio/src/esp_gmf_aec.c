/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_afe_config.h"
#include "esp_afe_aec.h"
#include "esp_gmf_payload.h"
#include "esp_gmf_aec.h"
#include "esp_gmf_audio_element.h"
#include "esp_gmf_err.h"
#include "esp_gmf_job.h"
#include "esp_gmf_node.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_cache.h"
#include "esp_gmf_cap.h"
#include "esp_gmf_caps_def.h"

#define ESP_AEC_BYTES_PER_SAMPLE (sizeof(uint16_t))  // ESP AEC processes data at 16 kHz, 16-bit, and single-channel

typedef struct {
  esp_gmf_audio_element_t parent;
  afe_aec_handle_t *aec_handle;
  uint32_t chunk_size;
  uint32_t frame_len;
  esp_gmf_cache_t *cache;
  esp_gmf_payload_t *in_load;
} gmf_aec_t;

static const char *TAG = "GMF_AEC";

static esp_gmf_err_t gmf_aec_new(void *cfg, esp_gmf_obj_handle_t *handle) {
  ESP_GMF_NULL_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;
  esp_gmf_aec_cfg_t *aec_cfg = (esp_gmf_aec_cfg_t *) cfg;
  esp_gmf_obj_handle_t new_obj = NULL;
  esp_gmf_err_t ret = esp_gmf_aec_init(aec_cfg, &new_obj);
  if (ret != ESP_GMF_ERR_OK) {
    return ret;
  }
  *handle = (void *) new_obj;
  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t gmf_aec_received_event_handler(esp_gmf_event_pkt_t *evt, void *ctx) {
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
      memcpy(&info, evt->payload, evt->payload_size);
      ESP_LOGD(TAG, "RECV info, from: %s-%p, next: %p, self: %s-%p, type: %x, state: %s, rate: %d, ch: %d, bits: %d",
               OBJ_GET_TAG(el), el, esp_gmf_node_for_next((esp_gmf_node_t *) el), OBJ_GET_TAG(self), self, evt->type,
               esp_gmf_event_get_state_str(state), info.sample_rates, info.channels, info.bits);
      uint32_t expected_rate = ((esp_gmf_aec_cfg_t *) OBJ_GET_CFG(self))->type == AFE_TYPE_VC_8K ? 8000 : 16000;
      if (info.sample_rates != expected_rate || info.bits != 16) {
        ESP_LOGE(TAG, "Unsupported format, rate: %d, bits: %d", info.sample_rates, info.bits);
        return ESP_GMF_ERR_NOT_SUPPORT;
      }
      esp_gmf_element_set_state(self, ESP_GMF_EVENT_STATE_INITIALIZED);
    }
  }
  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t gmf_aec_destroy(esp_gmf_audio_element_handle_t self) {
  ESP_GMF_NULL_CHECK(TAG, self, { return ESP_GMF_ERR_INVALID_ARG; });

  gmf_aec_t *gmf_aec = (gmf_aec_t *) self;

  if (OBJ_GET_CFG(self)) {
    esp_gmf_oal_free(OBJ_GET_CFG(self));
  }
  if (gmf_aec->aec_handle) {
    afe_aec_destroy(gmf_aec->aec_handle);
    gmf_aec->aec_handle = NULL;
  }
  if (gmf_aec->cache) {
    esp_gmf_cache_delete(gmf_aec->cache);
    gmf_aec->cache = NULL;
  }
  ESP_LOGD(TAG, "Destroyed");
  esp_gmf_audio_el_deinit(self);
  esp_gmf_oal_free(self);

  return ESP_GMF_ERR_OK;
}

static esp_gmf_job_err_t gmf_aec_open(esp_gmf_audio_element_handle_t self, void *para) {
  gmf_aec_t *gmf_aec = (gmf_aec_t *) self;
  esp_gmf_aec_cfg_t *cfg = OBJ_GET_CFG(self);
  gmf_aec->aec_handle = afe_aec_create(cfg->input_format, cfg->filter_len, cfg->type, cfg->mode);
  ESP_GMF_NULL_CHECK(TAG, gmf_aec->aec_handle, { return ESP_GMF_JOB_ERR_FAIL; });

  gmf_aec->chunk_size = afe_aec_get_chunksize(gmf_aec->aec_handle);
  gmf_aec->frame_len = gmf_aec->chunk_size * gmf_aec->aec_handle->pcm_config.total_ch_num * ESP_AEC_BYTES_PER_SAMPLE;
  esp_gmf_cache_new(gmf_aec->frame_len, &gmf_aec->cache);
  ESP_GMF_NULL_CHECK(TAG, gmf_aec->cache, { return ESP_GMF_JOB_ERR_FAIL; });

  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, gmf_aec->frame_len);
  ESP_LOGI(TAG, "GMF AEC open, frame_len: %lu, nch %d, chunksize %lu", gmf_aec->frame_len,
           gmf_aec->aec_handle->pcm_config.total_ch_num, gmf_aec->chunk_size);
  esp_gmf_info_sound_t snd_info = {0};
  snd_info.sample_rates = cfg->type == AFE_TYPE_VC_8K ? 8000 : 16000;
  snd_info.bits = 16;
  snd_info.channels = 1;
  esp_gmf_element_notify_snd_info(self, &snd_info);
  return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_job_err_t gmf_aec_process(esp_gmf_audio_element_handle_t self, void *para) {
  int ret = ESP_GMF_JOB_ERR_OK;
  gmf_aec_t *gmf_aec = (gmf_aec_t *) self;
  esp_gmf_port_handle_t in_port = ESP_GMF_ELEMENT_GET(self)->in;
  esp_gmf_port_handle_t out_port = ESP_GMF_ELEMENT_GET(self)->out;
  esp_gmf_payload_t *out_load = NULL;
  esp_gmf_payload_t *cache_load = NULL;
  esp_gmf_err_io_t load_ret = 0;
  bool need_load = false;

  esp_gmf_cache_ready_for_load(gmf_aec->cache, &need_load);
  if (need_load) {
    load_ret = esp_gmf_port_acquire_in(in_port, &gmf_aec->in_load, gmf_aec->frame_len, in_port->wait_ticks);
    ESP_GMF_PORT_ACQUIRE_IN_CHECK(TAG, load_ret, ret, { goto __quit; });
    esp_gmf_cache_load(gmf_aec->cache, gmf_aec->in_load);
  }
  esp_gmf_cache_acquire(gmf_aec->cache, gmf_aec->frame_len, &cache_load);
  if (cache_load->valid_size != gmf_aec->frame_len) {
    if (cache_load->is_done == true) {
      ret = ESP_GMF_JOB_ERR_DONE;
    } else {
      ret = ESP_GMF_JOB_ERR_CONTINUE;
    }
    ESP_LOGD(TAG, "Return Continue, size:%u", cache_load->valid_size);
    goto __quit;
  }
  load_ret =
      esp_gmf_port_acquire_out(out_port, &out_load, gmf_aec->chunk_size * ESP_AEC_BYTES_PER_SAMPLE, ESP_GMF_MAX_DELAY);
  ESP_GMF_PORT_ACQUIRE_OUT_CHECK(TAG, load_ret, ret, { goto __quit; });
  if (out_load->buf_length < gmf_aec->chunk_size * ESP_AEC_BYTES_PER_SAMPLE) {
    ret = ESP_GMF_JOB_ERR_FAIL;
    ESP_LOGE(TAG, "Output buffer is not enough");
    goto __quit;
  }
  afe_aec_process(gmf_aec->aec_handle, (int16_t *) cache_load->buf, (int16_t *) out_load->buf);
  out_load->valid_size = gmf_aec->chunk_size * ESP_AEC_BYTES_PER_SAMPLE;
  out_load->is_done = cache_load->is_done;
  esp_gmf_cache_ready_for_load(gmf_aec->cache, &need_load);
  if (need_load == false) {
    ret = ESP_GMF_JOB_ERR_TRUNCATE;
  } else {
    ret = (out_load->is_done == true ? ESP_GMF_JOB_ERR_DONE : ESP_GMF_JOB_ERR_OK);
  }

__quit:
  if (out_load) {
    esp_gmf_port_release_out(out_port, out_load, ESP_GMF_MAX_DELAY);
  }
  if (gmf_aec->in_load && (ret != ESP_GMF_JOB_ERR_TRUNCATE)) {
    esp_gmf_port_release_in(in_port, gmf_aec->in_load, ESP_GMF_MAX_DELAY);
    gmf_aec->in_load = NULL;
  }
  if (cache_load) {
    esp_gmf_cache_release(gmf_aec->cache, cache_load);
  }
  return ret;
}

static esp_gmf_job_err_t gmf_aec_close(esp_gmf_audio_element_handle_t self, void *para) {
  gmf_aec_t *gmf_aec = (gmf_aec_t *) self;
  if (gmf_aec->aec_handle) {
    afe_aec_destroy(gmf_aec->aec_handle);
    gmf_aec->aec_handle = NULL;
  }
  if (gmf_aec->cache) {
    esp_gmf_cache_delete(gmf_aec->cache);
    gmf_aec->cache = NULL;
  }
  return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_err_t _load_aec_caps_func(esp_gmf_element_handle_t handle) {
  esp_gmf_cap_t *caps = NULL;
  esp_gmf_cap_t aec_caps = {0};
  aec_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_AEC;
  aec_caps.attr_fun = NULL;
  int ret = esp_gmf_cap_append(&caps, &aec_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create capability");

  esp_gmf_element_t *el = (esp_gmf_element_t *) handle;
  el->caps = caps;
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_aec_init(esp_gmf_aec_cfg_t *config, esp_gmf_obj_handle_t *handle) {
  ESP_GMF_NULL_CHECK(TAG, config, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;
  gmf_aec_t *gmf_aec = esp_gmf_oal_calloc(1, sizeof(gmf_aec_t));
  ESP_GMF_MEM_VERIFY(
      TAG, gmf_aec, { return ESP_GMF_ERR_MEMORY_LACK; }, "aec", sizeof(gmf_aec_t));
  esp_gmf_obj_t *obj = (esp_gmf_obj_t *) gmf_aec;
  obj->new_obj = gmf_aec_new;
  obj->del_obj = gmf_aec_destroy;

  esp_gmf_aec_cfg_t *obj_cfg = esp_gmf_oal_calloc(1, sizeof(esp_gmf_aec_cfg_t));
  memcpy(obj_cfg, config, sizeof(esp_gmf_aec_cfg_t));
  esp_gmf_err_t ret = esp_gmf_obj_set_config(obj, obj_cfg, sizeof(esp_gmf_aec_cfg_t));
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ configuration");
  ret = esp_gmf_obj_set_tag(obj, "ai_aec");
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ tag");
  esp_gmf_element_cfg_t el_cfg = {0};
  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(el_cfg.in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
  ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(el_cfg.out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                    ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
  el_cfg.dependency = true;
  esp_gmf_audio_el_init(gmf_aec, &el_cfg);

  ESP_GMF_ELEMENT_GET(gmf_aec)->ops.open = gmf_aec_open;
  ESP_GMF_ELEMENT_GET(gmf_aec)->ops.process = gmf_aec_process;
  ESP_GMF_ELEMENT_GET(gmf_aec)->ops.close = gmf_aec_close;
  ESP_GMF_ELEMENT_GET(gmf_aec)->ops.event_receiver = gmf_aec_received_event_handler;
  ESP_GMF_ELEMENT_GET(gmf_aec)->ops.load_caps = _load_aec_caps_func;

  *handle = obj;
  return ESP_GMF_ERR_OK;
__failed:
  esp_gmf_obj_delete(obj);
  return ret;
}
