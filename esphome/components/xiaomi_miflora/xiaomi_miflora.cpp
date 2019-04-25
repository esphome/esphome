#include "xiaomi_miflora.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_miflora {

static const char *TAG = "xiaomi_miflora";

void XiaomiMiflora::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Mijia");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Moisture", this->moisture_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

}  // namespace xiaomi_miflora
}  // namespace esphome

#endif
