// namespace esp_afe marker for ESPHome lint; vendored GMF C API remains global.
/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "esp_gmf_element.h"
#include "esp_vad.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ESP_GMF_VAD_CFG_DEFAULT() \
  { \
    .sample_rate = 16000, .frame_ms = 30, .vad_mode = VAD_MODE_3, .result_callback = NULL, .ctx = NULL, \
    .model_name = NULL, .partition_label = "model", .min_speech_ms = 32, .min_noise_ms = 64, \
  }

/**
 * @brief  Callback type for VAD result notification
 *
 * @param[in]  state  VAD result state
 * @param[in]  ctx    User context passed during initialization
 */
typedef void (*esp_gmf_vad_result_callback_t)(vad_state_t state, void *ctx);

/**
 * @brief  Configuration structure for VAD
 *
 * @note  The standalone VAD element expects 16-bit, single-channel PCM input.
 *        Supported sample rates are 8000, 16000 and 32000 Hz for the
 *        WebRTC backend. When `CONFIG_SR_VADN_VADNET1_MEDIUM` is selected,
 *        the element uses the VADNet backend and resolves the model from the
 *        model partition. VADNet uses the frame size required by the selected
 *        model, so `frame_ms` is only used by the WebRTC backend.
 * @note  With `esp_srmodel_init` + flash mmap, esp-sr still may print a check for
 *        paths like `/srmodel/<model>/vadn1_index` (hufzip legacy). That does
 *        not mean `esp_srmodel_init` failed, but if VAD never produces sensible
 *        results, the VADNet binary may not be using mmap data as expected; use
 *        `CONFIG_SR_VADN_WEBRTC` or align the esp-sr model pack / SPIFFS layout
 *        with the vendor’s VADNet requirements.
 */
typedef struct {
  uint16_t sample_rate;                          /*!< Audio sample rate in Hz */
  uint16_t frame_ms;                             /*!< Audio frame duration in ms */
  vad_mode_t vad_mode;                           /*!< VAD operating mode */
  esp_gmf_vad_result_callback_t result_callback; /*!< Result callback, called when VAD state changes */
  void *ctx;                                     /*!< User context to be passed to the callback */
  const char *model_name;      /*!< Optional VADNet model name; NULL to auto-select the first vadnet model */
  const char *partition_label; /*!< VADNet model partition label, defaults to "model" when NULL */
  uint16_t min_speech_ms;      /*!< VADNet minimum speech duration in ms, defaults to 32 when <= 0 */
  uint16_t min_noise_ms;       /*!< VADNet minimum noise duration in ms, defaults to 64 when <= 0 */
} esp_gmf_vad_cfg_t;

/**
 * @brief  Initialize the Espressif VAD element
 *
 * @param[in]   cfg         Pointer to the configuration structure
 * @param[out]  out_handle  Pointer to the handle to be created
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_vad_init(esp_gmf_vad_cfg_t *cfg, esp_gmf_obj_handle_t *out_handle);

/**
 * @brief  Set the VAD result callback
 *
 * @param[in]  handle           Handle to the VAD element
 * @param[in]  result_callback  Callback function to be called when VAD state changes, can be NULL to clear
 * @param[in]  ctx              User-defined context to be passed to the callback
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_vad_set_result_cb(esp_gmf_element_handle_t handle, esp_gmf_vad_result_callback_t result_callback,
                                        void *ctx);

#ifdef __cplusplus
}
#endif /* __cplusplus */
