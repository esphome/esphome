// Copyright (c) 2019-2022 Valentin Roland (github.com/vroland)
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Original source: https://github.com/vroland/epdiy
// Modified by Xinyuan-LilyGO (GPL-3.0): https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
// Modified for ESPHome: rewritten from legacy RMT API to ESP-IDF 5.x RMT driver

/******************************************************************************/
/***        include files                                                   ***/
/******************************************************************************/

#include "rmt_pulse.h"

#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include <esp_check.h>
#include <string.h>

/******************************************************************************/
/***        macro definitions                                               ***/
/******************************************************************************/

/******************************************************************************/
/***        type definitions                                                ***/
/******************************************************************************/

/******************************************************************************/
/***        local function prototypes                                       ***/
/******************************************************************************/

/**
 * @brief Remote peripheral interrupt. Used to signal when transmission is done.
 */
// static void IRAM_ATTR rmt_interrupt_handler(void *arg);

/******************************************************************************/
/***        exported variables                                              ***/
/******************************************************************************/

/******************************************************************************/
/***        local variables                                                 ***/
/******************************************************************************/

/**
 * @brief RMT channel handle
 */
static rmt_channel_handle_t rmt_chan = NULL;

/**
 * @brief RMT transmit configuration
 */
static rmt_transmit_config_t rmt_tx_config = {
    .loop_count = 0,
    .flags.eot_level = 0,
};

/**
 * @brief RMT encoder handle
 */
static rmt_encoder_handle_t rmt_encoder = NULL;

/******************************************************************************/
/***        exported functions                                              ***/
/******************************************************************************/

void rmt_pulse_init(gpio_num_t pin) {
  rmt_tx_channel_config_t tx_chan_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .gpio_num = pin,
      .mem_block_symbols = 64,
      .resolution_hz = 10000000,  // 10MHz, 0.1us resolution
      .trans_queue_depth = 4,
      .flags.invert_out = false,
      .flags.with_dma = false,
  };

  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &rmt_chan));
  ESP_ERROR_CHECK(rmt_enable(rmt_chan));

  // Create copy encoder for simple symbol transmission
  rmt_copy_encoder_config_t copy_encoder_config = {};
  ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &rmt_encoder));
}

void IRAM_ATTR pulse_ckv_ticks(uint16_t high_time_ticks, uint16_t low_time_ticks, bool wait) {
  rmt_symbol_word_t rmt_symbol;

  if (high_time_ticks > 0) {
    rmt_symbol.level0 = 1;
    rmt_symbol.duration0 = high_time_ticks;
    rmt_symbol.level1 = 0;
    rmt_symbol.duration1 = low_time_ticks;
  } else {
    rmt_symbol.level0 = 1;
    rmt_symbol.duration0 = low_time_ticks;
    rmt_symbol.level1 = 0;
    rmt_symbol.duration1 = 0;
  }

  if (wait) {
    ESP_ERROR_CHECK(rmt_transmit(rmt_chan, rmt_encoder, &rmt_symbol, sizeof(rmt_symbol_word_t), &rmt_tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(rmt_chan, -1));
  } else {
    rmt_transmit(rmt_chan, rmt_encoder, &rmt_symbol, sizeof(rmt_symbol_word_t), &rmt_tx_config);
  }
}

void IRAM_ATTR pulse_ckv_us(uint16_t high_time_us, uint16_t low_time_us, bool wait) {
  pulse_ckv_ticks(10 * high_time_us, 10 * low_time_us, wait);
}

// bool IRAM_ATTR rmt_busy()
// {
//     return !rmt_tx_done;
// }

/******************************************************************************/
/***        local functions                                                 ***/
/******************************************************************************/

// static void IRAM_ATTR rmt_interrupt_handler(void *arg)
// {
//     rmt_tx_done = true;
//     RMT.int_clr.val = RMT.int_st.val;
// }

/******************************************************************************/
/***        END OF FILE                                                     ***/
/******************************************************************************/
