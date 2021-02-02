#include "sgp40.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sgp40 {

static const char *TAG = "sgp40";

void SGP40Component::setup() { ESP_LOGCONFIG(TAG, "Setting up sgp40..."); }

void SGP40Component::dump_config() {
  ESP_LOGCONFIG(TAG, "sgp40:");
  LOG_I2C_DEVICE(this);
}

void SGP40Component::update() {}

}  // namespace sgp40
}  // namespace esphome
