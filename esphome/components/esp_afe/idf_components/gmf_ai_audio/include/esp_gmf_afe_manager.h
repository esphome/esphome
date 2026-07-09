// namespace esp_afe marker for ESPHome lint; vendored GMF C API remains global.
/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_afe_sr_iface.h"
#include "esp_gmf_err.h"

/**
 * The AFE Manager aims to provide users with a simple interface for managing AFE (Audio front end) functions,
 * including WakeNet, VAD, AEC, SE, and more
 * This component will automatically create feed and fetch tasks,
 * users only need to provide data read callback functions and result processing callback functions
 * Users can configure `AFE` functions through the `afe_config_t` structure
 * The data fed into `AFE` must be in 16-bit PCM format with a sampling rate of 16kHz,
 * the number of channels and channel arrangement are determined by the configuration in the `afe_config_init` function,
 * for details, please refer to the description of the `afe_config_init` function which provide by `esp-sr`
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ESP_AFE_MANAGER_FEED_TASK_CORE (0)
#define ESP_AFE_MANAGER_FEED_TASK_PRIO (5)
#define ESP_AFE_MANAGER_FEED_TASK_STACK (3 * 1024)
#define ESP_AFE_MANAGER_FETCH_TASK_CORE (1)
#define ESP_AFE_MANAGER_FETCH_TASK_PRIO (5)
#define ESP_AFE_MANAGER_FETCH_TASK_STACK (3 * 1024)

/**
 * @enum
 * @brief  Enumeration of AFE functions
 */
typedef enum {
  ESP_AFE_FEATURE_WAKENET, /*!< WakeNet function */
  ESP_AFE_FEATURE_VAD,     /*!< Voice Activity Detection function */
  ESP_AFE_FEATURE_AEC,     /*!< Acoustic Echo Cancellation function */
  ESP_AFE_FEATURE_SE,      /*!< Speech Enhancement function */
} esp_gmf_afe_feature_t;

/**
 * @brief  Handle for the AFE manager
 */
typedef void *esp_gmf_afe_manager_handle_t;

/**
 * @brief  Callback function type for processing AFE results
 *
 * @param[in]  result    Pointer to the result structure
 * @param[in]  user_ctx  User context to be passed to the callback function
 */
typedef void (*esp_gmf_afe_manager_result_cb_t)(afe_fetch_result_t *result, void *user_ctx);

/**
 * @brief  Callback type for reading data
 *
 * @param[in]  buffer    Pointer to the buffer to read data into
 * @param[in]  buf_sz    Size of the buffer
 * @param[in]  user_ctx  User context to be passed to the callback function
 * @param[in]  ticks     Number of ticks to wait for data
 *
 * @return
 */
typedef int32_t (*esp_gmf_afe_manager_read_cb_t)(void *buffer, int buf_sz, void *user_ctx, uint32_t ticks);

/**
 * @brief  Configuration structure for the task setting
 */
typedef struct {
  uint32_t stack_size; /*!< Task stack size */
  uint8_t core;        /*!< Task core id */
  uint8_t prio;        /*!< Task priority */
} esp_gmf_afe_manager_task_setting_t;

/**
 * @brief  Configuration structure for the AFE manager
 */
typedef struct {
  afe_config_t *afe_cfg;                                 /*!< Configuration of ESP AFE */
  esp_gmf_afe_manager_task_setting_t feed_task_setting;  /*!< Feed task setting */
  esp_gmf_afe_manager_task_setting_t fetch_task_setting; /*!< Fetch task setting */
  esp_gmf_afe_manager_read_cb_t read_cb;                 /*!< Callback function for reading audio data */
  void *read_ctx;                                        /*!< Context for the read callback function */
  esp_gmf_afe_manager_result_cb_t result_cb;             /*!< Callback function for processing AFE results */
  void *result_ctx;                                      /*!< Context for the result callback function */
} esp_gmf_afe_manager_cfg_t;

#define DEFAULT_GMF_AFE_MANAGER_CFG(_afe_cfg, _read_cb, _read_ctx, _result_cb, _result_ctx) \
  { \
    .afe_cfg = _afe_cfg, \
    .feed_task_setting = {.stack_size = ESP_AFE_MANAGER_FEED_TASK_STACK, \
                          .core = ESP_AFE_MANAGER_FEED_TASK_CORE, \
                          .prio = ESP_AFE_MANAGER_FEED_TASK_PRIO}, \
    .fetch_task_setting = {.stack_size = ESP_AFE_MANAGER_FETCH_TASK_STACK, \
                           .core = ESP_AFE_MANAGER_FETCH_TASK_CORE, \
                           .prio = ESP_AFE_MANAGER_FETCH_TASK_PRIO}, \
    .read_cb = _read_cb, .read_ctx = _read_ctx, .result_cb = _result_cb, .result_ctx = _result_ctx \
  }

/**
 * @brief  GMF AFE Manager Feature Configuration
 *
 *         This structure defines the feature enable states for the AFE manager
 *         A value of `true` indicates that the feature is enabled, while `false` indicates it is disabled
 */
typedef struct {
  bool wakeup; /*!< Wake-up detection */
  bool vad;    /*!< Voice Activity Detection (VAD) */
  bool ns;     /*!< Noise Suppression (NS) */
  bool aec;    /*!< Acoustic Echo Cancellation (AEC) */
  bool se;     /*!< Speech Enhancement (SE) */
} esp_gmf_afe_manager_features_t;

/**
 * @brief  Create an AFE Manager instance
 *
 * @param[in]   cfg     Pointer to the AFE manager configuration structure
 * @param[out]  handle  Pointer to the created AFE manager handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_FAIL         Failed to create the AFE manager
 *       - ESP_GMF_ERR_MEMORY_LACK  Insufficient memory allocation
 */
esp_gmf_err_t esp_gmf_afe_manager_create(esp_gmf_afe_manager_cfg_t *cfg, esp_gmf_afe_manager_handle_t *handle);

/**
 * @brief  Destroy an AFE Manager instance
 *
 * @param[in]  handle  AFE manager handle to be destroyed
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid handle
 */
esp_gmf_err_t esp_gmf_afe_manager_destroy(esp_gmf_afe_manager_handle_t handle);

/**
 * @brief  Set the audio input read callback for the AFE Manager
 *
 * @note  If the read callback is set to `NULL`, the AFE Manager will be suspended
 *
 * @param[in]  handle    AFE manager handle
 * @param[in]  read_cb   Function pointer to the read callback
 * @param[in]  read_ctx  User-defined context to be passed to the callback
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_afe_manager_set_read_cb(esp_gmf_afe_manager_handle_t handle,
                                              esp_gmf_afe_manager_read_cb_t read_cb, void *read_ctx);

/**
 * @brief  Register a processing result callback for the AFE Manager
 *
 * @param[in]  handle    AFE manager handle
 * @param[in]  proc_cb   Function pointer to the result callback
 * @param[in]  user_ctx  User-defined context to be passed to the callback
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_afe_manager_set_result_cb(esp_gmf_afe_manager_handle_t handle,
                                                esp_gmf_afe_manager_result_cb_t proc_cb, void *user_ctx);

/**
 * @brief  Suspend or resume the AFE Manager
 *
 * @param[in]  handle   AFE manager handle
 * @param[in]  suspend  `true` to suspend, `false` to resume
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_afe_manager_suspend(esp_gmf_afe_manager_handle_t handle, bool suspend);

/**
 * @brief  Enable or disable specific features in the AFE Manager
 *
 * @param[in]  handle   AFE manager handle
 * @param[in]  feature  Feature to be configured (see `esp_gmf_afe_feature_t`)
 * @param[in]  enable   `true` to enable, `false` to disable
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_afe_manager_enable_features(esp_gmf_afe_manager_handle_t handle, esp_gmf_afe_feature_t feature,
                                                  bool enable);

/**
 * @brief  Retrieve the current feature enable states of the AFE Manager
 *
 * @param[in]   handle    AFE manager handle
 * @param[out]  features  Pointer to a structure to store the feature states
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_afe_manager_get_features(esp_gmf_afe_manager_handle_t handle,
                                               esp_gmf_afe_manager_features_t *features);

/**
 * @brief  Get the processing chunk size for the AFE Manager
 *
 * @note  The chunk size represents the number of audio samples per channel.
 *        The AFE Manager processes data in fixed-size chunks.
 *
 * @param[in]   handle  AFE manager handle
 * @param[out]  size    Pointer to store the chunk size (unit: samples)
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_afe_manager_get_chunk_size(esp_gmf_afe_manager_handle_t handle, size_t *size);

/**
 * @brief  Retrieve the number of input channels for the AFE Manager
 *
 * @param[in]   handle  AFE manager handle
 * @param[out]  ch_num  Pointer to store the number of channels
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_afe_manager_get_input_ch_num(esp_gmf_afe_manager_handle_t handle, uint8_t *ch_num);

#ifdef __cplusplus
}
#endif /* __cplusplus */
