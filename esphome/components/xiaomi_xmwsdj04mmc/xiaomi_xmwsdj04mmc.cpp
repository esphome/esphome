#include "xiaomi_xmwsdj04mmc.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::xiaomi_xmwsdj04mmc {

ESPHOME_LOG_TAG(TAG, "xiaomi_xmwsdj04mmc");

void XiaomiXMWSDJ04MMC::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi XMWSDJ04MMC");
  this->dump_bindkey_(TAG);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

void XiaomiXMWSDJ04MMC::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  sensor::publish_optional(this->temperature_sensor_, res.temperature);
  // see https://github.com/custom-components/sensor.mitemp_bt/issues/7#issuecomment-595948254
  if (res.humidity.has_value() && this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(truncf(*res.humidity));
  sensor::publish_optional(this->battery_level_sensor_, res.battery_level);
}

}  // namespace esphome::xiaomi_xmwsdj04mmc
