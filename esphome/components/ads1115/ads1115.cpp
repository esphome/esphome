#include "ads1115.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ads1115 {

static const char *const TAG = "ads1115";
static const uint8_t ADS1115_REGISTER_CONVERSION = 0x00;
static const uint8_t ADS1115_REGISTER_CONFIG = 0x01;

static const uint8_t ADS1115_DATA_RATE_860_SPS = 0b111;  // 3300_SPS for ADS1015

void ADS1115Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADS1115...");
  uint16_t value;
  if (!this->read_byte_16(ADS1115_REGISTER_CONVERSION, &value)) {
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "Configuring ADS1115...");

  uint16_t config = 0;
  // Clear single-shot bit
  //        0b0xxxxxxxxxxxxxxx
  config |= 0b0000000000000000;
  // Setup multiplexer
  //        0bx000xxxxxxxxxxxx
  config |= ADS1115_MULTIPLEXER_P0_N1 << 12;

  // Setup Gain
  //        0bxxxx000xxxxxxxxx
  config |= ADS1115_GAIN_6P144 << 9;

  if (this->continuous_mode_) {
    // Set continuous mode
    //        0bxxxxxxx0xxxxxxxx
    config |= 0b0000000000000000;
  } else {
    // Set singleshot mode
    //        0bxxxxxxx1xxxxxxxx
    config |= 0b0000000100000000;
  }

  // Set data rate - 860 samples per second (we're in singleshot mode)
  //        0bxxxxxxxx100xxxxx
  config |= ADS1115_DATA_RATE_860_SPS << 5;

  // Set comparator mode - hysteresis
  //        0bxxxxxxxxxxx0xxxx
  config |= 0b0000000000000000;

  // Set comparator polarity - active low
  //        0bxxxxxxxxxxxx0xxx
  config |= 0b0000000000000000;

  // Set comparator latch enabled - false
  //        0bxxxxxxxxxxxxx0xx
  config |= 0b0000000000000000;

  // Set comparator que mode - disabled
  //        0bxxxxxxxxxxxxxx11
  config |= 0b0000000000000011;

  if (!this->write_byte_16(ADS1115_REGISTER_CONFIG, config)) {
    this->mark_failed();
    return;
  }
  this->prev_config_ = config;
}
void ADS1115Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Setting up ADS1115...");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ADS1115 failed!");
  }

  for (auto *sensor : this->sensors_) {
    LOG_SENSOR("  ", "Sensor", sensor);
    ESP_LOGCONFIG(TAG, "    Multiplexer: %u", sensor->get_multiplexer());
    ESP_LOGCONFIG(TAG, "    Gain: %u", sensor->get_gain());
    ESP_LOGCONFIG(TAG, "    Resolution: %u", sensor->get_resolution());
  }
}
float ADS1115Component::request_measurement(ADS1115Sensor *sensor) {
  uint16_t config = this->prev_config_;
  // Multiplexer
  //        0bxBBBxxxxxxxxxxxx
  config &= 0b1000111111111111;
  config |= (sensor->get_multiplexer() & 0b111) << 12;

  // Gain
  //        0bxxxxBBBxxxxxxxxx
  config &= 0b1111000111111111;
  config |= (sensor->get_gain() & 0b111) << 9;

  if (!this->continuous_mode_) {
    // Start conversion
    config |= 0b1000000000000000;
  }

  if (!this->continuous_mode_ || this->prev_config_ != config) {
    if (!this->write_byte_16(ADS1115_REGISTER_CONFIG, config)) {
      this->status_set_warning();
      return NAN;
    }
    this->prev_config_ = config;

    // about 1.2 ms with 860 samples per second
    delay(2);

    // in continuous mode, conversion will always be running, rely on the delay
    // to ensure conversion is taking place with the correct settings
    // can we use the rdy pin to trigger when a conversion is done?
    if (!this->continuous_mode_) {
      uint32_t start = millis();
      while (this->read_byte_16(ADS1115_REGISTER_CONFIG, &config) && (config >> 15) == 0) {
        if (millis() - start > 100) {
          ESP_LOGW(TAG, "Reading ADS1115 timed out");
          this->status_set_warning();
          return NAN;
        }
        yield();
      }
    }
  }

  uint16_t raw_conversion;
  if (!this->read_byte_16(ADS1115_REGISTER_CONVERSION, &raw_conversion)) {
    this->status_set_warning();
    return NAN;
  }

  if (sensor->get_resolution() == ADS1015_12_BITS) {
    bool negative = (raw_conversion >> 15) == 1;

    // shift raw_conversion as it's only 12-bits, left justified
    raw_conversion = raw_conversion >> (16 - ADS1015_12_BITS);

    // check if number was negative in order to keep the sign
    if (negative) {
      // the number was negative
      // 1) set the negative bit back
      raw_conversion |= 0x8000;
      // 2) reset the former (shifted) negative bit
      raw_conversion &= 0xF7FF;
    }
  }

  auto signed_conversion = static_cast<int16_t>(raw_conversion);

  float millivolts;
  float divider = (sensor->get_resolution() == ADS1115_16_BITS) ? 32768.0f : 2048.0f;
  switch (sensor->get_gain()) {
    case ADS1115_GAIN_6P144:
      millivolts = (signed_conversion * 6144) / divider;
      break;
    case ADS1115_GAIN_4P096:
      millivolts = (signed_conversion * 4096) / divider;
      break;
    case ADS1115_GAIN_2P048:
      millivolts = (signed_conversion * 2048) / divider;
      break;
    case ADS1115_GAIN_1P024:
      millivolts = (signed_conversion * 1024) / divider;
      break;
    case ADS1115_GAIN_0P512:
      millivolts = (signed_conversion * 512) / divider;
      break;
    case ADS1115_GAIN_0P256:
      millivolts = (signed_conversion * 256) / divider;
      break;
    default:
      millivolts = NAN;
  }

  this->status_clear_warning();
  return millivolts / 1e3f;
}

float ADS1115Sensor::sample() { return this->parent_->request_measurement(this); }
void ADS1115Sensor::update() {
  float v = this->parent_->request_measurement(this);
  if (!std::isnan(v)) {
    ESP_LOGD(TAG, "'%s': Got Voltage=%fV", this->get_name().c_str(), v);
    this->publish_state(v);
  }
}

}  // namespace ads1115
}  // namespace esphome
