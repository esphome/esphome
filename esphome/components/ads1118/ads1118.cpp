#include "ads1118.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ads1118 {

static const char *const TAG = "ads1118";
static const uint8_t ADS1118_DATA_RATE_860_SPS = 0b111;

void ADS1118::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ads1118");
  this->spi_setup();

  this->config_ = 0;
  // Setup multiplexer
  //        0bx000xxxxxxxxxxxx
  this->config_ |= ADS1118_MULTIPLEXER_P0_NG << 12;

  // Setup Gain
  //        0bxxxx000xxxxxxxxx
  this->config_ |= ADS1118_GAIN_6P144 << 9;

  // Set singleshot mode
  //        0bxxxxxxx1xxxxxxxx
  this->config_ |= 0b0000000100000000;

  // Set data rate - 860 samples per second (we're in singleshot mode)
  //        0bxxxxxxxx100xxxxx
  this->config_ |= ADS1118_DATA_RATE_860_SPS << 5;

  // Set temperature sensor mode - ADC
  //        0bxxxxxxxxxxx0xxxx
  this->config_ |= 0b0000000000000000;

  // Set DOUT pull up - enable
  //        0bxxxxxxxxxxxx0xxx
  this->config_ |= 0b0000000000001000;

  // NOP - must be 01
  //        0bxxxxxxxxxxxxx01x
  this->config_ |= 0b0000000000000010;

  // Not used - can be 0 or 1, lets be positive
  //        0bxxxxxxxxxxxxxxx1
  this->config_ |= 0b0000000000000001;
}

void ADS1118::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1118:");
  LOG_PIN("  CS Pin:", this->cs_);
}

float ADS1118::request_measurement(ADS1118Multiplexer multiplexer, ADS1118Gain gain, bool temperature_mode) {
  uint16_t temp_config = this->config_;
  // Multiplexer
  //        0bxBBBxxxxxxxxxxxx
  temp_config &= 0b1000111111111111;
  temp_config |= (multiplexer & 0b111) << 12;

  // Gain
  //        0bxxxxBBBxxxxxxxxx
  temp_config &= 0b1111000111111111;
  temp_config |= (gain & 0b111) << 9;

  if (temperature_mode) {
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

  this->enable();
  uint8_t adc_first_byte = this->read_byte();
  uint8_t adc_second_byte = this->read_byte();
  this->disable();
  uint16_t raw_conversion = encode_uint16(adc_first_byte, adc_second_byte);

  auto signed_conversion = static_cast<int16_t>(raw_conversion);

  if (temperature_mode) {
    return (signed_conversion >> 2) * 0.03125f;
  } else {
    float millivolts;
    float divider = 32768.0f;
    switch (gain) {
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

}  // namespace ads1118
}  // namespace esphome
