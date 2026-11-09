#include "xiaomi_mue4094rt.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_mue4094rt {

ESPHOME_LOG_TAG(TAG, "xiaomi_mue4094rt");

static constexpr uint32_t MOTION_TIMEOUT_ID = 0;

void XiaomiMUE4094RT::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi MUE4094RT");
  LOG_BINARY_SENSOR("  ", "Motion", this);
}

void XiaomiMUE4094RT::publish_result(const xiaomi_ble::XiaomiParseResult &res) {
  if (res.has_motion.has_value()) {
    this->publish_state(*res.has_motion);
    this->set_timeout(MOTION_TIMEOUT_ID, timeout_, [this]() { this->publish_state(false); });
  }
}

}  // namespace esphome::xiaomi_mue4094rt
