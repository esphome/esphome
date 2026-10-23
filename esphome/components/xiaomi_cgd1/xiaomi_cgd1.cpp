#include "xiaomi_cgd1.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_cgd1 {

ESPHOME_LOG_TAG(TAG, "xiaomi_cgd1");

static constexpr size_t CGD1_BINDKEY_SIZE = 16;

void XiaomiCGD1::dump_config() {
  char bindkey_hex[format_hex_pretty_size(CGD1_BINDKEY_SIZE)];
  ESP_LOGCONFIG(TAG,
                "Xiaomi CGD1\n"
                "  Bindkey: %s",
                format_hex_pretty_to(bindkey_hex, this->bindkey_, CGD1_BINDKEY_SIZE, '.'));
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

void XiaomiCGD1::publish_result_(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_ != nullptr)
    this->temperature_->publish_state(*res.temperature);
  if (res.humidity.has_value() && this->humidity_ != nullptr)
    this->humidity_->publish_state(*res.humidity);
  if (res.battery_level.has_value() && this->battery_level_ != nullptr)
    this->battery_level_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_cgd1
