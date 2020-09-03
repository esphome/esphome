#include "xiaomi_hhccjcy01.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_hhccjcy01 {

static const char *TAG = "xiaomi_hhccjcy01";

void XiaomiHHCCJCY01::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi HHCCJCY01");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Moisture", this->moisture_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

}  // namespace xiaomi_hhccjcy01
}  // namespace esphome

#endif
