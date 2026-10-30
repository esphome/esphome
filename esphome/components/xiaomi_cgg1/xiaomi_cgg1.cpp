#include "xiaomi_cgg1.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <iterator>

namespace esphome::xiaomi_cgg1 {

ESPHOME_LOG_TAG(TAG, "xiaomi_cgg1");

void XiaomiCGG1::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi CGG1");
  // The bindkey is optional on the CGG1; without one it stays all zero
  if (std::any_of(std::begin(this->bindkey_), std::end(this->bindkey_), [](uint8_t b) { return b != 0; }))
    this->dump_bindkey_(TAG);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiCGG1::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  sensor::publish_optional(this->temperature_sensor_, res.temperature);
  sensor::publish_optional(this->humidity_sensor_, res.humidity);
  sensor::publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_cgg1
