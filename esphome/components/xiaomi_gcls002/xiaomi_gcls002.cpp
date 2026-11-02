#include "xiaomi_gcls002.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_gcls002 {

ESPHOME_LOG_TAG(TAG, "xiaomi_gcls002");

void XiaomiGCLS002::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi GCLS002");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Moisture", this->moisture_sensor_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
}

void XiaomiGCLS002::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  publish_optional(this->temperature_sensor_, res.temperature);
  publish_optional(this->moisture_sensor_, res.moisture);
  publish_optional(this->conductivity_sensor_, res.conductivity);
  publish_optional(this->illuminance_sensor_, res.illuminance);
}

}  // namespace esphome::xiaomi_gcls002
