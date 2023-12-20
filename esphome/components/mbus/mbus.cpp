#include "mbus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus";

void MBus::setup() { ESP_LOGD(TAG, "setup"); }
void MBus::loop() {
  ESP_LOGD(TAG, "loop");
  delay(1000);
}

void MBus::dump_config() {
  ESP_LOGCONFIG(TAG, "MBus:");
  // LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  // ESP_LOGCONFIG(TAG, "  Send Wait Time: %d ms", this->send_wait_time_);
  // ESP_LOGCONFIG(TAG, "  CRC Disabled: %s", YESNO(this->disable_crc_));
}

float MBus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}
}  // namespace mbus
}  // namespace esphome
