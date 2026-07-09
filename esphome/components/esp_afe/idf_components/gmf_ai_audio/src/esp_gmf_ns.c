/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sdkconfig.h>

#include "esp_log.h"

#if CONFIG_SR_NSN_NSNET2
#include "esp_nsn_models.h"
#include "model_path.h"
#else
#include "esp_ns.h"
#endif

#include "esp_gmf_audio_element.h"
#include "esp_gmf_cache.h"
#include "esp_gmf_cap.h"
#include "esp_gmf_caps_def.h"
#include "esp_gmf_err.h"
#include "esp_gmf_job.h"
#include "esp_gmf_node.h"
#include "esp_gmf_ns.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_payload.h"

#define ESP_NS_BYTES_PER_SAMPLE (sizeof(int16_t))

typedef struct {
  esp_gmf_audio_element_t parent;
#if CONFIG_SR_NSN_NSNET2
  const esp_nsn_iface_t *ns_iface;
  esp_nsn_data_t *ns_data;
  srmodel_list_t *models;
#else
  ns_handle_t ns_handle;
#endif
  uint32_t frame_samples;
  uint32_t frame_bytes;
  esp_gmf_cache_t *cache;
  esp_gmf_payload_t *in_load;
} gmf_ns_t;

static const char *TAG = "GMF_NS";

static bool gmf_ns_is_supported_sample_rate(int sample_rate) { return sample_rate == 16000; }

#if !CONFIG_SR_NSN_NSNET2
static bool gmf_ns_is_supported_frame_ms(int frame_ms) {
  return (frame_ms == 10) || (frame_ms == 20) || (frame_ms == 30);
}
#endif

static esp_gmf_err_t gmf_ns_check_config(const esp_gmf_ns_cfg_t *cfg) {
  ESP_GMF_NULL_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
#if CONFIG_SR_NSN_NSNET2
  if (cfg->model_name == NULL) {
    ESP_LOGE(TAG, "model_name must not be NULL");
    return ESP_GMF_ERR_INVALID_ARG;
  }
#else
  if (gmf_ns_is_supported_frame_ms(cfg->frame_ms) == false) {
    ESP_LOGE(TAG, "Unsupported frame duration: %d ms", cfg->frame_ms);
    return ESP_GMF_ERR_INVALID_ARG;
  }
#endif
  if (gmf_ns_is_supported_sample_rate(cfg->sample_rate) == false) {
    ESP_LOGE(TAG, "Unsupported sample rate: %d", cfg->sample_rate);
    return ESP_GMF_ERR_INVALID_ARG;
  }
  if (cfg->channel != 1) {
    ESP_LOGE(TAG, "Unsupported channel count: %d", cfg->channel);
    return ESP_GMF_ERR_INVALID_ARG;
  }
  return ESP_GMF_ERR_OK;
}

#if CONFIG_SR_NSN_NSNET2
static const char *gmf_ns_get_partition_label(const esp_gmf_ns_cfg_t *cfg) {
  if ((cfg == NULL) || (cfg->partition_label == NULL) || (cfg->partition_label[0] == '\0')) {
    return "model";
  }
  return cfg->partition_label;
}

static void gmf_ns_release_model(gmf_ns_t *gmf_ns) {
  if (gmf_ns == NULL) {
    return;
  }
  if ((gmf_ns->ns_data != NULL) && (gmf_ns->ns_iface != NULL) && (gmf_ns->ns_iface->destroy != NULL)) {
    gmf_ns->ns_iface->destroy(gmf_ns->ns_data);
    gmf_ns->ns_data = NULL;
  }
  if (gmf_ns->models != NULL) {
    esp_srmodel_deinit(gmf_ns->models);
    gmf_ns->models = NULL;
  }
  gmf_ns->ns_iface = NULL;
}
#else
static void gmf_ns_release_backend(gmf_ns_t *gmf_ns) {
  if ((gmf_ns != NULL) && (gmf_ns->ns_handle != NULL)) {
    ns_destroy(gmf_ns->ns_handle);
    gmf_ns->ns_handle = NULL;
  }
}
#endif

static esp_gmf_err_t gmf_ns_new(void *cfg, esp_gmf_obj_handle_t *handle) {
  ESP_GMF_NULL_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;
  esp_gmf_ns_cfg_t *ns_cfg = (esp_gmf_ns_cfg_t *) cfg;
  esp_gmf_obj_handle_t new_obj = NULL;
  esp_gmf_err_t ret = esp_gmf_ns_init(ns_cfg, &new_obj);
  if (ret != ESP_GMF_ERR_OK) {
    return ret;
  }
  *handle = (void *) new_obj;
  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t gmf_ns_received_event_handler(esp_gmf_event_pkt_t *evt, void *ctx) {
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
        return ESP_GMF_ERR_INVALID_ARG;
      }
      memcpy(&info, evt->payload, sizeof(info));
      ESP_LOGD(TAG, "RECV info, from: %s-%p, next: %p, self: %s-%p, type: %x, state: %s, rate: %d, ch: %d, bits: %d",
               OBJ_GET_TAG(el), el, esp_gmf_node_for_next((esp_gmf_node_t *) el), OBJ_GET_TAG(self), self, evt->type,
               esp_gmf_event_get_state_str(state), info.sample_rates, info.channels, info.bits);
      esp_gmf_ns_cfg_t *cfg = OBJ_GET_CFG(self);
      if ((info.sample_rates != cfg->sample_rate) || (info.bits != 16) || (info.channels != cfg->channel)) {
        ESP_LOGE(TAG, "Unsupported format, rate: %d, bits: %d, channels: %d", info.sample_rates, info.bits,
                 info.channels);
        return ESP_GMF_ERR_NOT_SUPPORT;
      }
      esp_gmf_element_set_state(self, ESP_GMF_EVENT_STATE_INITIALIZED);
    }
  }
  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t gmf_ns_destroy(esp_gmf_audio_element_handle_t self) {
  ESP_GMF_NULL_CHECK(TAG, self, { return ESP_GMF_ERR_INVALID_ARG; });

  gmf_ns_t *gmf_ns = (gmf_ns_t *) self;

  if (OBJ_GET_CFG(self)) {
    esp_gmf_oal_free(OBJ_GET_CFG(self));
  }
#if CONFIG_SR_NSN_NSNET2
  gmf_ns_release_model(gmf_ns);
#else
  gmf_ns_release_backend(gmf_ns);
#endif
  if (gmf_ns->cache) {
    esp_gmf_cache_delete(gmf_ns->cache);
    gmf_ns->cache = NULL;
  }
  esp_gmf_audio_el_deinit(self);
  esp_gmf_oal_free(self);

  return ESP_GMF_ERR_OK;
}

static esp_gmf_job_err_t gmf_ns_open(esp_gmf_audio_element_handle_t self, void *para) {
  gmf_ns_t *gmf_ns = (gmf_ns_t *) self;
  esp_gmf_ns_cfg_t *cfg = OBJ_GET_CFG(self);
  esp_gmf_err_t cfg_ret = gmf_ns_check_config(cfg);
  if (cfg_ret != ESP_GMF_ERR_OK) {
    return ESP_GMF_JOB_ERR_FAIL;
  }

#if CONFIG_SR_NSN_NSNET2
  const char *partition_label = gmf_ns_get_partition_label(cfg);
  gmf_ns->models = esp_srmodel_init(partition_label);
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->models, { return ESP_GMF_JOB_ERR_FAIL; });
  if (esp_srmodel_exists(gmf_ns->models, (char *) cfg->model_name) < 0) {
    ESP_LOGE(TAG, "Model not found in partition `%s`: %s", partition_label, cfg->model_name);
    goto __failed;
  }

  gmf_ns->ns_iface = (cfg->ns_iface != NULL) ? cfg->ns_iface : esp_nsnet_handle_from_name((char *) cfg->model_name);
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->ns_iface, { goto __failed; });
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->ns_iface->create, { goto __failed; });
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->ns_iface->get_samp_chunksize, { goto __failed; });

  gmf_ns->ns_data = gmf_ns->ns_iface->create((char *) cfg->model_name);
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->ns_data, { goto __failed; });

  gmf_ns->frame_samples = gmf_ns->ns_iface->get_samp_chunksize(gmf_ns->ns_data);
  if (gmf_ns->frame_samples == 0) {
    ESP_LOGE(TAG, "Invalid NS frame size");
    goto __failed;
  }
  if ((gmf_ns->ns_iface->get_samp_rate != NULL) &&
      (gmf_ns->ns_iface->get_samp_rate(gmf_ns->ns_data) != cfg->sample_rate)) {
    ESP_LOGE(TAG, "Model sample rate mismatch, expected: %d", cfg->sample_rate);
    goto __failed;
  }
#else
  gmf_ns->ns_handle = ns_create(cfg->frame_ms);
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->ns_handle, { return ESP_GMF_JOB_ERR_FAIL; });
  gmf_ns->frame_samples = cfg->sample_rate * cfg->frame_ms / 1000;
#endif

  gmf_ns->frame_bytes = gmf_ns->frame_samples * ESP_NS_BYTES_PER_SAMPLE;
  esp_gmf_cache_new(gmf_ns->frame_bytes, &gmf_ns->cache);
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->cache, { goto __failed; });

  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, gmf_ns->frame_bytes);
  ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                    ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, gmf_ns->frame_bytes);
#if CONFIG_SR_NSN_NSNET2
  ESP_LOGI(TAG, "GMF NS open, model: %s, sample_rate: %d, frame_bytes: %lu", cfg->model_name, cfg->sample_rate,
           (unsigned long) gmf_ns->frame_bytes);
#else
  ESP_LOGI(TAG, "GMF NS open, backend: webrtc, sample_rate: %d, frame_ms: %d, frame_bytes: %lu", cfg->sample_rate,
           cfg->frame_ms, (unsigned long) gmf_ns->frame_bytes);
#endif

  esp_gmf_info_sound_t snd_info = {
      .sample_rates = cfg->sample_rate,
      .bits = 16,
      .channels = 1,
  };
  esp_gmf_element_notify_snd_info(self, &snd_info);
  return ESP_GMF_JOB_ERR_OK;

__failed:
  if (gmf_ns->cache) {
    esp_gmf_cache_delete(gmf_ns->cache);
    gmf_ns->cache = NULL;
  }
#if CONFIG_SR_NSN_NSNET2
  gmf_ns_release_model(gmf_ns);
#else
  gmf_ns_release_backend(gmf_ns);
#endif
  return ESP_GMF_JOB_ERR_FAIL;
}

static esp_gmf_job_err_t gmf_ns_process(esp_gmf_audio_element_handle_t self, void *para) {
  int ret = ESP_GMF_JOB_ERR_OK;
  gmf_ns_t *gmf_ns = (gmf_ns_t *) self;
  esp_gmf_port_handle_t in_port = ESP_GMF_ELEMENT_GET(self)->in;
  esp_gmf_port_handle_t out_port = ESP_GMF_ELEMENT_GET(self)->out;
  esp_gmf_payload_t *out_load = NULL;
  esp_gmf_payload_t *cache_load = NULL;
  esp_gmf_err_io_t load_ret = 0;
  bool need_load = false;

  ESP_GMF_NULL_CHECK(TAG, in_port, { return ESP_GMF_JOB_ERR_FAIL; });
  ESP_GMF_NULL_CHECK(TAG, out_port, { return ESP_GMF_JOB_ERR_FAIL; });
#if CONFIG_SR_NSN_NSNET2
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->ns_data, { return ESP_GMF_JOB_ERR_FAIL; });
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->ns_iface, { return ESP_GMF_JOB_ERR_FAIL; });
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->ns_iface->process, { return ESP_GMF_JOB_ERR_FAIL; });
#else
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->ns_handle, { return ESP_GMF_JOB_ERR_FAIL; });
#endif
  ESP_GMF_NULL_CHECK(TAG, gmf_ns->cache, { return ESP_GMF_JOB_ERR_FAIL; });

  esp_gmf_cache_ready_for_load(gmf_ns->cache, &need_load);
  if (need_load) {
    load_ret = esp_gmf_port_acquire_in(in_port, &gmf_ns->in_load, gmf_ns->frame_bytes, in_port->wait_ticks);
    ESP_GMF_PORT_ACQUIRE_IN_CHECK(TAG, load_ret, ret, { goto __quit; });
    esp_gmf_cache_load(gmf_ns->cache, gmf_ns->in_load);
  }
  esp_gmf_cache_acquire(gmf_ns->cache, gmf_ns->frame_bytes, &cache_load);
  if (cache_load->valid_size != gmf_ns->frame_bytes) {
    if (cache_load->is_done == true) {
      ret = ESP_GMF_JOB_ERR_DONE;
    } else {
      ret = ESP_GMF_JOB_ERR_CONTINUE;
    }
    ESP_LOGD(TAG, "Return Continue, size:%u", cache_load->valid_size);
    goto __quit;
  }

  load_ret = esp_gmf_port_acquire_out(out_port, &out_load, gmf_ns->frame_bytes, ESP_GMF_MAX_DELAY);
  ESP_GMF_PORT_ACQUIRE_OUT_CHECK(TAG, load_ret, ret, { goto __quit; });
  if (out_load->buf_length < gmf_ns->frame_bytes) {
    ret = ESP_GMF_JOB_ERR_FAIL;
    ESP_LOGE(TAG, "Output buffer is not enough");
    goto __quit;
  }

#if CONFIG_SR_NSN_NSNET2
  gmf_ns->ns_iface->process(gmf_ns->ns_data, (int16_t *) cache_load->buf, (int16_t *) out_load->buf);
#else
  ns_process(gmf_ns->ns_handle, (int16_t *) cache_load->buf, (int16_t *) out_load->buf);
#endif
  out_load->valid_size = gmf_ns->frame_bytes;
  out_load->is_done = cache_load->is_done;

  esp_gmf_cache_ready_for_load(gmf_ns->cache, &need_load);
  if (need_load == false) {
    ret = ESP_GMF_JOB_ERR_TRUNCATE;
  } else {
    ret = (cache_load->is_done == true ? ESP_GMF_JOB_ERR_DONE : ESP_GMF_JOB_ERR_OK);
  }

__quit:
  if (out_load) {
    esp_gmf_port_release_out(out_port, out_load, ESP_GMF_MAX_DELAY);
  }
  if (gmf_ns->in_load && (ret != ESP_GMF_JOB_ERR_TRUNCATE)) {
    esp_gmf_port_release_in(in_port, gmf_ns->in_load, ESP_GMF_MAX_DELAY);
    gmf_ns->in_load = NULL;
  }
  if (cache_load) {
    esp_gmf_cache_release(gmf_ns->cache, cache_load);
  }
  return ret;
}

static esp_gmf_job_err_t gmf_ns_close(esp_gmf_audio_element_handle_t self, void *para) {
  gmf_ns_t *gmf_ns = (gmf_ns_t *) self;
#if CONFIG_SR_NSN_NSNET2
  gmf_ns_release_model(gmf_ns);
#else
  gmf_ns_release_backend(gmf_ns);
#endif
  if (gmf_ns->cache) {
    esp_gmf_cache_delete(gmf_ns->cache);
    gmf_ns->cache = NULL;
  }
  gmf_ns->in_load = NULL;
  return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_err_t _load_ns_caps_func(esp_gmf_element_handle_t handle) {
  esp_gmf_cap_t *caps = NULL;
  esp_gmf_cap_t ns_caps = {0};
  ns_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_NS;
  ns_caps.attr_fun = NULL;
  int ret = esp_gmf_cap_append(&caps, &ns_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create capability");

  esp_gmf_element_t *el = (esp_gmf_element_t *) handle;
  el->caps = caps;
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_ns_init(esp_gmf_ns_cfg_t *config, esp_gmf_obj_handle_t *handle) {
  esp_gmf_err_t ret = gmf_ns_check_config(config);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Invalid NS configuration");
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  *handle = NULL;

  gmf_ns_t *gmf_ns = esp_gmf_oal_calloc(1, sizeof(gmf_ns_t));
  ESP_GMF_MEM_VERIFY(
      TAG, gmf_ns, { return ESP_GMF_ERR_MEMORY_LACK; }, "ns", sizeof(gmf_ns_t));
  esp_gmf_obj_t *obj = (esp_gmf_obj_t *) gmf_ns;
  obj->new_obj = gmf_ns_new;
  obj->del_obj = gmf_ns_destroy;

  esp_gmf_ns_cfg_t *obj_cfg = esp_gmf_oal_calloc(1, sizeof(esp_gmf_ns_cfg_t));
  ESP_GMF_NULL_CHECK(TAG, obj_cfg, {
    ret = ESP_GMF_ERR_MEMORY_LACK;
    goto __failed;
  });
  memcpy(obj_cfg, config, sizeof(esp_gmf_ns_cfg_t));
  ret = esp_gmf_obj_set_config(obj, obj_cfg, sizeof(esp_gmf_ns_cfg_t));
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ configuration");
  ret = esp_gmf_obj_set_tag(obj, "ai_ns");
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ tag");
  esp_gmf_element_cfg_t el_cfg = {0};
  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(el_cfg.in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
  ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(el_cfg.out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                    ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
  el_cfg.dependency = true;
  ret = esp_gmf_audio_el_init(gmf_ns, &el_cfg);
  if (ret != ESP_GMF_ERR_OK) {
    return ret;
  }

  ESP_GMF_ELEMENT_GET(gmf_ns)->ops.open = gmf_ns_open;
  ESP_GMF_ELEMENT_GET(gmf_ns)->ops.process = gmf_ns_process;
  ESP_GMF_ELEMENT_GET(gmf_ns)->ops.close = gmf_ns_close;
  ESP_GMF_ELEMENT_GET(gmf_ns)->ops.event_receiver = gmf_ns_received_event_handler;
  ESP_GMF_ELEMENT_GET(gmf_ns)->ops.load_caps = _load_ns_caps_func;

  *handle = obj;
  return ESP_GMF_ERR_OK;

__failed:
  esp_gmf_obj_delete(obj);
  return ret;
}
