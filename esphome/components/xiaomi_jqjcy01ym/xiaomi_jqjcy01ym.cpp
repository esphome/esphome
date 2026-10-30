#include "xiaomi_jqjcy01ym.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_jqjcy01ym {

ESPHOME_LOG_TAG(TAG, "xiaomi_jqjcy01ym");

void XiaomiJQJCY01YM::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi JQJCY01YM");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiJQJCY01YM::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  sensor::publish_optional(this->temperature_sensor_, res.temperature);
  sensor::publish_optional(this->humidity_sensor_, res.humidity);
  sensor::publish_optional(this->formaldehyde_sensor_, res.formaldehyde);
  sensor::publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_jqjcy01ym
