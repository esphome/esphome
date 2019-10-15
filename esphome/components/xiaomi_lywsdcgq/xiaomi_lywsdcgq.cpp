#include "xiaomi_lywsdcgq.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_lywsdcgq {

static const char *TAG = "xiaomi_lywsdcgq";

void XiaomiLYWSDCGQ::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi LYWSDCGQ");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

}  // namespace xiaomi_lywsdcgq
}  // namespace esphome

#endif
