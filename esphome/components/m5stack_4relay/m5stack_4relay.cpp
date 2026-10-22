#include "m5stack_4relay.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5stack_4relay {

ESPHOME_LOG_TAG(TAG, "m5stack_4relay");

static constexpr uint8_t REG_MODE = 0x10;
static constexpr uint8_t REG_RELAY = 0x11;
static constexpr uint8_t MODE_SYNC = 1;

void M5Stack4Relay::setup() {
  if (!this->write_byte(REG_MODE, MODE_SYNC) || !this->read_byte(REG_RELAY, &this->relays_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
#ifdef USE_SWITCH
  for (size_t channel = 0; channel < this->switches_.size(); channel++) {
    switch_::Switch *sw = this->switches_[channel];
    if (sw == nullptr)
      continue;
    // The chip keeps the relays across an ESP reboot, so a switch with restore mode DISABLED shows what it
    // holds; the other modes drive the relay through control(), so inverted: applies
    sw->publish_state((this->relays_ >> channel) & 1);
    sw->restore();
  }
#endif
}

void M5Stack4Relay::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack 4 Relay:");
  LOG_I2C_DEVICE(this);
}

bool M5Stack4Relay::set_relay(uint8_t channel, bool on) {
  if (this->is_failed())
    return false;
  uint8_t value = this->relays_;
  assign_bits(value, 1 << channel, on);
  if (!this->status_comm_result(this->write_byte(REG_RELAY, value)))
    return false;
  this->relays_ = value;
#ifdef USE_SWITCH
  if (this->switches_[channel] != nullptr)
    this->switches_[channel]->publish_state(on);
#endif
  return true;
}

}  // namespace esphome::m5stack_4relay
