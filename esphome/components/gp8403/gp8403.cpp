#include "gp8403.h"

#include "esphome/core/log.h"

namespace esphome {
namespace gp8403 {

static const char *const TAG = "gp8403";

static const uint8_t RANGE_REGISTER = 0x01;

void GP8403::setup() { this->write_register(RANGE_REGISTER, (uint8_t *) (&this->voltage_), 1); }

void GP8403::dump_config() {
  ESP_LOGCONFIG(TAG, "GP8403:");
  ESP_LOGCONFIG(TAG, "  Voltage: %dV", this->voltage_ == GP8403_VOLTAGE_5V ? 5 : 10);
  LOG_I2C_DEVICE(this);
}

}  // namespace gp8403
}  // namespace esphome
