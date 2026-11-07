#include "ads1115.h"
#include "esphome/components/ads1xxx/ads1xxx.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::ads1115 {

ESPHOME_LOG_TAG(TAG, "ads1115");
static constexpr uint8_t REGISTER_CONVERSION = 0x00;
static constexpr uint8_t REGISTER_CONFIG = 0x01;
static constexpr uint16_t CONFIG_COMPARATOR_DISABLE = 0b11;

void ADS1115Component::setup() {
  const uint16_t config = ads1xxx::CONFIG_DATA_RATE | CONFIG_COMPARATOR_DISABLE |
                          (this->continuous_mode_ ? 0 : ads1xxx::CONFIG_SINGLE_SHOT);
  if (!this->setup_check(this->write_byte_16(REGISTER_CONFIG, config)))
    return;
  this->prev_config_ = config;
}
void ADS1115Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS1115:");
  LOG_I2C_DEVICE(this);
}
float ADS1115Component::request_measurement(uint16_t config_bits, uint8_t conversion_delay_ms) {
  const uint16_t config = CONFIG_COMPARATOR_DISABLE | config_bits |
                          (this->continuous_mode_ ? 0 : ads1xxx::CONFIG_SINGLE_SHOT | ads1xxx::CONFIG_START);

  if (!this->continuous_mode_ || this->prev_config_ != config) {
    if (!this->write_byte_16(REGISTER_CONFIG, config)) {
      this->status_comm_result(false);
      return NAN;
    }
    this->prev_config_ = config;
    delay(conversion_delay_ms);

    // In continuous mode the delay alone covers a conversion with the new settings
    if (!this->continuous_mode_) {
      uint32_t start = millis();
      uint16_t status;
      while (this->read_byte_16(REGISTER_CONFIG, &status) && (status & ads1xxx::CONFIG_START) == 0) {
        if (millis() - start > 100) {
          this->status_comm_result(false, LOG_STR("Conversion timed out"));
          return NAN;
        }
        yield();
      }
    }
  }

  uint16_t raw;
  if (!this->status_comm_result(this->read_byte_16(REGISTER_CONVERSION, &raw)))
    return NAN;
  return ads1xxx::to_volts(static_cast<int16_t>(raw), config_bits);
}

}  // namespace esphome::ads1115
