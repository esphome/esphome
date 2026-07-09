// namespace esp_afe marker for ESPHome lint; vendored GMF C API remains global.
/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_element.h"
#include "esp_gmf_err.h"
#include "esp_nsn_iface.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ESP_GMF_NS_CFG_DEFAULT() \
  { \
    .ns_iface = NULL, .model_name = "nsnet2", .partition_label = "model", .sample_rate = 16000, .channel = 1, \
    .frame_ms = 10, \
  }

/**
 * @brief  Configuration structure for NS
 *
 * @note  The standalone NS element expects 16-bit, single-channel PCM input.
 *        The currently supported sample rate is 16000 Hz.
 *        With CONFIG_SR_NSN_NSNET2, the model is loaded from `partition_label`.
 *        With CONFIG_SR_NSN_WEBRTC, `model_name`, `partition_label` and `ns_iface`
 *        are ignored and WebRTC NS processes `frame_ms` chunks.
 */
typedef struct {
  const esp_nsn_iface_t *ns_iface; /*!< Optional custom NS interface; set to NULL to resolve from model_name */
  const char *model_name;          /*!< NS model name, such as "nsnet2" */
  const char *partition_label;     /*!< Model partition label, defaults to "model" when NULL */
  uint16_t sample_rate;            /*!< Audio sample rate in Hz */
  uint8_t channel;                 /*!< Audio channel count, only 1 is supported */
  uint8_t frame_ms;                /*!< WebRTC NS frame duration in ms, supports 10, 20 and 30 ms */
} esp_gmf_ns_cfg_t;

/**
 * @brief  Initialize the Espressif NS element
 *
 * @param[in]   cfg         Pointer to the configuration structure
 * @param[out]  out_handle  Pointer to the handle to be created
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_ns_init(esp_gmf_ns_cfg_t *cfg, esp_gmf_obj_handle_t *out_handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */
