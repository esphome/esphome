// Copyright (c) 2019-2022 Valentin Roland (github.com/vroland)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Original source: https://github.com/vroland/epdiy
// Modified by Xinyuan-LilyGO; combined work licensed under GPL-3.0-or-later:
// https://github.com/Xinyuan-LilyGO/LilyGo-EPD47

/**
 * Implements a 8bit parallel interface to transmit pixel
 * data to the display, based on the I2S peripheral.
 */

#pragma once

#include <driver/gpio.h>
#include <stdint.h>

#ifdef ESP_PLATFORM
#include <esp_attr.h>
#else
#ifndef IRAM_ATTR
#define IRAM_ATTR  // NOLINT
#endif
#endif

#ifdef __cplusplus
namespace esphome {
namespace lilygo_t5_47_plus {
extern "C" {
#endif

/**
 * I2S bus configuration parameters.
 */
typedef struct {  // NOLINT(modernize-use-using)
  /// GPIO numbers of the parallel bus pins.
  gpio_num_t data_0;
  gpio_num_t data_1;
  gpio_num_t data_2;
  gpio_num_t data_3;
  gpio_num_t data_4;
  gpio_num_t data_5;
  gpio_num_t data_6;
  gpio_num_t data_7;

  /// Data clock pin.
  gpio_num_t clock;

  /// "Start Pulse", enabling data input on the target device (active low)
  gpio_num_t start_pulse;

  // Width of a display row in pixels.
  uint32_t epd_row_width;
} i2s_bus_config;

/**
 * @brief Initialize the I2S data bus for communication with a 8bit parallel
 *        display interface.
 */
void i2s_bus_init(i2s_bus_config *cfg);

/**
 * @brief Get the currently writable line buffer.
 */
volatile uint8_t IRAM_ATTR *i2s_get_current_buffer();

/**
 * @brief Switches front and back line buffer.
 *
 * @note If the switched-to line buffer is currently in use, this function
 *       blocks until transmission is done.
 */
void IRAM_ATTR i2s_switch_buffer();

/**
 * @brief Start transmission of the current back buffer.
 */
void IRAM_ATTR i2s_start_line_output();

/**
 * @brief Returns true if there is an ongoing transmission.
 */
bool IRAM_ATTR i2s_is_busy();

/**
 * @brief Give up allocated resources.
 */
void i2s_deinit();

#ifdef __cplusplus
}  // extern "C"
}  // namespace lilygo_t5_47_plus
}  // namespace esphome
#endif
