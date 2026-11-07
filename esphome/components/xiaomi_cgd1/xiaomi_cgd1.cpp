#include "xiaomi_cgd1.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_cgd1 {

ESPHOME_LOG_TAG(TAG, "xiaomi_cgd1");

void XiaomiCGD1::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi CGD1");
  this->dump_bindkey_(TAG);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiCGD1::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  publish_optional(this->temperature_sensor_, res.temperature);
  publish_optional(this->humidity_sensor_, res.humidity);
  publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_cgd1
