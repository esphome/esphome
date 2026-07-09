// namespace esp_afe marker for ESPHome lint; vendored GMF C API remains global.
/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_afe_manager.h"
#include "esp_gmf_err.h"
#include "esp_gmf_element.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* This element can run with esp32, esp32s3 and esp32p4 */

#define ESP_GMF_AFE_VCMD_MAX_LEN (256)

#define ESP_GMF_AFE_DEFAULT_DELAY_SAMPLES (2048)
#define ESP_GMF_AFE_DEFAULT_WAKEUP_TIME_MS (10000)
#define ESP_GMF_AFE_DEFAULT_WAKEUP_END_MS (2000)
#define ESP_GMF_AFE_DEFAULT_VCMD_TIMEOUT_MS (5760)

/**
 * @brief  Information when wakeup state detected, event data for "ESP_GMF_AFE_EVT_WAKEUP_START"
 */
typedef struct {
  float data_volume;       /*!< Volume of input audio, the unit is decibel(dB) */
  int wake_word_index;     /*!< Wake word index which start from 1 */
  int wakenet_model_index; /*!< Wakenets index which start from 1 */
} esp_gmf_afe_wakeup_info_t;

/**
 * @brief  Information when voice command detected, event data for `ESP_GMF_AFE_EVT_VCMD_DECTECTED`
 */
typedef struct {
  int phrase_id;                      /*!< Phrase ID */
  float prob;                         /*!< probability */
  char str[ESP_GMF_AFE_VCMD_MAX_LEN]; /*!< Command string */
} esp_gmf_afe_vcmd_info_t;

/**
 * @brief  AFE manager event type
 */
typedef enum {
  ESP_GMF_AFE_EVT_WAKEUP_START = -100, /*!< Wakeup start */
  ESP_GMF_AFE_EVT_WAKEUP_END,          /*!< Wakeup stop */
  ESP_GMF_AFE_EVT_VAD_START,           /*!< Vad start */
  ESP_GMF_AFE_EVT_VAD_END,             /*!< Vad stop */
  ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT,   /*!< Voice command detect timeout */
  ESP_GMF_AFE_EVT_VCMD_DECTECTED = 0   /*!< Form 0 is the id of the voice commands detected by Multinet*/
                                       /* DO NOT add items below this line */
} esp_gmf_afe_event_t;

/**
 * @brief  Event structure for GMF AFE
 */
typedef struct {
  esp_gmf_afe_event_t type; /*!< Event type */
  void *event_data;         /*!< Event data */
  size_t data_len;          /*!< Length of event data */
} esp_gmf_afe_evt_t;

/**
 * @brief  Callback type for GMF AFE events
 */
typedef void (*esp_gmf_afe_event_cb_t)(esp_gmf_element_handle_t el, esp_gmf_afe_evt_t *event, void *user_data);

/**
 * @brief  Configuration structure for GMF AFE wrapper
 */
typedef struct {
  esp_gmf_afe_manager_handle_t afe_manager; /*!< AFE Manager handle */
  uint32_t delay_samples;                   /*!< Number of samples to delay
                                                 Note: If the user wants to using the output of AFE only after detecting the VAD start
                                               event,                   the time corresponding to the value of this parameter should not
                                               be less than the                   `vad_min_speech_ms` in `afe_config_t` used when creating
                                               the                   afe_manager,       otherwise, a                   small portion of
                                               the data at the                   beginning of the                   voice may be lost */
  void *models;                             /*!< List of models */
  uint32_t wakeup_time;    /*!< Unit:ms. The duration that the wakeup state remains when VAD is not triggered */
  uint32_t wakeup_end;     /*!< Unit:ms. When the silence time after AUDIO_REC_VAD_END state exceeds this value, it is
                              determined as AUDIO_REC_WAKEUP_END */
  bool vcmd_detect_en;     /*!< Enable voice command detection */
  uint32_t vcmd_timeout;   /*!< Timeout for voice command detection, units: ms*/
  const char *mn_language; /*!< Language for the multi-net model, `cn` or `en` */
  esp_gmf_afe_event_cb_t event_cb; /*!< Callback function for AI audio events */
  void *event_ctx;                 /*!< User context to be passed to the callback function */
} esp_gmf_afe_cfg_t;

#define DEFAULT_GMF_AFE_CFG(__afe_manager, __event_cb, __event_ctx, __models) \
  { \
    .afe_manager = __afe_manager,                            /*   */ \
        .models = __models,                                  /*   */ \
        .delay_samples = ESP_GMF_AFE_DEFAULT_DELAY_SAMPLES,  /* Default delay samples, 64ms */ \
        .wakeup_time = ESP_GMF_AFE_DEFAULT_WAKEUP_TIME_MS,   /* Default wakeup duration */ \
        .wakeup_end = ESP_GMF_AFE_DEFAULT_WAKEUP_END_MS,     /* Default wakeup end time */ \
        .vcmd_detect_en = false,                             /*   */ \
        .vcmd_timeout = ESP_GMF_AFE_DEFAULT_VCMD_TIMEOUT_MS, /*   */ \
        .mn_language = "cn",                                 /* Default language `cn` */ \
        .event_cb = __event_cb,                              /*   */ \
        .event_ctx = __event_ctx,                            /*   */ \
  }

/**
 * @brief  Initialize the GMF AFE
 *
 * @param[in]   config  Pointer to the configuration structure
 * @param[out]  handle  Pointer to the handle to be created
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_afe_init(void *config, esp_gmf_obj_handle_t *handle);

/**
 * @brief  Begin voice command detection
 *
 * @param[in]  handle  Handle to the GMF object
 *
 * @return
 *       - ESP_GMF_ERR_OK             Success
 *       - ESP_GMF_ERR_INVALID_ARG    Invalid argument
 *       - ESP_GMF_ERR_INVALID_STATE  Voice command not enabled
 */
esp_gmf_err_t esp_gmf_afe_vcmd_detection_begin(esp_gmf_element_handle_t handle);

/**
 * @brief  Cancel voice command detection
 *
 * @note  This function is used to clear the states of voice command detection process,
 *        the voice command detection will stay enabled,
 *        and the user can call `esp_gmf_afe_vcmd_detection_begin` to start the detection again
 *
 * @param[in]  handle  Handle to the GMF object
 *
 * @return
 *       - ESP_GMF_ERR_OK             Success
 *       - ESP_GMF_ERR_INVALID_ARG    Invalid argument
 *       - ESP_GMF_ERR_INVALID_STATE  Voice command not enabled
 */
esp_gmf_err_t esp_gmf_afe_vcmd_detection_cancel(esp_gmf_element_handle_t handle);

/**
 * @brief  Set the event callback for the AFE (Audio Front-End) element
 *
 *         This function registers a callback function to handle events generated by the
 *         AFE element. The callback will be invoked with the specified context whenever
 *         an event occurs
 *
 * @param  handle  The handle to the AFE element
 * @param  cb      The callback function to handle AFE events
 * @param  ctx     User-defined context to be passed to the callback function
 *
 * @return
 *       - ESP_GMF_ERR_OK             Success
 *       - ESP_GMF_ERR_INVALID_ARG    Invalid argument
 *       - ESP_GMF_ERR_INVALID_STATE  Config not exist
 */
esp_gmf_err_t esp_gmf_afe_set_event_cb(esp_gmf_element_handle_t handle, esp_gmf_afe_event_cb_t cb, void *ctx);

/**
 * @brief  Enable or disable keep-awake mode
 *
 *         When keep-awake mode is enabled, the system will remain in the wake state
 *         and prevent wakeup_end events from being triggered automatically
 *         This is useful for scenarios where you want to keep the system active
 *         without automatic timeout
 *
 * @param  handle  The handle to the AFE element
 * @param  enable  True to enable keep-awake mode, false to disable
 *
 * @return
 *       - ESP_GMF_ERR_OK             Success
 *       - ESP_GMF_ERR_INVALID_ARG    Invalid argument
 *       - ESP_GMF_ERR_INVALID_STATE  Config not exist
 *       - ESP_GMF_ERR_TIMEOUT        Command send timeout
 */
esp_gmf_err_t esp_gmf_afe_keep_awake(esp_gmf_element_handle_t handle, bool enable);

/**
 * @brief  Manually trigger wakeup state
 *
 *         This function allows manual activation of the wakeup state without waiting
 *         for automatic wakeword detection. It is useful in the following scenarios:
 *
 *         1. **Button-triggered activation**: When users press a physical button to
 *            activate voice interaction, bypassing the need for wakewords
 *         2. **External event-driven activation**: When the system needs to enter
 *            wakeup state based on external triggers (sensors, timers, network events)
 *
 *         After calling this function, the AFE will enter wakeup state and begin
 *         listening for voice commands (if voice command detection is enabled).
 *         The system will generate ESP_GMF_AFE_EVT_WAKEUP_START event and remain
 *         active according to the configured wakeup_time duration.
 *
 * @param[in]  handle  Handle to the GMF object
 *
 * @return
 *       - ESP_GMF_ERR_OK             Success
 *       - ESP_GMF_ERR_INVALID_ARG    Invalid argument
 *       - ESP_GMF_ERR_INVALID_STATE  Element not opened
 *       - ESP_GMF_ERR_TIMEOUT        Command send timeout
 */
esp_gmf_err_t esp_gmf_afe_trigger_wakeup(esp_gmf_element_handle_t handle);

/**
 * @brief  Manually trigger sleep of wakeup state
 *
 * @param[in]  handle  Handle to the GMF object
 *
 * @return
 *       - ESP_GMF_ERR_OK             Success
 *       - ESP_GMF_ERR_INVALID_ARG    Invalid argument
 *       - ESP_GMF_ERR_INVALID_STATE  Element not opened
 *       - ESP_GMF_ERR_TIMEOUT        Command send timeout
 */
esp_gmf_err_t esp_gmf_afe_trigger_sleep(esp_gmf_element_handle_t handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */
