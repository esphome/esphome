// Based on the TC74 datasheet https://ww1.microchip.com/downloads/en/DeviceDoc/21462D.pdf

#include "tc74.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tc74 {

static const char *const TAG = "tc74";

static const uint8_t TC74_REGISTER_TEMPERATURE = 0x00;
static const uint8_t TC74_REGISTER_CONFIGURATION = 0x01;
static const uint8_t TC74_DATA_READY_MASK = 0x40;

// It is possible the "Data Ready" bit will not be set if the TC74 has not been powered on for at least 250ms, so it not
// being set does not constitute a failure.
void TC74Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TC74...");
  uint8_t config_reg;
  if (this->read_register(TC74_REGISTER_CONFIGURATION, &config_reg, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  this->data_ready_ = config_reg & TC74_DATA_READY_MASK;
}

void TC74Component::update() { this->read_temperature_(); }

void TC74Component::dump_config() {
  LOG_SENSOR("", "TC74", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Connection with TC74 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void TC74Component::read_temperature_() {
  if (!this->data_ready_) {
    uint8_t config_reg;
    if (this->read_register(TC74_REGISTER_CONFIGURATION, &config_reg, 1) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }

    if (config_reg & TC74_DATA_READY_MASK) {
      this->data_ready_ = true;
    } else {
      ESP_LOGD(TAG, "TC74 not ready");
      return;
    }
  }

  uint8_t temperature_reg;
  if (this->read_register(TC74_REGISTER_TEMPERATURE, &temperature_reg, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  ESP_LOGD(TAG, "Got Temperature=%d °C", temperature_reg);
  this->publish_state(temperature_reg);
  this->status_clear_warning();
}

float TC74Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace tc74
}  // namespace esphome
