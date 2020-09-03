#include "xiaomi_cgg1.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_cgg1 {

static const char *TAG = "xiaomi_cgg1";

void XiaomiCGG1::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi CGG1");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

}  // namespace xiaomi_cgg1
}  // namespace esphome

#endif
