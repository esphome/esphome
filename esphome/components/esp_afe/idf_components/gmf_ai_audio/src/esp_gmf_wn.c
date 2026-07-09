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
#include "esp_types.h"
#include "esp_gmf_audio_element.h"
#include "esp_gmf_cache.h"
#include "esp_gmf_cap.h"
#include "esp_gmf_caps_def.h"
#include "esp_gmf_err.h"
#include "esp_gmf_job.h"
#include "esp_gmf_node.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_payload.h"

#include "esp_wn_models.h"
#include "esp_gmf_wn.h"
#include "esp_gmf_ch_sort.h"

#define ESP_WN_BYTES_PER_SAMPLE (sizeof(uint16_t))

typedef struct {
  esp_gmf_audio_element_t parent;
  esp_wn_iface_t *wn_iface;
  model_iface_data_t *model_data;
  uint32_t sample_rate;
  uint32_t ch;
  uint32_t chunk_size;
  uint32_t frame_len;
  esp_gmf_cache_t *cache;
  esp_gmf_payload_t *in_load;
} gmf_wn_t;

static const char *TAG = "GMF_WN";

static esp_gmf_err_t gmf_wn_new(void *cfg, esp_gmf_obj_handle_t *handle) {
  ESP_GMF_NULL_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;
  return esp_gmf_wn_init((esp_gmf_wn_cfg_t *) cfg, (esp_gmf_element_handle_t *) handle);
}

static esp_gmf_err_t gmf_wn_received_event_handler(esp_gmf_event_pkt_t *evt, void *ctx) {
  ESP_GMF_NULL_CHECK(TAG, evt, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, ctx, { return ESP_GMF_ERR_INVALID_ARG; });
  esp_gmf_element_handle_t self = (esp_gmf_element_handle_t) ctx;
  esp_gmf_element_handle_t el = evt->from;
  esp_gmf_event_state_t state = ESP_GMF_EVENT_STATE_NONE;
  esp_gmf_element_get_state(self, &state);
  if (evt->sub == ESP_GMF_INFO_SOUND) {
    esp_gmf_info_sound_t info = {0};
    memcpy(&info, evt->payload, evt->payload_size);
    ESP_LOGD(TAG, "RECV info, from: %s-%p, next: %p, self: %s-%p, type: %x, state: %s, rate: %d, ch: %d, bits: %d",
             OBJ_GET_TAG(el), el, esp_gmf_node_for_next((esp_gmf_node_t *) el), OBJ_GET_TAG(self), self, evt->type,
             esp_gmf_event_get_state_str(state), info.sample_rates, info.channels, info.bits);
    if (info.sample_rates != 16000 || info.bits != 16) {
      ESP_LOGE(TAG, "Unsupported format, rate: %d, bits: %d", info.sample_rates, info.bits);
      return ESP_GMF_ERR_NOT_SUPPORT;
    }
  }
  if (state == ESP_GMF_EVENT_STATE_NONE) {
    esp_gmf_element_set_state(self, ESP_GMF_EVENT_STATE_INITIALIZED);
  }

  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t gmf_wn_destroy(esp_gmf_obj_handle_t self) {
  ESP_GMF_NULL_CHECK(TAG, self, { return ESP_GMF_ERR_INVALID_ARG; });

  gmf_wn_t *gmf_wn = (gmf_wn_t *) self;

  if (OBJ_GET_CFG(self)) {
    esp_gmf_oal_free(OBJ_GET_CFG(self));
  }
  if (gmf_wn->wn_iface && gmf_wn->model_data) {
    gmf_wn->wn_iface->destroy(gmf_wn->model_data);
    gmf_wn->model_data = NULL;
  }
  if (gmf_wn->cache) {
    esp_gmf_cache_delete(gmf_wn->cache);
    gmf_wn->cache = NULL;
  }
  ESP_LOGD(TAG, "Destroyed");
  esp_gmf_audio_el_deinit(self);
  esp_gmf_oal_free(self);

  return ESP_GMF_ERR_OK;
}

static esp_gmf_job_err_t gmf_wn_open(esp_gmf_audio_element_handle_t self, void *para) {
  gmf_wn_t *gmf_wn = (gmf_wn_t *) self;
  esp_gmf_wn_cfg_t *cfg = OBJ_GET_CFG(self);
  char *model_name = esp_srmodel_filter(cfg->models, ESP_WN_PREFIX, NULL);
  ESP_GMF_NULL_CHECK(TAG, model_name, { return ESP_GMF_JOB_ERR_FAIL; });
  gmf_wn->wn_iface = (esp_wn_iface_t *) esp_wn_handle_from_name(model_name);
  ESP_GMF_NULL_CHECK(TAG, gmf_wn->wn_iface, { return ESP_GMF_JOB_ERR_FAIL; });
  gmf_wn->model_data = gmf_wn->wn_iface->create(model_name, cfg->det_mode);
  ESP_GMF_NULL_CHECK(TAG, gmf_wn->model_data, { return ESP_GMF_JOB_ERR_FAIL; });

  int algo_ch_num = gmf_wn->wn_iface->get_channel_num(gmf_wn->model_data);
  gmf_wn->ch = strlen(cfg->input_format);
  if (algo_ch_num > gmf_wn->ch) {
    ESP_LOGE(TAG, "Input channel number is less than needed");
    return ESP_GMF_JOB_ERR_FAIL;
  }
  gmf_wn->sample_rate = gmf_wn->wn_iface->get_samp_rate(gmf_wn->model_data);
  gmf_wn->chunk_size = gmf_wn->wn_iface->get_samp_chunksize(gmf_wn->model_data);
  gmf_wn->frame_len = gmf_wn->chunk_size * gmf_wn->ch * ESP_WN_BYTES_PER_SAMPLE;

  esp_gmf_cache_new(gmf_wn->frame_len, &gmf_wn->cache);
  ESP_GMF_NULL_CHECK(TAG, gmf_wn->cache, { return ESP_GMF_JOB_ERR_FAIL; });

  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, gmf_wn->frame_len);
  ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                    ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE,
                                    gmf_wn->chunk_size * ESP_WN_BYTES_PER_SAMPLE);
  ESP_LOGI(TAG, "Open, frame_len: %" PRIu32 ", ch %" PRIu32 ", chunksize %" PRIu32, gmf_wn->frame_len, gmf_wn->ch,
           gmf_wn->chunk_size);
  esp_gmf_info_sound_t snd_info = {0};
  snd_info.sample_rates = 16000;
  snd_info.bits = 16;
  snd_info.channels = 1;
  esp_gmf_element_notify_snd_info(self, &snd_info);
  return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_job_err_t gmf_wn_process(esp_gmf_audio_element_handle_t self, void *para) {
  int ret = ESP_GMF_JOB_ERR_OK;
  gmf_wn_t *gmf_wn = (gmf_wn_t *) self;
  esp_gmf_port_handle_t in_port = ESP_GMF_ELEMENT_GET(self)->in;
  esp_gmf_port_handle_t out_port = ESP_GMF_ELEMENT_GET(self)->out;
  esp_gmf_payload_t *out_load = NULL;
  esp_gmf_payload_t *cache_load = NULL;
  esp_gmf_err_io_t load_ret = 0;
  esp_gmf_wn_cfg_t *cfg = OBJ_GET_CFG(self);
  bool need_load = false;

  esp_gmf_cache_ready_for_load(gmf_wn->cache, &need_load);
  if (need_load) {
    load_ret = esp_gmf_port_acquire_in(in_port, &gmf_wn->in_load, gmf_wn->frame_len, in_port->wait_ticks);
    ESP_GMF_PORT_ACQUIRE_IN_CHECK(TAG, load_ret, ret, { goto __quit; });
    esp_gmf_cache_load(gmf_wn->cache, gmf_wn->in_load);
  }
  esp_gmf_cache_acquire(gmf_wn->cache, gmf_wn->frame_len, &cache_load);
  if (cache_load->valid_size != gmf_wn->frame_len) {
    if (cache_load->is_done == true) {
      ret = ESP_GMF_JOB_ERR_DONE;
    } else {
      ret = ESP_GMF_JOB_ERR_CONTINUE;
      ESP_LOGD(TAG, "Return Continue, size:%u", cache_load->valid_size);
    }
    goto __quit;
  }
  load_ret = esp_gmf_port_acquire_out(out_port, &out_load, gmf_wn->frame_len, ESP_GMF_MAX_DELAY);
  out_load->is_done = cache_load->is_done;
  ESP_GMF_PORT_ACQUIRE_OUT_CHECK(TAG, load_ret, ret, { goto __quit; });
  if (out_load->buf_length < gmf_wn->frame_len) {
    ret = ESP_GMF_JOB_ERR_FAIL;
    ESP_LOGE(TAG, "Output buffer is not enough");
    goto __quit;
  }
  esp_gmf_sort_with_format((int16_t *) cache_load->buf, cfg->input_format, gmf_wn->chunk_size, gmf_wn->ch,
                           (int16_t *) out_load->buf);
  int res = gmf_wn->wn_iface->detect(gmf_wn->model_data, (int16_t *) out_load->buf);
  if (res > 0) {
    int trigger_ch = gmf_wn->wn_iface->get_triggered_channel(gmf_wn->model_data);
    if (cfg->detect_cb) {
      cfg->detect_cb(self, trigger_ch, cfg->user_ctx);
    }
  }

  out_load->valid_size = gmf_wn->chunk_size * ESP_WN_BYTES_PER_SAMPLE;
  esp_gmf_cache_ready_for_load(gmf_wn->cache, &need_load);
  if (need_load == false) {
    ret = ESP_GMF_JOB_ERR_TRUNCATE;
  } else {
    ret = (out_load->is_done == true ? ESP_GMF_JOB_ERR_DONE : ESP_GMF_JOB_ERR_OK);
  }

__quit:
  if (out_load) {
    esp_gmf_port_release_out(out_port, out_load, ESP_GMF_MAX_DELAY);
  }
  if (gmf_wn->in_load && (ret != ESP_GMF_JOB_ERR_TRUNCATE)) {
    esp_gmf_port_release_in(in_port, gmf_wn->in_load, ESP_GMF_MAX_DELAY);
    gmf_wn->in_load = NULL;
  }
  if (cache_load) {
    esp_gmf_cache_release(gmf_wn->cache, cache_load);
  }
  return ret;
}

static esp_gmf_job_err_t gmf_wn_close(esp_gmf_audio_element_handle_t self, void *para) {
  gmf_wn_t *gmf_wn = (gmf_wn_t *) self;
  if (gmf_wn->wn_iface && gmf_wn->model_data) {
    gmf_wn->wn_iface->destroy(gmf_wn->model_data);
    gmf_wn->model_data = NULL;
  }
  if (gmf_wn->cache) {
    esp_gmf_cache_delete(gmf_wn->cache);
    gmf_wn->cache = NULL;
  }
  return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_err_t _load_wn_caps_func(esp_gmf_element_handle_t handle) {
  esp_gmf_cap_t *caps = NULL;
  esp_gmf_cap_t wn_caps = {0};
  wn_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_WWE;
  wn_caps.attr_fun = NULL;
  int ret = esp_gmf_cap_append(&caps, &wn_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create capability");

  esp_gmf_element_t *el = (esp_gmf_element_t *) handle;
  el->caps = caps;
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_wn_init(esp_gmf_wn_cfg_t *config, esp_gmf_element_handle_t *handle) {
  esp_gmf_err_t ret = ESP_GMF_ERR_OK;
  ESP_GMF_NULL_CHECK(TAG, config, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;
  gmf_wn_t *gmf_wn = esp_gmf_oal_calloc(1, sizeof(gmf_wn_t));
  ESP_GMF_MEM_VERIFY(
      TAG, gmf_wn, { return ESP_GMF_ERR_MEMORY_LACK; }, "wn", sizeof(gmf_wn_t));
  esp_gmf_obj_t *obj = (esp_gmf_obj_t *) gmf_wn;
  obj->new_obj = gmf_wn_new;
  obj->del_obj = gmf_wn_destroy;

  esp_gmf_wn_cfg_t *obj_cfg = esp_gmf_oal_calloc(1, sizeof(esp_gmf_wn_cfg_t));
  ESP_GMF_NULL_CHECK(TAG, obj_cfg, {
    ret = ESP_GMF_ERR_MEMORY_LACK;
    goto __failed;
  });
  memcpy(obj_cfg, config, sizeof(esp_gmf_wn_cfg_t));
  ret = esp_gmf_obj_set_config(obj, obj_cfg, sizeof(esp_gmf_wn_cfg_t));
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ configuration");
  ret = esp_gmf_obj_set_tag(obj, "ai_wn");
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ tag");
  esp_gmf_element_cfg_t el_cfg = {0};
  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(el_cfg.in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
  ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(el_cfg.out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                    ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
  el_cfg.dependency = true;
  ret = esp_gmf_audio_el_init(gmf_wn, &el_cfg);
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed to initialize wn element");

  ESP_GMF_ELEMENT_GET(gmf_wn)->ops.open = gmf_wn_open;
  ESP_GMF_ELEMENT_GET(gmf_wn)->ops.process = gmf_wn_process;
  ESP_GMF_ELEMENT_GET(gmf_wn)->ops.close = gmf_wn_close;
  ESP_GMF_ELEMENT_GET(gmf_wn)->ops.event_receiver = gmf_wn_received_event_handler;
  ESP_GMF_ELEMENT_GET(gmf_wn)->ops.load_caps = _load_wn_caps_func;

  *handle = obj;
  return ESP_GMF_ERR_OK;
__failed:
  esp_gmf_obj_delete(obj);
  return ret;
}

esp_gmf_err_t esp_gmf_wn_set_detect_cb(esp_gmf_element_handle_t self, esp_wn_detect_cb_t cb, void *ctx) {
  ESP_GMF_NULL_CHECK(TAG, self, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, cb, { return ESP_GMF_ERR_INVALID_ARG; });
  esp_gmf_wn_cfg_t *cfg = OBJ_GET_CFG(self);
  cfg->detect_cb = cb;
  cfg->user_ctx = ctx;
  return ESP_GMF_ERR_OK;
}
