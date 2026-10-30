#include "xiaomi_lywsd02.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_lywsd02 {

ESPHOME_LOG_TAG(TAG, "xiaomi_lywsd02");

void XiaomiLYWSD02::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi LYWSD02");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiLYWSD02::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  sensor::publish_optional(this->temperature_sensor_, res.temperature);
  sensor::publish_optional(this->humidity_sensor_, res.humidity);
  sensor::publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_lywsd02
