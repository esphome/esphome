// namespace esp_afe marker for ESPHome lint; vendored GMF C API remains global.
/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief  Sort input data according to the input format and convert data layout
 *
 * @note  This function performs two operations:
 *          1. Channel sorting: Keeps microphone (M) channels in original order, removes unused (N) channels,
 *             and places reference (R) channels at the end
 *          2. Layout conversion: Converts data from interleaved format (M0[0]R[0]M1[0]N[0]M0[1]R[1]M1[1]N[1]...) to
 * block format (M0[0]M0[1]...M1[0]M1[1]...R[0]R[1]...)
 *
 * @param[in]   input_data    Pointer to the interleaved input data buffer
 * @param[in]   input_format  Input format string (e.g., "MMNR" for 2 mic channels, 1 unused, 1 reference)
 * @param[in]   num_samples   Number of samples per channel
 * @param[in]   num_channels  Total number of channels in the input data
 * @param[out]  sorted_data   Pointer to the output buffer for block-formatted sorted data
 *                            Data will be arranged as: [mic_samples][ref_samples]
 */
static inline void esp_gmf_sort_with_format(int16_t *input_data, const char *input_format, int num_samples,
                                            int num_channels, int16_t *sorted_data) {
  int mic_index = 0;
  int ref_index = 0;

  for (int ch = 0; ch < num_channels; ++ch) {
    if (input_format[ch] == 'M') {
      for (int sample = 0; sample < num_samples; ++sample) {
        sorted_data[mic_index++] = input_data[sample * num_channels + ch];
      }
    }
  }

  ref_index = mic_index;
  for (int ch = 0; ch < num_channels; ++ch) {
    if (input_format[ch] == 'R') {
      for (int sample = 0; sample < num_samples; ++sample) {
        sorted_data[ref_index++] = input_data[sample * num_channels + ch];
      }
    }
  }
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
