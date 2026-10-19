#include "es7243.h"

#ifdef USE_ESP32

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <driver/gpio.h>

#include <cmath>
#include <iterator>

namespace esphome::es7243 {

ESPHOME_LOG_TAG(TAG, "es7243");

// Register map from the ES7243 datasheet
static constexpr uint8_t REG_MODE_CONFIG = 0x00;
static constexpr uint8_t REG_SDP_FORMAT = 0x01;
static constexpr uint8_t REG_MUTE_CONTROL = 0x05;
static constexpr uint8_t REG_STATE_CONTROL = 0x06;
static constexpr uint8_t REG_GAIN = 0x08;  // ANACTRL1
static constexpr uint8_t MCLK_PULSES = 20;

struct GainEntry {
  float db;
  uint8_t reg;
};
// Register 0x08 values for each gain, from the ESP-ADF es7243 driver
static constexpr GainEntry GAIN_TABLE[] = {
    {1.0f, 0x11},  {3.5f, 0x13},  {18.0f, 0x21}, {20.5f, 0x23},
    {22.5f, 0x06}, {24.5f, 0x41}, {25.0f, 0x07}, {27.0f, 0x43},
};

// The table entry nearest the requested gain; values outside the table snap to its ends
static uint8_t nearest_gain_index(float gain_db) {
  uint8_t best = 0;
  for (uint8_t i = 1; i < std::size(GAIN_TABLE); i++) {
    if (std::fabs(gain_db - GAIN_TABLE[i].db) < std::fabs(gain_db - GAIN_TABLE[best].db))
      best = i;
  }
  return best;
}

void ES7243::setup() {
  // The chip ignores I2C until it has seen MCLK, so pulse the pin before the I2S peripheral takes it over
  const gpio_num_t mclk = this->i2s_->get_pin_config().mclk;
  gpio_reset_pin(mclk);
  gpio_set_direction(mclk, GPIO_MODE_OUTPUT);
  for (uint8_t i = 0; i < MCLK_PULSES; i++) {
    gpio_set_level(mclk, 0);
    delay(1);
    gpio_set_level(mclk, 1);
    delay(1);
  }

  // The ESP-ADF sequence: mute while the format and gain are set, then unmute
  const bool ok = this->write_byte(REG_MODE_CONFIG, 0x01) && this->write_byte(REG_STATE_CONTROL, 0x00) &&
                  this->write_byte(REG_MUTE_CONTROL, 0x1B) && this->write_byte(REG_SDP_FORMAT, 0x0C) &&
                  this->write_mic_gain_() && this->write_byte(REG_MUTE_CONTROL, 0x13);
  if (!ok) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->setup_complete_ = true;
}

void ES7243::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ES7243 audio ADC:\n"
                "  Mic gain: %.1f dB",
                this->mic_gain());
  LOG_I2C_DEVICE(this);
}

float ES7243::mic_gain() { return GAIN_TABLE[this->gain_index_].db; }

bool ES7243::set_mic_gain(float mic_gain) {
  this->gain_index_ = nearest_gain_index(mic_gain);
  if (!this->setup_complete_)
    return true;
  return this->status_comm_result(this->write_mic_gain_());
}

bool ES7243::write_mic_gain_() { return this->write_byte(REG_GAIN, GAIN_TABLE[this->gain_index_].reg); }

}  // namespace esphome::es7243

#endif  // USE_ESP32
