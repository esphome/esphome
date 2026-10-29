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
  if (res.temperature.has_value() && this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(*res.temperature);
  // see https://github.com/custom-components/sensor.mitemp_bt/issues/7#issuecomment-595948254
  if (res.humidity.has_value() && this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(truncf(*res.humidity));
  if (res.battery_level.has_value() && this->battery_level_sensor_ != nullptr)
    this->battery_level_sensor_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_mhoc401
