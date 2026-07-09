/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_gmf_obj.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_gmf_audio_element.h"
#include "esp_gmf_err.h"
#include "esp_gmf_job.h"
#include "esp_gmf_new_databus.h"
#include "esp_gmf_oal_mem.h"
#include "esp_wn_iface.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_process_sdkconfig.h"
#include "esp_gmf_afe_manager.h"
#include "esp_gmf_afe.h"
#include "esp_gmf_cap.h"
#include "esp_gmf_caps_def.h"
#include "esp_gmf_method.h"
#include "esp_gmf_ai_audio_methods.h"

#define AFE_DEFAULT_DATA_SIZE (2048)
#define WAKEUP_ST_SET(handle, st) (handle->wakeup_state = st)

/**
 * @brief  Represents the various states of the wakeup process in the system
 *
 *         This enumeration defines the different states that the system can be in
 *         during the wakeup and speech processing lifecycle.
 */
typedef enum {
  ST_IDLE,
  ST_WAKEUP,
  ST_SPEECHING,
  ST_WAIT_FOR_SLEEP,
} wakeup_state_t;

/**
 * @brief  Enumeration representing various wakeup events in the system
 *
 *         This enumeration defines the different types of events that can trigger
 *         the state changing in the system. Each event corresponds to a specific
 *         condition or state.
 */
typedef enum {
  ET_NOISE_DECT,
  ET_SPEECH_DECT,
  ET_WWE_DECT,
  ET_WAKEUP_TIMER_EXPIRED,
  ET_KEEP_WAKE_MODIFIED,
  ET_TRIGGER_WAKEUP,
  ET_TRIGGER_SLEEP,
  ET_UNKNOWN
} wakeup_event_t;

typedef struct {
  wakeup_event_t event;
  union {
    struct {
      bool enable;
    } keep_awake;
    struct {
      bool state;
    } trigger;
  } payload;
} afe_evt_msg_t;

/**
 * @brief  Structure representing the ESP GMF AFE component
 */
typedef struct {
  esp_gmf_audio_element_t parent;
  esp_gmf_db_handle_t in_db;
  esp_gmf_db_handle_t out_db;
  wakeup_state_t wakeup_state;
  wakeup_event_t last_event;
  bool origin_vad_enable;
  bool keep_wake;
  esp_timer_handle_t wakeup_timer;
  SemaphoreHandle_t wake_st_lock;
  model_iface_data_t *mn_handle;
  bool mn_detecting;
  SemaphoreHandle_t mn_lock;
  esp_mn_state_t mn_state;
  esp_mn_iface_t *multinet;
  QueueHandle_t msg_queue;
} esp_gmf_afe_t;

static const char *TAG = "GMF_AFE";

static const char *state_str[] = {
    "ST_IDLE",
    "ST_WAKEUP",
    "ST_SPEECHING",
    "ST_WAIT_FOR_SLEEP",
};
static const char *event_str[] = {
    "ET_NOISE_DECT",         "ET_SPEECH_DECT",    "ET_WWE_DECT",      "ET_WAKEUP_TIMER_EXPIRED",
    "ET_KEEP_WAKE_MODIFIED", "ET_TRIGGER_WAKEUP", "ET_TRIGGER_SLEEP", "ET_UNKNOWN",
};

static wakeup_event_t result_2_event(afe_fetch_result_t *result) {
  if (result->wakeup_state == WAKENET_DETECTED) {
    return ET_WWE_DECT;
  }
  if (result->vad_state == VAD_SILENCE) {
    return ET_NOISE_DECT;
  }
  if (result->vad_state == VAD_SPEECH) {
    return ET_SPEECH_DECT;
  }
  return ET_UNKNOWN;
}

static void event_2_user(esp_gmf_afe_t *gmf_afe, int event, void *event_data, size_t dlen) {
  esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(gmf_afe);
  if (cfg->event_cb) {
    esp_gmf_afe_evt_t afe_event = {
        .type = event,
        .event_data = event_data,
        .data_len = dlen,
    };
    cfg->event_cb(gmf_afe, &afe_event, cfg->event_ctx);
  }
}

static void afe_result_2_wakeup_info(afe_fetch_result_t *result, esp_gmf_afe_wakeup_info_t *info) {
  info->wake_word_index = result->wake_word_index;
  info->wakenet_model_index = result->wakenet_model_index;
  info->data_volume = result->data_volume;
}

static void wakeup_state_reset(esp_gmf_afe_t *gmf_afe) {
  esp_timer_stop(gmf_afe->wakeup_timer);
  WAKEUP_ST_SET(gmf_afe, ST_IDLE);
  esp_gmf_afe_manager_features_t feat = {0};
  esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(gmf_afe);
  if (cfg && cfg->afe_manager) {
    esp_gmf_afe_manager_get_features(cfg->afe_manager, &feat);
    if (feat.wakeup && gmf_afe->origin_vad_enable) {
      esp_gmf_afe_manager_enable_features(cfg->afe_manager, ESP_AFE_FEATURE_VAD, false);
    }
  }
}

static void wakeup_timer_start(esp_gmf_afe_t *gmf_afe) {
  esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(gmf_afe);
  int timeout = 0;
  if (gmf_afe->wakeup_state == ST_WAKEUP) {
    timeout = cfg->wakeup_time;
  } else if (gmf_afe->wakeup_state == ST_WAIT_FOR_SLEEP) {
    timeout = cfg->wakeup_end;
  }
  esp_timer_stop(gmf_afe->wakeup_timer);
  if (timeout) {
    esp_timer_start_once(gmf_afe->wakeup_timer, timeout * 1000);
  }
}

static void wakeup_state_update(esp_gmf_afe_t *gmf_afe, wakeup_event_t event, void *event_data, size_t len) {
  esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(gmf_afe);
  esp_gmf_afe_manager_features_t afe_feat = {0};
  int32_t user_event = -1;
  if (event == ET_KEEP_WAKE_MODIFIED || gmf_afe->last_event != event) {
    ESP_LOGV(TAG, "Recorder update state, cur %s, event %s", state_str[gmf_afe->wakeup_state], event_str[event]);
    gmf_afe->last_event = event;
  } else {
    return;
  }
  xSemaphoreTake(gmf_afe->wake_st_lock, portMAX_DELAY);
  if ((event == ET_WWE_DECT || event == ET_TRIGGER_WAKEUP) && gmf_afe->wakeup_state != ST_IDLE) {
    wakeup_state_reset(gmf_afe);
  }
  esp_gmf_afe_manager_get_features(cfg->afe_manager, &afe_feat);
  switch (gmf_afe->wakeup_state) {
    case ST_IDLE: {
      if (event == ET_WWE_DECT || event == ET_TRIGGER_WAKEUP) {
        WAKEUP_ST_SET(gmf_afe, ST_WAKEUP);
        if (!gmf_afe->keep_wake) {
          wakeup_timer_start(gmf_afe);
        }
        if (gmf_afe->origin_vad_enable) {
          esp_gmf_afe_manager_enable_features(cfg->afe_manager, ESP_AFE_FEATURE_VAD, true);
        }
        user_event = ESP_GMF_AFE_EVT_WAKEUP_START;
      } else if (event == ET_SPEECH_DECT && afe_feat.wakeup == false) {
        WAKEUP_ST_SET(gmf_afe, ST_SPEECHING);
        user_event = ESP_GMF_AFE_EVT_VAD_START;
      }
      break;
    }
    case ST_WAKEUP: {
      if (event == ET_SPEECH_DECT) {
        esp_timer_stop(gmf_afe->wakeup_timer);
        WAKEUP_ST_SET(gmf_afe, ST_SPEECHING);
        user_event = ESP_GMF_AFE_EVT_VAD_START;
      } else if (event == ET_KEEP_WAKE_MODIFIED) {
        if (gmf_afe->keep_wake) {
          esp_timer_stop(gmf_afe->wakeup_timer);
        } else {
          wakeup_timer_start(gmf_afe);
        }
      } else if (event == ET_WAKEUP_TIMER_EXPIRED) {
        WAKEUP_ST_SET(gmf_afe, ST_IDLE);
        esp_timer_stop(gmf_afe->wakeup_timer);
        user_event = ESP_GMF_AFE_EVT_WAKEUP_END;
      } else if (event == ET_TRIGGER_SLEEP) {
        WAKEUP_ST_SET(gmf_afe, ST_IDLE);
        esp_timer_stop(gmf_afe->wakeup_timer);
        if (gmf_afe->origin_vad_enable) {
          esp_gmf_afe_manager_enable_features(cfg->afe_manager, ESP_AFE_FEATURE_VAD, false);
        }
        user_event = ESP_GMF_AFE_EVT_WAKEUP_END;
      }
      break;
    }
    case ST_SPEECHING: {
      if (event == ET_NOISE_DECT) {
        if (afe_feat.wakeup == true) {
          if (!gmf_afe->keep_wake) {
            WAKEUP_ST_SET(gmf_afe, ST_WAIT_FOR_SLEEP);
            wakeup_timer_start(gmf_afe);
          } else {
            WAKEUP_ST_SET(gmf_afe, ST_WAKEUP);
          }
        } else {
          WAKEUP_ST_SET(gmf_afe, ST_IDLE);
        }
        user_event = ESP_GMF_AFE_EVT_VAD_END;
      } else if (event == ET_TRIGGER_SLEEP) {
        WAKEUP_ST_SET(gmf_afe, ST_IDLE);
        if (gmf_afe->origin_vad_enable) {
          esp_gmf_afe_manager_enable_features(cfg->afe_manager, ESP_AFE_FEATURE_VAD, false);
        }
        user_event = ESP_GMF_AFE_EVT_WAKEUP_END;
      }
      break;
    }
    case ST_WAIT_FOR_SLEEP: {
      if (event == ET_SPEECH_DECT) {
        WAKEUP_ST_SET(gmf_afe, ST_SPEECHING);
        user_event = ESP_GMF_AFE_EVT_VAD_START;
      } else if (event == ET_WAKEUP_TIMER_EXPIRED) {
        WAKEUP_ST_SET(gmf_afe, ST_IDLE);
        if (gmf_afe->origin_vad_enable) {
          esp_gmf_afe_manager_enable_features(cfg->afe_manager, ESP_AFE_FEATURE_VAD, false);
        }
        user_event = ESP_GMF_AFE_EVT_WAKEUP_END;
      } else if (event == ET_KEEP_WAKE_MODIFIED) {
        if (gmf_afe->keep_wake) {
          esp_timer_stop(gmf_afe->wakeup_timer);
          WAKEUP_ST_SET(gmf_afe, ST_WAKEUP);
        } else {
          wakeup_timer_start(gmf_afe);
        }
      } else if (event == ET_TRIGGER_SLEEP) {
        WAKEUP_ST_SET(gmf_afe, ST_IDLE);
        esp_timer_stop(gmf_afe->wakeup_timer);
        if (gmf_afe->origin_vad_enable) {
          esp_gmf_afe_manager_enable_features(cfg->afe_manager, ESP_AFE_FEATURE_VAD, false);
        }
        user_event = ESP_GMF_AFE_EVT_WAKEUP_END;
      }
      break;
    }
    default:
      break;
  }
  xSemaphoreGive(gmf_afe->wake_st_lock);
  if (user_event != -1) {
    event_2_user(gmf_afe, user_event, event_data, len);
  }
}

static void wakeup_afe_monitor(afe_fetch_result_t *result, void *user_ctx) {
  wakeup_event_t event = result_2_event(result);
  if (event == ET_WWE_DECT) {
    esp_gmf_afe_wakeup_info_t info = {0};
    afe_result_2_wakeup_info(result, &info);
    wakeup_state_update(user_ctx, event, &info, sizeof(esp_gmf_afe_wakeup_info_t));
  } else {
    wakeup_state_update(user_ctx, event, NULL, 0);
  }
}

static void wakeup_timer_expired(void *arg) {
  esp_gmf_afe_t *gmf_afe = arg;
  wakeup_state_update(gmf_afe, ET_WAKEUP_TIMER_EXPIRED, NULL, 0);
}

static esp_err_t vcmd_det_begin(esp_gmf_afe_t *gmf_afe) {
  if (!gmf_afe) {
    return ESP_ERR_INVALID_ARG;
  }
  xSemaphoreTake(gmf_afe->mn_lock, portMAX_DELAY);
  gmf_afe->mn_detecting = true;
  gmf_afe->mn_state = ESP_MN_STATE_TIMEOUT;
  xSemaphoreGive(gmf_afe->mn_lock);
  return ESP_OK;
}

static esp_err_t vcmd_det_cancel(esp_gmf_afe_t *gmf_afe) {
  if (!gmf_afe) {
    return ESP_ERR_INVALID_ARG;
  }
  xSemaphoreTake(gmf_afe->mn_lock, portMAX_DELAY);
  gmf_afe->mn_detecting = false;
  gmf_afe->mn_state = ESP_MN_STATE_TIMEOUT;
  gmf_afe->multinet->clean(gmf_afe->mn_handle);
  xSemaphoreGive(gmf_afe->mn_lock);
  return ESP_OK;
}

static void mn_afe_monitor(afe_fetch_result_t *result, void *user_ctx) {
  esp_gmf_afe_t *gmf_afe = user_ctx;

  if (gmf_afe->mn_detecting) {
    if ((gmf_afe->mn_state != ESP_MN_STATE_DETECTING) && result->vad_cache_size) {
      uint8_t *cache = (uint8_t *) result->vad_cache;
      while (cache < ((uint8_t *) result->vad_cache + result->vad_cache_size)) {
        gmf_afe->mn_state = gmf_afe->multinet->detect(gmf_afe->mn_handle, (int16_t *) cache);
        if (gmf_afe->mn_state == ESP_MN_STATE_DETECTED) {
          goto __proc_state;
        }
        cache += result->data_size;
      }
    }
    gmf_afe->mn_state = gmf_afe->multinet->detect(gmf_afe->mn_handle, result->data);
  __proc_state:
    switch (gmf_afe->mn_state) {
      case ESP_MN_STATE_DETECTED: {
        esp_mn_results_t *mn_result = gmf_afe->multinet->get_results(gmf_afe->mn_handle);
        esp_gmf_afe_vcmd_info_t vcmd_info = {.phrase_id = mn_result->phrase_id[0], .prob = mn_result->prob[0]};
        strlcpy(vcmd_info.str, mn_result->string, sizeof(vcmd_info.str));
        event_2_user(gmf_afe, vcmd_info.phrase_id, &vcmd_info, sizeof(esp_gmf_afe_vcmd_info_t));

        break;
      }
      case ESP_MN_STATE_TIMEOUT: {
        xSemaphoreTake(gmf_afe->mn_lock, portMAX_DELAY);
        gmf_afe->mn_detecting = false;
        xSemaphoreGive(gmf_afe->mn_lock);
        event_2_user(gmf_afe, ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT, NULL, 0);
        break;
      }
      default:
        break;
    }
  }
}

static void esp_gmf_afe_result_proc(afe_fetch_result_t *result, void *user_ctx) {
  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) user_ctx;
  if (result->data_size) {
    esp_gmf_afe_manager_features_t feat = {0};
    esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(gmf_afe);
    esp_gmf_afe_manager_get_features(cfg->afe_manager, &feat);
    if (feat.wakeup || feat.vad) {
      wakeup_afe_monitor(result, user_ctx);
    }
    if (cfg->vcmd_detect_en) {
      mn_afe_monitor(result, user_ctx);
    }

    uint32_t available_size = 0;
    esp_gmf_db_get_available(gmf_afe->out_db, &available_size);

    if (available_size < result->data_size) {
      esp_gmf_data_bus_block_t blk = {0};
      blk.buf_length = result->data_size - available_size;
      ESP_GMF_RET_ON_FAIL(
          TAG, esp_gmf_db_acquire_read(gmf_afe->out_db, &blk, blk.buf_length, ESP_GMF_MAX_DELAY), { return; },
          "DB failed to acquire read");
      ESP_GMF_RET_ON_FAIL(
          TAG, esp_gmf_db_release_read(gmf_afe->out_db, &blk, ESP_GMF_MAX_DELAY), { return; },
          "DB failed to release read");
    }
    esp_gmf_data_bus_block_t blk = {0};
    ESP_GMF_RET_ON_FAIL(
        TAG, esp_gmf_db_acquire_write(gmf_afe->out_db, &blk, result->data_size, ESP_GMF_MAX_DELAY), { return; },
        "DB failed to acquire write");
    blk.buf = (uint8_t *) result->data;
    blk.valid_size = result->data_size;
    ESP_GMF_RET_ON_FAIL(
        TAG, esp_gmf_db_release_write(gmf_afe->out_db, &blk, ESP_GMF_MAX_DELAY), { return; },
        "DB failed to release write");
  }
}

static int32_t esp_gmf_afe_read_cb(void *buffer, int buf_sz, void *user_ctx, TickType_t ticks) {
  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) user_ctx;
  esp_gmf_data_bus_block_t blk = {0};
  blk.buf = buffer;
  blk.buf_length = buf_sz;
  ESP_LOGD(TAG, "Feed %zu", blk.buf_length);
  ESP_GMF_RET_ON_FAIL(
      TAG, esp_gmf_db_acquire_read(gmf_afe->in_db, &blk, buf_sz, ticks), { return 0; }, "DB failed to acquire read");
  ESP_GMF_RET_ON_FAIL(
      TAG, esp_gmf_db_release_read(gmf_afe->in_db, &blk, ticks), { return 0; }, "DB failed to release read");
  return buf_sz;
}

static esp_gmf_err_t esp_gmf_afe_new(void *cfg, esp_gmf_obj_handle_t *handle) {
  ESP_GMF_MEM_CHECK(TAG, cfg, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_MEM_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  esp_gmf_afe_cfg_t *gmf_afe_manager_cfg = (esp_gmf_afe_cfg_t *) cfg;
  esp_gmf_obj_handle_t new_obj = NULL;
  int ret = ESP_GMF_ERR_OK;
  ret = esp_gmf_afe_init(gmf_afe_manager_cfg, &new_obj);
  if (ret != ESP_GMF_ERR_OK) {
    return ret;
  }
  *handle = (void *) new_obj;
  ESP_LOGI(TAG, "New an object,%s-%p", OBJ_GET_TAG(new_obj), new_obj);
  return ret;
}

static esp_gmf_err_t esp_gmf_afe_destroy(esp_gmf_audio_element_handle_t self) {
  if (self) {
    if (OBJ_GET_CFG(self)) {
      esp_gmf_oal_free(OBJ_GET_CFG(self));
    }
    esp_gmf_audio_el_deinit(self);
    esp_gmf_oal_free(self);
  }
  return ESP_GMF_ERR_OK;
}

static esp_gmf_job_err_t esp_gmf_afe_open(esp_gmf_audio_element_handle_t self, void *para) {
  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) self;
  esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(self);
  size_t buf_size = 0, chunk_size = 0;
  uint8_t ch_num = 0;
  esp_gmf_afe_manager_get_input_ch_num(cfg->afe_manager, &ch_num);
  esp_gmf_afe_manager_get_chunk_size(cfg->afe_manager, &chunk_size);
  buf_size = chunk_size * ch_num * sizeof(uint16_t);
  ESP_GMF_ELEMENT_GET(self)->in_attr.data_size = buf_size;
  ESP_GMF_ELEMENT_GET(self)->out_attr.data_size = chunk_size * 2;
  esp_gmf_afe_manager_features_t feat = {0};
  esp_gmf_afe_manager_get_features(cfg->afe_manager, &feat);
  if (feat.wakeup || feat.vad) {
    gmf_afe->wake_st_lock = xSemaphoreCreateMutex();
    esp_timer_create_args_t wakeup_timer_cfg = {
        .callback = wakeup_timer_expired,
        .arg = gmf_afe,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wakeup_timer",
    };
    esp_timer_create(&wakeup_timer_cfg, &gmf_afe->wakeup_timer);
    if (feat.wakeup && feat.vad) {
      esp_gmf_afe_manager_enable_features(cfg->afe_manager, ESP_AFE_FEATURE_VAD, false);
      gmf_afe->origin_vad_enable = true;
    }
  }
  if (cfg->vcmd_detect_en) {
    char *mn_name = esp_srmodel_filter(cfg->models, ESP_MN_PREFIX, cfg->mn_language);
    ESP_GMF_NULL_CHECK(TAG, mn_name, return ESP_GMF_JOB_ERR_FAIL);
    gmf_afe->multinet = esp_mn_handle_from_name(mn_name);

    if (cfg->vcmd_timeout == 0) {
      ESP_LOGW(TAG, "Voice command timeout configured as 0, reset to default: 5760 ms");
      cfg->vcmd_timeout = 5760;
    }
    gmf_afe->mn_handle = gmf_afe->multinet->create(mn_name, cfg->vcmd_timeout);
    ESP_GMF_NULL_CHECK(TAG, gmf_afe->mn_handle, return ESP_GMF_JOB_ERR_FAIL);
    gmf_afe->mn_lock = xSemaphoreCreateMutex();
    ESP_GMF_NULL_CHECK(TAG, gmf_afe->mn_lock, return ESP_GMF_JOB_ERR_FAIL);
    gmf_afe->mn_state = ESP_MN_STATE_TIMEOUT;

    esp_mn_commands_update_from_sdkconfig((esp_mn_iface_t *) gmf_afe->multinet, gmf_afe->mn_handle);
  }
  esp_gmf_db_new_ringbuf(2, buf_size, &gmf_afe->in_db);
  esp_gmf_db_new_ringbuf(1, buf_size * 2 + (cfg->delay_samples * sizeof(uint16_t)), &gmf_afe->out_db);
  esp_gmf_afe_manager_set_result_cb(cfg->afe_manager, esp_gmf_afe_result_proc, self);
  esp_gmf_afe_manager_set_read_cb(cfg->afe_manager, esp_gmf_afe_read_cb, self);
  if (gmf_afe->msg_queue == NULL) {
    gmf_afe->msg_queue = xQueueCreate(4, sizeof(afe_evt_msg_t));
    ESP_GMF_NULL_CHECK(TAG, gmf_afe->msg_queue, goto __failed);
  }
  esp_gmf_info_sound_t snd_info = {0};
  snd_info.sample_rates = 16000;
  snd_info.bits = 16;
  snd_info.channels = 1;
  esp_gmf_element_notify_snd_info(self, &snd_info);
  return ESP_GMF_JOB_ERR_OK;

__failed:
  esp_gmf_obj_delete(self);
  return ESP_GMF_JOB_ERR_FAIL;
}

static esp_gmf_job_err_t esp_gmf_afe_close(esp_gmf_audio_element_handle_t self, void *para) {
  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) self;
  esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(self);
  if (cfg->vcmd_detect_en) {
    vcmd_det_cancel(gmf_afe);
  }
  esp_gmf_afe_manager_set_read_cb(cfg->afe_manager, NULL, NULL);
  esp_gmf_afe_manager_set_result_cb(cfg->afe_manager, NULL, NULL);

  if (gmf_afe->in_db) {
    esp_gmf_db_deinit(gmf_afe->in_db);
    gmf_afe->in_db = NULL;
  }
  if (gmf_afe->out_db) {
    esp_gmf_db_deinit(gmf_afe->out_db);
    gmf_afe->out_db = NULL;
  }
  if (gmf_afe->wake_st_lock) {
    vSemaphoreDelete(gmf_afe->wake_st_lock);
    gmf_afe->wake_st_lock = NULL;
  }
  if (gmf_afe->wakeup_timer) {
    esp_timer_stop(gmf_afe->wakeup_timer);
    esp_timer_delete(gmf_afe->wakeup_timer);
    gmf_afe->wakeup_timer = NULL;
  }
  if (gmf_afe->mn_lock) {
    vSemaphoreDelete(gmf_afe->mn_lock);
    gmf_afe->mn_lock = NULL;
  }
  if (gmf_afe->mn_handle) {
    gmf_afe->multinet->destroy(gmf_afe->mn_handle);
    gmf_afe->mn_handle = NULL;
  }
  if (gmf_afe->msg_queue) {
    vQueueDelete(gmf_afe->msg_queue);
    gmf_afe->msg_queue = NULL;
  }
  return ESP_GMF_JOB_ERR_OK;
}

static void esp_gmf_afe_cmd_handler(esp_gmf_audio_element_handle_t self) {
  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) self;
  if (gmf_afe->msg_queue) {
    afe_evt_msg_t msg;
    while (xQueueReceive(gmf_afe->msg_queue, &msg, 0) == pdTRUE) {
      switch (msg.event) {
        case ET_KEEP_WAKE_MODIFIED: {
          xSemaphoreTake(gmf_afe->wake_st_lock, portMAX_DELAY);
          gmf_afe->keep_wake = msg.payload.keep_awake.enable;
          xSemaphoreGive(gmf_afe->wake_st_lock);
          wakeup_state_update(gmf_afe, msg.event, NULL, 0);
          break;
        }
        case ET_TRIGGER_WAKEUP:
        case ET_TRIGGER_SLEEP: {
          wakeup_state_update(gmf_afe, msg.event, NULL, 0);
          break;
        }
        default:
          break;
      }
    }
  }
}

static esp_gmf_job_err_t esp_gmf_afe_proc(esp_gmf_audio_element_handle_t self, void *para) {
  int ret = 0;
  esp_gmf_port_handle_t in_port = ESP_GMF_ELEMENT_GET(self)->in;
  esp_gmf_port_handle_t out_port = ESP_GMF_ELEMENT_GET(self)->out;
  esp_gmf_payload_t *in_load = NULL;
  esp_gmf_payload_t *out_load = NULL;
  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) self;
  esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(self);

  esp_gmf_afe_cmd_handler(self);

  ret = esp_gmf_port_acquire_in(in_port, &in_load, ESP_GMF_ELEMENT_GET(self)->in_attr.data_size, ESP_GMF_MAX_DELAY);
  if (ret < 0) {
    ESP_LOGE(TAG, "Read data error, ret:%d, line:%d", ret, __LINE__);
    return ret == ESP_GMF_IO_ABORT ? ESP_GMF_JOB_ERR_OK : ESP_GMF_JOB_ERR_FAIL;
  }

  esp_gmf_data_bus_block_t blk = {0};
  esp_gmf_db_acquire_write(gmf_afe->in_db, &blk, in_load->valid_size, ESP_GMF_MAX_DELAY);
  blk.buf = in_load->buf;
  blk.valid_size = in_load->valid_size;
  esp_gmf_db_release_write(gmf_afe->in_db, &blk, ESP_GMF_MAX_DELAY);

  uint32_t filled_size = 0;
  esp_gmf_db_get_filled_size(gmf_afe->out_db, &filled_size);

  if (filled_size > (cfg->delay_samples * sizeof(uint16_t))) {
    uint32_t rsize = filled_size - (cfg->delay_samples * sizeof(uint16_t));
    esp_gmf_port_acquire_out(out_port, &out_load, rsize, ESP_GMF_MAX_DELAY);
    esp_gmf_data_bus_block_t blk = {0};
    blk.buf = out_load->buf;
    blk.buf_length = rsize;
    esp_gmf_db_acquire_read(gmf_afe->out_db, &blk, rsize, ESP_GMF_MAX_DELAY);
    esp_gmf_db_release_read(gmf_afe->out_db, &blk, ESP_GMF_MAX_DELAY);
    out_load->valid_size = rsize;
    esp_gmf_port_release_out(out_port, out_load, ESP_GMF_MAX_DELAY);
    ret = ESP_GMF_JOB_ERR_OK;
  } else {
    ret = ESP_GMF_JOB_ERR_CONTINUE;
  }
  if (in_load->is_done) {
    ret = ESP_GMF_JOB_ERR_DONE;
  }
  esp_gmf_port_release_in(in_port, in_load, ESP_GMF_MAX_DELAY);
  return ret;
}

static esp_gmf_err_t _load_afe_caps_func(esp_gmf_element_handle_t handle) {
  esp_gmf_cap_t *caps = NULL;
  esp_gmf_cap_t afe_caps = {0};
  afe_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_AEC;
  afe_caps.attr_fun = NULL;
  int ret = esp_gmf_cap_append(&caps, &afe_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create AEC capability");
  afe_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_AGC;
  ret = esp_gmf_cap_append(&caps, &afe_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create AGC capability");
  afe_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_NS;
  afe_caps.attr_fun = NULL;
  ret = esp_gmf_cap_append(&caps, &afe_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create NS capability");
  afe_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_VAD;
  afe_caps.attr_fun = NULL;
  ret = esp_gmf_cap_append(&caps, &afe_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create VAD capability");
  afe_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_WWE;
  afe_caps.attr_fun = NULL;
  ret = esp_gmf_cap_append(&caps, &afe_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create WWE capability");
  afe_caps.cap_eightcc = ESP_GMF_CAPS_AUDIO_VCMD;
  afe_caps.attr_fun = NULL;
  ret = esp_gmf_cap_append(&caps, &afe_caps);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to create VCMD capability");

  esp_gmf_element_t *el = (esp_gmf_element_t *) handle;
  el->caps = caps;
  return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t __afe_set_vcmd_det(esp_gmf_audio_element_handle_t handle, esp_gmf_args_desc_t *arg_desc,
                                        uint8_t *buf, int buf_len) {
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, arg_desc, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, buf, { return ESP_GMF_ERR_INVALID_ARG; });

  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) handle;
  esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(gmf_afe);
  uint8_t vcmd_det_start = buf[0];
  if (cfg->vcmd_detect_en) {
    if (vcmd_det_start) {
      vcmd_det_begin(gmf_afe);
    } else {
      vcmd_det_cancel(gmf_afe);
    }
    return ESP_GMF_ERR_OK;
  }
  return ESP_GMF_ERR_INVALID_STATE;
}

static esp_gmf_err_t _load_afe_methods_func(esp_gmf_element_handle_t handle) {
  esp_gmf_method_t *method = NULL;
  esp_gmf_args_desc_t *set_args = NULL;
  esp_gmf_err_t ret = esp_gmf_args_desc_append(&set_args, ESP_GMF_METHOD_AFE_START_VCMD_DET_ARG_EN,
                                               ESP_GMF_ARGS_TYPE_UINT8, sizeof(uint8_t), 0);
  ESP_GMF_RET_ON_NOT_OK(
      TAG, ret, { return ret; }, "Failed to append vcmd det argument");
  ret = esp_gmf_method_append(&method, ESP_GMF_METHOD_AFE_START_VCMD_DET, __afe_set_vcmd_det, set_args);
  ESP_GMF_RET_ON_ERROR(
      TAG, ret, { return ret; }, "Failed to register %s method", ESP_GMF_METHOD_AFE_START_VCMD_DET);

  esp_gmf_element_t *el = (esp_gmf_element_t *) handle;
  el->method = method;
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_afe_init(void *config, esp_gmf_obj_handle_t *handle) {
  ESP_GMF_MEM_CHECK(TAG, config, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_MEM_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  ESP_GMF_NULL_CHECK(TAG, ((esp_gmf_afe_cfg_t *) config)->afe_manager, { return ESP_GMF_ERR_INVALID_ARG; })

  esp_gmf_afe_t *gmf_afe = esp_gmf_oal_calloc(1, sizeof(esp_gmf_afe_t));
  ESP_GMF_MEM_CHECK(TAG, gmf_afe, { return ESP_GMF_ERR_MEMORY_LACK; });
  esp_gmf_obj_t *obj = (esp_gmf_obj_t *) gmf_afe;
  obj->new_obj = esp_gmf_afe_new;
  obj->del_obj = esp_gmf_afe_destroy;

  esp_gmf_afe_cfg_t *obj_cfg = esp_gmf_oal_calloc(1, sizeof(esp_gmf_afe_cfg_t));
  memcpy(obj_cfg, config, sizeof(esp_gmf_afe_cfg_t));
  esp_gmf_obj_set_config(obj, obj_cfg, sizeof(esp_gmf_afe_cfg_t));
  int ret = esp_gmf_obj_set_tag(obj, "ai_afe");
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed set OBJ tag");

  esp_gmf_element_cfg_t el_cfg = {0};
  ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(el_cfg.in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                   ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, AFE_DEFAULT_DATA_SIZE);
  ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(el_cfg.out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                    ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, AFE_DEFAULT_DATA_SIZE);
  el_cfg.dependency = false;
  ret = esp_gmf_audio_el_init(gmf_afe, &el_cfg);
  ESP_GMF_RET_ON_NOT_OK(TAG, ret, goto __failed, "Failed to initialize audio element");
  gmf_afe->last_event = ET_UNKNOWN;
  ESP_GMF_ELEMENT_GET(gmf_afe)->ops.open = esp_gmf_afe_open;
  ESP_GMF_ELEMENT_GET(gmf_afe)->ops.process = esp_gmf_afe_proc;
  ESP_GMF_ELEMENT_GET(gmf_afe)->ops.close = esp_gmf_afe_close;
  ESP_GMF_ELEMENT_GET(gmf_afe)->ops.load_caps = _load_afe_caps_func;
  ESP_GMF_ELEMENT_GET(gmf_afe)->ops.load_methods = _load_afe_methods_func;

  *handle = obj;
  ESP_LOGI(TAG, "Create AFE, %s-%p", OBJ_GET_TAG(obj), obj);

  return ESP_GMF_ERR_OK;

__failed:
  esp_gmf_afe_destroy(gmf_afe);
  return ESP_GMF_ERR_FAIL;
}

static esp_gmf_err_t esp_gmf_afe_set_vcmd_detection(esp_gmf_element_handle_t handle, bool enable) {
  ESP_GMF_NULL_CHECK(TAG, handle, { return ESP_GMF_ERR_INVALID_ARG; });
  const esp_gmf_method_t *method_head = NULL;
  const esp_gmf_method_t *method = NULL;
  esp_gmf_element_get_method((esp_gmf_element_handle_t) handle, &method_head);
  esp_gmf_method_found(method_head, ESP_GMF_METHOD_AFE_START_VCMD_DET, &method);
  uint8_t buf[1] = {enable};
  esp_gmf_args_set_value(method->args_desc, ESP_GMF_METHOD_AFE_START_VCMD_DET_ARG_EN, buf, buf, sizeof(uint8_t));
  return esp_gmf_element_exe_method((esp_gmf_element_handle_t) handle, ESP_GMF_METHOD_AFE_START_VCMD_DET, buf,
                                    sizeof(buf));
}

esp_gmf_err_t esp_gmf_afe_vcmd_detection_begin(esp_gmf_element_handle_t handle) {
  return esp_gmf_afe_set_vcmd_detection(handle, true);
}

esp_gmf_err_t esp_gmf_afe_vcmd_detection_cancel(esp_gmf_element_handle_t handle) {
  return esp_gmf_afe_set_vcmd_detection(handle, false);
}

esp_gmf_err_t esp_gmf_afe_set_event_cb(esp_gmf_element_handle_t handle, esp_gmf_afe_event_cb_t cb, void *ctx) {
  ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG;);
  esp_gmf_afe_cfg_t *cfg = OBJ_GET_CFG(handle);
  ESP_GMF_NULL_CHECK(TAG, cfg, return ESP_GMF_ERR_INVALID_STATE;)
  cfg->event_cb = cb;
  cfg->event_ctx = ctx;
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_afe_keep_awake(esp_gmf_element_handle_t handle, bool enable) {
  ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG;);
  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) handle;
  if (gmf_afe->msg_queue == NULL) {
    ESP_LOGE(TAG, "GMF AFE Message queue not initialized");
    return ESP_GMF_ERR_INVALID_STATE;
  }
  afe_evt_msg_t msg = {0};
  msg.event = ET_KEEP_WAKE_MODIFIED;
  msg.payload.keep_awake.enable = enable;
  BaseType_t ok = xQueueSend(gmf_afe->msg_queue, &msg, pdMS_TO_TICKS(1000));
  return ok == pdTRUE ? ESP_GMF_ERR_OK : ESP_GMF_ERR_TIMEOUT;
}

esp_gmf_err_t esp_gmf_afe_trigger_wakeup(esp_gmf_element_handle_t handle) {
  ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG;);
  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) handle;
  if (gmf_afe->msg_queue == NULL) {
    ESP_LOGE(TAG, "GMF AFE Message queue not initialized");
    return ESP_GMF_ERR_INVALID_STATE;
  }
  afe_evt_msg_t msg = {0};
  msg.event = ET_TRIGGER_WAKEUP;
  msg.payload.trigger.state = true;
  BaseType_t ok = xQueueSend(gmf_afe->msg_queue, &msg, pdMS_TO_TICKS(1000));
  return ok == pdTRUE ? ESP_GMF_ERR_OK : ESP_GMF_ERR_TIMEOUT;
}

esp_gmf_err_t esp_gmf_afe_trigger_sleep(esp_gmf_element_handle_t handle) {
  ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG;);
  esp_gmf_afe_t *gmf_afe = (esp_gmf_afe_t *) handle;
  if (gmf_afe->msg_queue == NULL) {
    ESP_LOGE(TAG, "GMF AFE Message queue not initialized");
    return ESP_GMF_ERR_INVALID_STATE;
  }
  afe_evt_msg_t msg = {0};
  msg.event = ET_TRIGGER_SLEEP;
  msg.payload.trigger.state = false;
  BaseType_t ok = xQueueSend(gmf_afe->msg_queue, &msg, pdMS_TO_TICKS(1000));
  return ok == pdTRUE ? ESP_GMF_ERR_OK : ESP_GMF_ERR_TIMEOUT;
}
