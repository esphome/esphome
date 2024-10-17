#include "gp8211.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gp8211 {

static const char *const TAG = "gp8211";

static const uint8_t RANGE_REGISTER = 0x01;  // Register für die Spannungsrange

void GP8211::setup() {
  // Wähle den Spannungsmodus basierend auf der YAML-Konfiguration
  uint8_t voltage_setting = (this->voltage_ == GP8211_VOLTAGE_10V) ? 0x77 : 0x55;  // 0x77 für 0-10V, 0x55 für 0-5V
  this->write_register(RANGE_REGISTER, &voltage_setting, 1);
}

void GP8211::dump_config() {
  ESP_LOGCONFIG(TAG, "GP8211:");
  ESP_LOGCONFIG(TAG, "  Voltage: %s Mode activated", this->voltage_ == GP8211_VOLTAGE_10V ? "10V" : "5V");
  LOG_I2C_DEVICE(this);
}

}  // namespace gp8211
}  // namespace esphome
