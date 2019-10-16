#include "xiaomi_cleargrass.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_cleargrass {

static const char *TAG = "xiaomi_cleargrass";

void XiaomiCleargrass::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Cleargrass");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

}  // namespace xiaomi_cleargrass
}  // namespace esphome

#endif
