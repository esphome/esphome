#include "m5stack_4relay.h"
#include <bitset>
#include "esphome/core/log.h"
namespace esphome {
namespace m5stack_4relay {
uint8_t component_status;
static const char *const TAG = "m5stack_4relay.switch";

void M5STACK4RELAYSwitchComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5STACK4RELAY (0x%02X)...", this->address_);
  component_status = 0;
  get_status(&component_status);
  ESP_LOGD(TAG, "Setup Status 0x%02X", component_status);
}

M5STACK4RELAYChannel *M5STACK4RELAYSwitchComponent::create_channel(uint8_t channel) {
  return new M5STACK4RELAYChannel(this, channel);
}

void M5STACK4RELAYSwitchComponent::get_status(uint8_t *state) { this->read_byte(RELAY_CONTROL_REG); }

bool M5STACK4RELAYSwitchComponent::set_channel_value_(uint8_t channel, bool state) {
  get_status(&component_status);

  ESP_LOGD(TAG, "Current Status 0x%02X", component_status);
  ESP_LOGD(TAG, "Desired channel: %1u", channel);
  ESP_LOGD(TAG, "Desired state: %1u", state);

  channel = 3 - channel;
  if (state) {
    component_status |= (1u << channel);
  } else {
    component_status &= ~(1u << channel);
  }

  ESP_LOGD(TAG, "New status 0x%02X", component_status);

  return this->parent_->write_byte(this->address_, RELAY_CONTROL_REG, component_status);
}

void M5STACK4RELAYChannel::write_state(bool state) {
  if (!this->parent_->set_channel_value_(this->channel_, state)) {
    publish_state(false);
  } else {
    publish_state(state);
  }
}

}  // namespace m5stack_4relay
}  // namespace esphome
