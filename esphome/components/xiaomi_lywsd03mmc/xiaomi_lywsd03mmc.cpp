#include "xiaomi_lywsd03mmc.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::xiaomi_lywsd03mmc {

ESPHOME_LOG_TAG(TAG, "xiaomi_lywsd03mmc");

void XiaomiLYWSD03MMC::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi LYWSD03MMC");
  this->dump_bindkey_(TAG);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

void XiaomiLYWSD03MMC::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_ != nullptr)
    this->temperature_->publish_state(*res.temperature);
  // see https://github.com/custom-components/sensor.mitemp_bt/issues/7#issuecomment-595948254
  if (res.humidity.has_value() && this->humidity_ != nullptr)
    this->humidity_->publish_state(truncf(*res.humidity));
  if (res.battery_level.has_value() && this->battery_level_ != nullptr)
    this->battery_level_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_lywsd03mmc
