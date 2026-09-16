#include "es7243.h"
#include "esphome/core/log.h"

#include "driver/gpio.h"

#include <cmath>

namespace esphome::es7243 {

static const char *const TAG = "es7243";

// Gain (dB) -> register 0x08 value, taken directly from ESP-ADF's
// es7243_adc_set_voice_volume() bucket table (comments there give the dB
// value for each register byte).
static uint8_t gain_to_reg(float gain_db) {
  struct Entry {
    float db;
    uint8_t reg;
  };
  static const Entry TABLE[] = {
      {1.0f, 0x11}, {3.5f, 0x13}, {18.0f, 0x21}, {20.5f, 0x23},
      {22.5f, 0x06}, {24.5f, 0x41}, {25.0f, 0x07}, {27.0f, 0x43},
  };
  const Entry *best = &TABLE[0];
  float best_diff = std::abs(gain_db - best->db);
  for (const auto &entry : TABLE) {
    float diff = std::abs(gain_db - entry.db);
    if (diff < best_diff) {
      best = &entry;
      best_diff = diff;
    }
  }
  return best->reg;
}

void ES7243::prime_mclk_() {
  // The chip's I2C interface doesn't activate until it has seen MCLK
  // activity. Bit-bang the pin as a plain GPIO output for a short pulse
  // train before any I2C traffic, using the raw ESP-IDF driver directly
  // (not ESPHome's GPIOPin) since this pin is also i2s_record's
  // i2s_mclk_pin and going through ESPHome's tracked pin schema would trip
  // its multi-use conflict check. The I2S peripheral takes over driving
  // this pin as the real MCLK signal once it starts, afterward.
  auto pin = static_cast<gpio_num_t>(this->mclk_gpio_num_);
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  for (int i = 0; i < 20; i++) {
    gpio_set_level(pin, 0);
    delay(1);
    gpio_set_level(pin, 1);
    delay(1);
  }
}

void ES7243::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ES7243...");
  this->prime_mclk_();

  bool ok = true;
  ok &= this->write_byte(0x00, 0x01);
  ok &= this->write_byte(0x06, 0x00);
  ok &= this->write_byte(0x05, 0x1B);
  ok &= this->write_byte(0x01, 0x0C);
  ok &= this->write_byte(0x08, gain_to_reg(this->mic_gain_));
  ok &= this->write_byte(0x05, 0x13);

  if (!ok) {
    ESP_LOGE(TAG, "Failed to initialize ES7243");
    this->mark_failed();
  }
}

void ES7243::dump_config() {
  ESP_LOGCONFIG(TAG, "ES7243 audio ADC:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Failed to initialize");
  }
}

bool ES7243::set_mic_gain(float mic_gain) {
  this->mic_gain_ = mic_gain;
  return this->write_byte(0x08, gain_to_reg(mic_gain));
}

}  // namespace esphome::es7243
