#include "xiaomi_gcls002.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_gcls002 {

ESPHOME_LOG_TAG(TAG, "xiaomi_gcls002");

void XiaomiGCLS002::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi GCLS002");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Moisture", this->moisture_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
}

void XiaomiGCLS002::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.temperature.has_value() && this->temperature_ != nullptr)
    this->temperature_->publish_state(*res.temperature);
  if (res.moisture.has_value() && this->moisture_ != nullptr)
    this->moisture_->publish_state(*res.moisture);
  if (res.conductivity.has_value() && this->conductivity_ != nullptr)
    this->conductivity_->publish_state(*res.conductivity);
  if (res.illuminance.has_value() && this->illuminance_ != nullptr)
    this->illuminance_->publish_state(*res.illuminance);
}

}  // namespace esphome::xiaomi_gcls002
