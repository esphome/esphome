#include "xiaomi_mhoc401.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::xiaomi_mhoc401 {

ESPHOME_LOG_TAG(TAG, "xiaomi_mhoc401");

void XiaomiMHOC401::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi MHOC401");
  this->dump_bindkey_(TAG);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiMHOC401::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  sensor::publish_optional(this->temperature_sensor_, res.temperature);
  // see https://github.com/custom-components/sensor.mitemp_bt/issues/7#issuecomment-595948254
  if (res.humidity.has_value() && this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(truncf(*res.humidity));
  sensor::publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_mhoc401
