#include "xiaomi_hhccpot002.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_hhccpot002 {

ESPHOME_LOG_TAG(TAG, "xiaomi_hhccpot002");

void XiaomiHHCCPOT002::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi HHCCPOT002");
  LOG_SENSOR("  ", "Moisture", this->moisture_sensor_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_sensor_);
}

void XiaomiHHCCPOT002::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  sensor::publish_optional(this->moisture_sensor_, res.moisture);
  sensor::publish_optional(this->conductivity_sensor_, res.conductivity);
}

}  // namespace esphome::xiaomi_hhccpot002
