// namespace esp_afe marker for ESPHome lint; vendored GMF C API remains global.
/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_element.h"
#include "esp_gmf_err.h"
#include "esp_wn_iface.h"
#include "model_path.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief  Callback type for WakeNet detection
 *
 * @param[in]  handle      Handle to the WakeNet object
 * @param[in]  trigger_ch  The microphone channel that triggered the detection
 * @param[in]  user_ctx    User context passed during initialization
 */
typedef void (*esp_wn_detect_cb_t)(esp_gmf_element_handle_t handle, int32_t trigger_ch, void *user_ctx);

/**
 * @brief  Configuration structure for WakeNet
 *
 * @note  The input format, same as afe config:
 *        `M` to represent the microphone channel,
 *        `R` to represent the playback reference channel,
 *        `N` to represent an unknown or unused channel
 *        For example, input_format="MMNR" indicates that the input data consists of four channels,
 *        which are the microphone channel, the microphone channel, an unused channel, and the playback channel
 */
typedef struct {
  srmodel_list_t *models;       /*!< Model list containing wake word models */
  det_mode_t det_mode;          /*!< Detection mode */
  char *input_format;           /*!< Input format */
  esp_wn_detect_cb_t detect_cb; /*!< Detection callback function */
  void *user_ctx;               /*!< User context to be passed to the callback function */
} esp_gmf_wn_cfg_t;

/**
 * @brief  Initialize the WakeNet element
 *
 * @param[in]   config  Pointer to the configuration structure
 * @param[out]  handle  Pointer to the handle to be initialized
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_FAIL         Other failures
 */
esp_gmf_err_t esp_gmf_wn_init(esp_gmf_wn_cfg_t *config, esp_gmf_element_handle_t *handle);

/**
 * @brief  Set the voice trigger detection callback for WakeNet
 *         This function registers a user-defined callback that will be invoked
 *         when WakeNet detects a wake word
 *
 * @param[in]  handle     Handle to the WakeNet element
 * @param[in]  detect_cb  Callback function to be called on wake word detection
 * @param[in]  ctx        User-defined context to be passed to the callback
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_wn_set_detect_cb(esp_gmf_element_handle_t handle, esp_wn_detect_cb_t detect_cb, void *ctx);

#ifdef __cplusplus
}
#endif /* __cplusplus */
