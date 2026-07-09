/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "esp_private/freertos_idf_additions_priv.h"
#include "esp_afe_sr_models.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_gmf_afe_manager.h"

#define AFE_RUN_EVENT (BIT0)
#define AFE_DESTROYING_EVENT (BIT1)
#define FEED_TASK_DESTROYED (BIT2)
#define FETCH_TASK_DESTROYED (BIT3)

/**
 * @brief  Information about an AFE (Audio Front-End) task.
 */
typedef struct {
  TaskHandle_t task; /*!< Task handle */
  bool running;      /*!< Task running status */
} afe_task_info_t;

/**
 * @brief  AFE (Audio Front-End) manager.
 */
typedef struct __afe {
  afe_task_info_t feed;                        /*!< Feed task information */
  afe_task_info_t fetch;                       /*!< Fetch task information */
  esp_gmf_afe_manager_read_cb_t read_cb;       /*!< Read callback function */
  void *read_ctx;                              /*!< Read callback context */
  esp_afe_sr_data_t *afe_data;                 /*!< AFE data */
  esp_gmf_afe_manager_result_cb_t result_proc; /*!< Result processing callback function */
  void *result_ctx;                            /*!< Result processing callback context */
  esp_gmf_afe_manager_features_t feat;         /*!< AFE feature state */
  EventGroupHandle_t ctrl_events;              /*!< Control events */
  SemaphoreHandle_t read_cb_lock;              /*!< Mutex for read callback */
  SemaphoreHandle_t result_cb_lock;            /*!< Mutex for result callback */
  const esp_afe_sr_iface_t *esp_afe;           /*!< AFE interface */
} esp_gmf_afe_manager_t;

static const char *TAG = "AFE_MANAGER";

static void feed_task(void *arg) {
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) arg;
  int chan_num = afe_manager->esp_afe->get_feed_channel_num(afe_manager->afe_data);
  int chunksize = afe_manager->esp_afe->get_feed_chunksize(afe_manager->afe_data);
  const int buf_size = chunksize * chan_num * sizeof(uint16_t);
  ESP_LOGI(TAG, "Feed task, ch %d, chunk %d, buf size %d", chan_num, chunksize, buf_size);
  int16_t *buf = heap_caps_calloc_prefer(1, buf_size, 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_INTERNAL);
  if (buf == NULL) {
    ESP_LOGE(TAG, "Feed task calloc failed, task quit");
    goto __quit;
  }
  afe_manager->feed.running = true;

  while (afe_manager->feed.running) {
    EventBits_t bits = xEventGroupWaitBits(afe_manager->ctrl_events, AFE_RUN_EVENT | AFE_DESTROYING_EVENT, false, false,
                                           portMAX_DELAY);
    if (bits & AFE_DESTROYING_EVENT) {
      break;
    }
    int rlen = 0;
    xSemaphoreTake(afe_manager->read_cb_lock, portMAX_DELAY);
    if (afe_manager->read_cb) {
      rlen = afe_manager->read_cb(buf, buf_size, afe_manager->read_ctx, pdMS_TO_TICKS(500));
    }
    xSemaphoreGive(afe_manager->read_cb_lock);
    if (rlen == buf_size) {
      afe_manager->esp_afe->feed(afe_manager->afe_data, buf);
    } else {
      ESP_LOGE(TAG, "AFE read failed %d", rlen);
    }
  }

__quit:
  if (buf) {
    heap_caps_free(buf);
  }
  xEventGroupSetBits(afe_manager->ctrl_events, FEED_TASK_DESTROYED);
  vTaskDelete(NULL);
}

static void fetch_task(void *arg) {
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) arg;
  afe_manager->fetch.running = true;

  while (afe_manager->fetch.running) {
    EventBits_t bits = xEventGroupWaitBits(afe_manager->ctrl_events, AFE_RUN_EVENT | AFE_DESTROYING_EVENT, false, false,
                                           portMAX_DELAY);
    if (bits & AFE_DESTROYING_EVENT) {
      break;
    }
    afe_fetch_result_t *result = afe_manager->esp_afe->fetch(afe_manager->afe_data);
    xSemaphoreTake(afe_manager->result_cb_lock, portMAX_DELAY);
    if (afe_manager->result_proc != NULL) {
      afe_manager->result_proc(result, afe_manager->result_ctx);
    }
    xSemaphoreGive(afe_manager->result_cb_lock);
  }
  xEventGroupSetBits(afe_manager->ctrl_events, FETCH_TASK_DESTROYED);
  vTaskDelete(NULL);
}

esp_gmf_err_t esp_gmf_afe_manager_set_result_cb(esp_gmf_afe_manager_handle_t handle,
                                                esp_gmf_afe_manager_result_cb_t proc, void *ctx) {
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) handle;
  ESP_RETURN_ON_FALSE(afe_manager, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE set result cb invalid handle");
  xSemaphoreTake(afe_manager->result_cb_lock, portMAX_DELAY);
  afe_manager->result_proc = proc;
  afe_manager->result_ctx = ctx;
  xSemaphoreGive(afe_manager->result_cb_lock);
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_afe_manager_destroy(esp_gmf_afe_manager_handle_t handle) {
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) handle;
  ESP_RETURN_ON_FALSE(afe_manager, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE destroy: handle NULL");

  xEventGroupSetBits(afe_manager->ctrl_events, AFE_DESTROYING_EVENT);
  EventBits_t wait_bits = 0;
  if (afe_manager->feed.task) {
    afe_manager->feed.running = false;
    wait_bits |= FEED_TASK_DESTROYED;
  }
  if (afe_manager->fetch.task) {
    afe_manager->fetch.running = false;
    wait_bits |= FETCH_TASK_DESTROYED;
  }
  EventBits_t bits = xEventGroupWaitBits(afe_manager->ctrl_events, wait_bits, true, true, portMAX_DELAY);
  if ((bits & wait_bits) != wait_bits) {
    ESP_LOGE(TAG, "AFE destroy wait bits timeout %" PRIu32, bits);
    return ESP_GMF_ERR_TIMEOUT;
  }
  if (afe_manager->ctrl_events) {
    vEventGroupDelete(afe_manager->ctrl_events);
  }
  if (afe_manager->read_cb_lock) {
    vSemaphoreDelete(afe_manager->read_cb_lock);
  }
  if (afe_manager->result_cb_lock) {
    vSemaphoreDelete(afe_manager->result_cb_lock);
  }
  if (afe_manager->afe_data) {
    afe_manager->esp_afe->destroy(afe_manager->afe_data);
    afe_manager->afe_data = NULL;
  }
  heap_caps_free(afe_manager);
  ESP_LOGI(TAG, "AFE manager destroy");
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_afe_manager_create(esp_gmf_afe_manager_cfg_t *cfg, esp_gmf_afe_manager_handle_t *handle) {
  esp_gmf_err_t ret = ESP_GMF_ERR_OK;
  *handle = NULL;

  ESP_RETURN_ON_FALSE(cfg != NULL, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE manager create: cfg NULL");
  ESP_RETURN_ON_FALSE(handle != NULL, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE manager create: handle NULL");
  ESP_RETURN_ON_FALSE(cfg->afe_cfg != NULL, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE manager create: cfg->afe_cfg NULL");

  esp_gmf_afe_manager_t *afe_manager =
      heap_caps_calloc_prefer(1, sizeof(esp_gmf_afe_manager_t), 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_INTERNAL);
  ESP_GOTO_ON_FALSE(afe_manager, ESP_GMF_ERR_MEMORY_LACK, __err, TAG, "AFE manager create no memory");
  afe_manager->read_cb = cfg->read_cb;
  afe_manager->read_ctx = cfg->read_ctx;
  afe_manager->result_proc = cfg->result_cb;
  afe_manager->result_ctx = cfg->result_ctx;
  afe_config_print(cfg->afe_cfg);
  afe_manager->esp_afe = esp_afe_handle_from_config(cfg->afe_cfg);
  afe_manager->afe_data = afe_manager->esp_afe->create_from_config(cfg->afe_cfg);
  ESP_GOTO_ON_FALSE(afe_manager->afe_data, ESP_GMF_ERR_FAIL, __err, TAG, "AFE manager create failed");

  afe_manager->ctrl_events = xEventGroupCreate();
  ESP_GOTO_ON_FALSE(afe_manager->ctrl_events, ESP_GMF_ERR_MEMORY_LACK, __err, TAG, "AFE manager create events failed");

  afe_manager->read_cb_lock = xSemaphoreCreateMutex();
  ESP_GOTO_ON_FALSE(afe_manager->read_cb_lock, ESP_GMF_ERR_MEMORY_LACK, __err, TAG, "AFE manager create mutex failed");
  afe_manager->result_cb_lock = xSemaphoreCreateMutex();
  ESP_GOTO_ON_FALSE(afe_manager->result_cb_lock, ESP_GMF_ERR_MEMORY_LACK, __err, TAG,
                    "AFE manager create mutex failed");

  if (afe_manager->read_cb) {
    xEventGroupSetBits(afe_manager->ctrl_events, AFE_RUN_EVENT);
  }

#if (configSUPPORT_STATIC_ALLOCATION == 1) && defined(CONFIG_SPIRAM_BOOT_INIT)
  prvTaskCreateDynamicPinnedToCoreWithCaps(feed_task, "afe_feed", cfg->feed_task_setting.stack_size, afe_manager,
                                           cfg->feed_task_setting.prio, cfg->feed_task_setting.core,
                                           (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT), &afe_manager->feed.task);
  ESP_GOTO_ON_FALSE(afe_manager->feed.task, ESP_GMF_ERR_MEMORY_LACK, __err, TAG, "Create afe feed task failed");

  prvTaskCreateDynamicPinnedToCoreWithCaps(fetch_task, "afe_fetch", cfg->fetch_task_setting.stack_size, afe_manager,
                                           cfg->fetch_task_setting.prio, cfg->fetch_task_setting.core,
                                           (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT), &afe_manager->fetch.task);
  ESP_GOTO_ON_FALSE(afe_manager->fetch.task, ESP_GMF_ERR_MEMORY_LACK, __err, TAG, "Create afe fetch task failed");
#else
  ESP_LOGW(TAG, "The AFE Manager may not work when PSRAM is disabled");

  xTaskCreatePinnedToCore(feed_task, "afe_feed", cfg->feed_task_setting.stack_size, afe_manager,
                          cfg->feed_task_setting.prio, &afe_manager->feed.task, cfg->feed_task_setting.core);
  ESP_GOTO_ON_FALSE(afe_manager->feed.task, ESP_GMF_ERR_MEMORY_LACK, __err, TAG, "Create afe feed task failed");

  xTaskCreatePinnedToCore(fetch_task, "afe_fetch", cfg->fetch_task_setting.stack_size, afe_manager,
                          cfg->fetch_task_setting.prio, &afe_manager->fetch.task, cfg->fetch_task_setting.core);
  ESP_GOTO_ON_FALSE(afe_manager->fetch.task, ESP_GMF_ERR_MEMORY_LACK, __err, TAG, "Create afe fetch task failed");
#endif /* (configSUPPORT_STATIC_ALLOCATION == 1) */

  afe_manager->feat.wakeup = cfg->afe_cfg->wakenet_init;
  afe_manager->feat.aec = cfg->afe_cfg->aec_init;
  afe_manager->feat.se = cfg->afe_cfg->se_init;
  afe_manager->feat.vad = (cfg->afe_cfg->vad_init);

  *handle = afe_manager;
  return ret;
__err:
  if (afe_manager) {
    esp_gmf_afe_manager_destroy(afe_manager);
  }
  return ret;
}

esp_gmf_err_t esp_gmf_afe_manager_get_features(esp_gmf_afe_manager_handle_t handle,
                                               esp_gmf_afe_manager_features_t *features) {
  ESP_RETURN_ON_FALSE(handle, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE get feature: handle NULL");
  ESP_RETURN_ON_FALSE(features, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE get feature: features NULL");
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) handle;
  memcpy(features, &(afe_manager->feat), sizeof(esp_gmf_afe_manager_features_t));
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_afe_manager_suspend(esp_gmf_afe_manager_handle_t handle, bool suspend) {
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) handle;
  ESP_RETURN_ON_FALSE(afe_manager, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE suspend: handle NULL");
  ESP_LOGI(TAG, "AFE manager suspend %d", suspend);
  if (suspend) {
    xEventGroupClearBits(afe_manager->ctrl_events, AFE_RUN_EVENT);
  } else {
    xEventGroupSetBits(afe_manager->ctrl_events, AFE_RUN_EVENT);
  }

  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_afe_manager_enable_features(esp_gmf_afe_manager_handle_t handle, esp_gmf_afe_feature_t feature,
                                                  bool enable) {
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) handle;
  ESP_RETURN_ON_FALSE(afe_manager, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE suspend: handle NULL");
  esp_gmf_err_t ret = ESP_GMF_ERR_OK;
  ESP_LOGD(TAG, "AFE Ctrl [%u, %d]", feature, enable);
  switch (feature) {
    case ESP_AFE_FEATURE_WAKENET: {
      if (enable) {
        ret = afe_manager->esp_afe->enable_wakenet(afe_manager->afe_data);
      } else {
        ret = afe_manager->esp_afe->disable_wakenet(afe_manager->afe_data);
      }
      ESP_LOGD(TAG, "Wakenet ctrl ret %d", ret);
      if (ret >= 0) {
        afe_manager->feat.wakeup = ret;
      }
      break;
    }
    case ESP_AFE_FEATURE_AEC: {
      if (enable) {
        ret = afe_manager->esp_afe->enable_aec(afe_manager->afe_data);
      } else {
        ret = afe_manager->esp_afe->disable_aec(afe_manager->afe_data);
      }
      ESP_LOGD(TAG, "AEC ctrl ret %d", ret);
      if (ret >= 0) {
        afe_manager->feat.aec = ret;
      }
      break;
    }
    case ESP_AFE_FEATURE_SE: {
      if (enable) {
        ret = afe_manager->esp_afe->enable_se(afe_manager->afe_data);
      } else {
        ret = afe_manager->esp_afe->disable_se(afe_manager->afe_data);
      }
      ESP_LOGD(TAG, "AE ctrl ret %d", ret);
      if (ret >= 0) {
        afe_manager->feat.se = ret;
      }
      break;
    }
    case ESP_AFE_FEATURE_VAD: {
      if (enable) {
        ret = afe_manager->esp_afe->enable_vad(afe_manager->afe_data);
      } else {
        ret = afe_manager->esp_afe->disable_vad(afe_manager->afe_data);
      }
      ESP_LOGD(TAG, "VAD ctrl ret %d", ret);
      if (ret >= 0) {
        afe_manager->feat.vad = ret;
      }
      ret = afe_manager->esp_afe->reset_vad(afe_manager->afe_data);
      break;
    }
    default:
      ESP_LOGW(TAG, "AFE manager feature ctrl: %u not support", feature);
      ret = ESP_GMF_ERR_INVALID_ARG;
      break;
  }

  return ret;
}

esp_gmf_err_t esp_gmf_afe_manager_set_read_cb(esp_gmf_afe_manager_handle_t handle,
                                              esp_gmf_afe_manager_read_cb_t read_cb, void *read_ctx) {
  ESP_RETURN_ON_FALSE(handle, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE Manager set read: handle NULL");
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) handle;
  esp_gmf_afe_manager_suspend(afe_manager, true);
  xSemaphoreTake(afe_manager->read_cb_lock, portMAX_DELAY);
  afe_manager->read_cb = read_cb;
  afe_manager->read_ctx = read_ctx;
  if (afe_manager->read_cb) {
    esp_gmf_afe_manager_suspend(afe_manager, false);
  }
  xSemaphoreGive(afe_manager->read_cb_lock);
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_afe_manager_get_chunk_size(esp_gmf_afe_manager_handle_t handle, size_t *size) {
  ESP_RETURN_ON_FALSE(handle, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE Manager get feed size: handle NULL");
  ESP_RETURN_ON_FALSE(size, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE Manager get feed size: size NULL");
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) handle;
  *size = afe_manager->esp_afe->get_feed_chunksize(afe_manager->afe_data);
  return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_afe_manager_get_input_ch_num(esp_gmf_afe_manager_handle_t handle, uint8_t *ch_num) {
  ESP_RETURN_ON_FALSE(handle, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE Manager get feed size: handle NULL");
  ESP_RETURN_ON_FALSE(ch_num, ESP_GMF_ERR_INVALID_ARG, TAG, "AFE Manager get feed size: ch_num NULL");
  esp_gmf_afe_manager_t *afe_manager = (esp_gmf_afe_manager_t *) handle;
  *ch_num = afe_manager->esp_afe->get_feed_channel_num(afe_manager->afe_data);
  return ESP_GMF_ERR_OK;
}
