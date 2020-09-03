#include "xiaomi_lywsd02.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_lywsd02 {

static const char *TAG = "xiaomi_lywsd02";

void XiaomiLYWSD02::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi LYWSD02");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
}

}  // namespace xiaomi_lywsd02
}  // namespace esphome

#endif
