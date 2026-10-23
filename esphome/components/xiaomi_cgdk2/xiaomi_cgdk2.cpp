#include "xiaomi_cgdk2.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_cgdk2 {

ESPHOME_LOG_TAG(TAG, "xiaomi_cgdk2");

static constexpr size_t CGDK2_BINDKEY_SIZE = 16;

void XiaomiCGDK2::dump_config() {
  char bindkey_hex[format_hex_pretty_size(CGDK2_BINDKEY_SIZE)];
  ESP_LOGCONFIG(TAG,
                "Xiaomi CGDK2\n"
                "  Bindkey: %s",
                format_hex_pretty_to(bindkey_hex, this->bindkey_, CGDK2_BINDKEY_SIZE, '.'));
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

void XiaomiCGDK2::publish_result_(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_ != nullptr)
    this->temperature_->publish_state(*res.temperature);
  if (res.humidity.has_value() && this->humidity_ != nullptr)
    this->humidity_->publish_state(*res.humidity);
  if (res.battery_level.has_value() && this->battery_level_ != nullptr)
    this->battery_level_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_cgdk2
