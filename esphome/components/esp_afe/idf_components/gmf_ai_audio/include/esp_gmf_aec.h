// namespace esp_afe marker for ESPHome lint; vendored GMF C API remains global.
/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "esp_gmf_element.h"
#include "esp_afe_config.h"

/* This element can run with esp32, esp32s3, esp32c5 and esp32p4 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief  Configuration structure for AEC
 *
 * @note  The input format, same as afe config:
 *        `M` to represent the microphone channel,
 *        `R` to represent the playback reference channel,
 *        `N` to represent an unknown or unused channel
 *        For example, input_format="MMNR" indicates that the input data consists of four channels,
 *        which are the microphone channel, the microphone channel, an unused channel, and the playback channel
 */
typedef struct {
  uint8_t filter_len; /*!< The length of filter. The larger the filter, the higher the CPU loading
                       *  Recommended filter_length = 4 for esp32s3 and esp32p4. Recommended filter_length = 2 for
                       * esp32c5 */
  afe_type_t type;    /*!< AFE type */
  afe_mode_t mode;    /*!< AFE mode */
  char *input_format; /*!< Input format */
} esp_gmf_aec_cfg_t;

/**
 * @brief  Initialize the Espressif AEC element
 *
 * @param[in]   cfg         Pointer to the configuration structure
 * @param[out]  out_handle  Pointer to the handle to be created
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_aec_init(esp_gmf_aec_cfg_t *cfg, esp_gmf_obj_handle_t *out_handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */
