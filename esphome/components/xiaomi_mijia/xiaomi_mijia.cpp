#include "xiaomi_mijia.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_mijia {

static const char *TAG = "xiaomi_mijia";

void XiaomiMijia::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Mijia");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

}  // namespace xiaomi_mijia
}  // namespace esphome

#endif
