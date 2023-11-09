#include "ads1118.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads1118 {

static const char *const TAG = "ads1118";
static const uint8_t ADS1118_DATA_RATE_860_SPS = 0b111;


void ADS1118::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ads1118");
  this->spi_setup();

  this->config = 0;
  // Setup multiplexer
  //        0bx000xxxxxxxxxxxx
  this->config |= ADS1118_MULTIPLEXER_P0_NG << 12;

  // Setup Gain
  //        0bxxxx000xxxxxxxxx
  this->config |= ADS1118_GAIN_6P144 << 9;

  // Set singleshot mode
  //        0bxxxxxxx1xxxxxxxx
  this->config |= 0b0000000100000000;

  // Set data rate - 860 samples per second (we're in singleshot mode)
  //        0bxxxxxxxx100xxxxx
  this->config |= ADS1118_DATA_RATE_860_SPS << 5;

  // Set temperature sensor mode - ADC
  //        0bxxxxxxxxxxx0xxxx
  this->config |= 0b0000000000000000;

  // Set DOUT pull up - enable
  //        0bxxxxxxxxxxxx0xxx
  this->config |= 0b0000000000001000;

  // NOP - must be 01
  //        0bxxxxxxxxxxxxx01x
  this->config |= 0b0000000000000010;

  // Not used - can be 0 or 1, lets be positive
  //        0bxxxxxxxxxxxxxxx1
  this->config |= 0b0000000000000001;
}

void ADS1118::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1118:");
  LOG_PIN("  CS Pin:", this->cs_);
  
  for (auto *sensor : this->sensors_) {
    LOG_SENSOR("  ", "Sensor", sensor);
    ESP_LOGCONFIG(TAG, "    Multiplexer: %u", sensor->get_multiplexer());
    ESP_LOGCONFIG(TAG, "    Gain: %u", sensor->get_gain());
  }
}

float ADS1118::request_measurement(ADS1118Sensor *sensor) {
  uint16_t temp_config = this->config;
  // Multiplexer
  //        0bxBBBxxxxxxxxxxxx
  temp_config &= 0b1000111111111111;
  temp_config |= (sensor->get_multiplexer() & 0b111) << 12;

  // Gain
  //        0bxxxxBBBxxxxxxxxx
  temp_config &= 0b1111000111111111;
  temp_config |= (sensor->get_gain() & 0b111) << 9;

  if (sensor->get_temperature_mode()) {
    // Set temperature sensor mode
    //        0bxxxxxxxxxxx1xxxx
    temp_config |= 0b0000000000010000;
  } else {
    // Set ADC mode
    //        0bxxxxxxxxxxx0xxxx
    temp_config &= 0b1111111111101111;
  }

  // Start conversion
  temp_config |= 0b1000000000000000;

  this->enable();
  this->write_byte16(temp_config);
  this->disable();

  // about 1.2 ms with 860 samples per second
  delay(2);

  uint16_t raw_conversion = 0;
  this->enable();
  uint8_t adc_first_byte = this->read_byte();
  uint8_t adc_second_byte = this->read_byte();
  this->disable();
  raw_conversion |= adc_first_byte << 8;
  raw_conversion |= adc_second_byte;

  auto signed_conversion = static_cast<int16_t>(raw_conversion);

  if (sensor->get_temperature_mode()) {
    return (signed_conversion >> 2) * 0.03125f;
  } else {
    float millivolts;
    float divider = 32768.0f;
    switch (sensor->get_gain()) {
      case ADS1118_GAIN_6P144:
        millivolts = (signed_conversion * 6144) / divider;
        break;
      case ADS1118_GAIN_4P096:
        millivolts = (signed_conversion * 4096) / divider;
        break;
      case ADS1118_GAIN_2P048:
        millivolts = (signed_conversion * 2048) / divider;
        break;
      case ADS1118_GAIN_1P024:
        millivolts = (signed_conversion * 1024) / divider;
        break;
      case ADS1118_GAIN_0P512:
        millivolts = (signed_conversion * 512) / divider;
        break;
      case ADS1118_GAIN_0P256:
        millivolts = (signed_conversion * 256) / divider;
        break;
      default:
        millivolts = NAN;
    }

    return millivolts / 1e3f;
  }

  
}

float ADS1118Sensor::sample() { return this->parent_->request_measurement(this); }
void ADS1118Sensor::update() {
  float v = this->parent_->request_measurement(this);
  if (!std::isnan(v)) {
    ESP_LOGD(TAG, "'%s': Got Voltage=%fV", this->get_name().c_str(), v);
    this->publish_state(v);
  }
}

}  // namespace ADS1118
}  // namespace esphome
