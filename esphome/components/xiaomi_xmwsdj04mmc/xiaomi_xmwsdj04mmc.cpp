#include "xiaomi_xmwsdj04mmc.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_xmwsdj04mmc {

ESPHOME_LOG_TAG(TAG, "xiaomi_xmwsdj04mmc");

static constexpr size_t XMWSDJ04MMC_BINDKEY_SIZE = 16;

void XiaomiXMWSDJ04MMC::dump_config() {
  char bindkey_hex[format_hex_pretty_size(XMWSDJ04MMC_BINDKEY_SIZE)];
  ESP_LOGCONFIG(TAG,
                "Xiaomi XMWSDJ04MMC\n"
                "  Bindkey: %s",
                format_hex_pretty_to(bindkey_hex, this->bindkey_, XMWSDJ04MMC_BINDKEY_SIZE, '.'));
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

void XiaomiXMWSDJ04MMC::publish_result_(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_ != nullptr)
    this->temperature_->publish_state(*res.temperature);
  // see https://github.com/custom-components/sensor.mitemp_bt/issues/7#issuecomment-595948254
  if (res.humidity.has_value() && this->humidity_ != nullptr)
    this->humidity_->publish_state(truncf(*res.humidity));
  if (res.battery_level.has_value() && this->battery_level_ != nullptr)
    this->battery_level_->publish_state(*res.battery_level);
}

}  // namespace esphome::xiaomi_xmwsdj04mmc
