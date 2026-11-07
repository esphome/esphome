#include "ads1118.h"
#include "esphome/components/ads1xxx/ads1xxx.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ads1118 {

ESPHOME_LOG_TAG(TAG, "ads1118");
// DOUT pull-up, NOP 01 (write the config), reserved bit set
static constexpr uint16_t CONFIG_BASE = ads1xxx::CONFIG_SINGLE_SHOT | ads1xxx::CONFIG_DATA_RATE | 0b1011;

void ADS1118::setup() { this->spi_setup(); }

void ADS1118::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1118:");
  LOG_PIN("  CS Pin:", this->cs_);
}

float ADS1118::request_measurement(uint16_t config_bits) {
  this->enable();
  this->write_byte16(CONFIG_BASE | ads1xxx::CONFIG_START | config_bits);
  this->disable();

  // about 1.2 ms with 860 samples per second
  delay(2);

  this->enable();
  uint8_t adc_first_byte = this->read_byte();
  uint8_t adc_second_byte = this->read_byte();
  this->disable();
  auto raw = static_cast<int16_t>(encode_uint16(adc_first_byte, adc_second_byte));

  if (config_bits & CONFIG_TEMPERATURE)
    return (raw >> 2) * 0.03125f;
  return ads1xxx::to_volts(raw, config_bits);
}

}  // namespace esphome::ads1118
