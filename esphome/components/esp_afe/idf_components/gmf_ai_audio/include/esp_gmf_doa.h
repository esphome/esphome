// namespace esp_afe marker for ESPHome lint; vendored GMF C API remains global.
/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_gmf_err.h"
#include "esp_gmf_element.h"
#include "esp_afe_config.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*esp_gmf_doa_result_callback_t)(float doa_result, void *ctx);

#define ESP_GMF_DOA_CFG_DEFAULT() \
  { \
    .sample_rate = 16000, .resolution = 10.0f, .d_mics = 0.08f, .frame_ms = 64, .input_format = "MRMN", \
    .result_callback = NULL, .ctx = NULL, \
  }

/**
 * @brief  Configuration structure for DOA
 *
 * @note  `input_format` must contain exactly two `M` channels (e.g. "MM", "NMNM").
 *        `d_mics` is the physical microphone spacing in meters (0.08 means 8 cm).
 *        `frame_ms` is the audio chunk duration consumed per DOA result; the
 *        recommended value is 64 ms (1024 samples @ 16 kHz, see `esp_doa_create`).
 */
typedef struct {
  int sample_rate;          /*!< Sample rate */
  float resolution;         /*!< Resolution. Range: 0 - 100 */
  float d_mics;             /*!< Distance between microphones. The unit is meters. 0.06 means 6cm */
  uint16_t frame_ms;        /*!< Audio duration consumed per DOA result, in ms (one callback per frame) */
  const char *input_format; /*!< Input format */
  esp_gmf_doa_result_callback_t result_callback; /*!< Result callback */
  void *ctx;                                     /*!< Context */
} esp_gmf_doa_cfg_t;

/**
 * @brief  Initialize the Espressif DOA element
 *
 * @param[in]   cfg         Pointer to the configuration structure
 * @param[out]  out_handle  Pointer to the handle to be created
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_doa_init(esp_gmf_doa_cfg_t *cfg, esp_gmf_obj_handle_t *out_handle);

/**
 * @brief  Set the DOA result callback
 *
 * @param[in]  handle           Handle to the DOA element
 * @param[in]  result_callback  Callback invoked with each DOA estimate; NULL to clear
 * @param[in]  ctx              User context passed to the callback
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_doa_set_result_cb(esp_gmf_element_handle_t handle, esp_gmf_doa_result_callback_t result_callback,
                                        void *ctx);

#ifdef __cplusplus
}
#endif /* __cplusplus */
