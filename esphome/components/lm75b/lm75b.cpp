#include "lm75b.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace lm75b {

static const char *const TAG = "lm75b";

void LM75BComponent::setup() { ESP_LOGCONFIG(TAG, "Setting up LM75B..."); }

float LM75BComponent::get_setup_priority() const { return setup_priority::DATA; }

void LM75BComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "lm75b:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setting up LM75B failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this);
}

void LM75BComponent::update() {
  this->set_timeout("read_temp", 50, [this]() {
    // Create a temporary buffer
    uint8_t buff[2];
    if (this->read_register(LM75B_REG_TEMPERATURE, buff, 2) != i2c::ERROR_OK) {
      this->status_set_warning("Error reading register");
      return;
    }
    // Swap bytes
    int16_t raw_temperature = (buff[0] << 8) | buff[1];
    // Remove 5 unused least significant bits
    raw_temperature >>= 5;
    // Get two's complement for 11 bit temperature register
    raw_temperature = this->twos_complement_(raw_temperature, 11);
    // Return the temperature in °C
    this->publish_state(raw_temperature * 0.125);
    if (this->status_has_warning()) {
      this->status_clear_warning();
    }
  });
}

int16_t LM75BComponent::twos_complement_(int16_t val, uint8_t bits) {
  if (val & ((uint16_t) 1 << (bits - 1))) {
    val -= (uint16_t) 1 << bits;
  }
  return val;
}

}  // namespace lm75b
}  // namespace esphome
