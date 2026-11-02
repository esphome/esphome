#include "xiaomi_lywsdcgq.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_lywsdcgq {

ESPHOME_LOG_TAG(TAG, "xiaomi_lywsdcgq");

void XiaomiLYWSDCGQ::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi LYWSDCGQ");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiLYWSDCGQ::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  publish_optional(this->temperature_sensor_, res.temperature);
  publish_optional(this->humidity_sensor_, res.humidity);
  publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_lywsdcgq
