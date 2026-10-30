#include "xiaomi_mhoc303.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_mhoc303 {

ESPHOME_LOG_TAG(TAG, "xiaomi_mhoc303");

void XiaomiMHOC303::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi MHOC303");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiMHOC303::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  sensor::publish_optional(this->temperature_sensor_, res.temperature);
  sensor::publish_optional(this->humidity_sensor_, res.humidity);
  sensor::publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_mhoc303
