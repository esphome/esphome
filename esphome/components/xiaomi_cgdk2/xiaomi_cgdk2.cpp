#include "xiaomi_cgdk2.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_cgdk2 {

ESPHOME_LOG_TAG(TAG, "xiaomi_cgdk2");

void XiaomiCGDK2::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi CGDK2");
  this->dump_bindkey_(TAG);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiCGDK2::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  sensor::publish_optional(this->temperature_sensor_, res.temperature);
  sensor::publish_optional(this->humidity_sensor_, res.humidity);
  sensor::publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_cgdk2
