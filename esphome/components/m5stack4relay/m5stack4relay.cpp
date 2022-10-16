#include "esphome/core/log.h"
#include "m5stack4relay.h"
#include <bitset>

namespace esphome {
namespace m5stack4relay {

static const char *const TAG = "m5stack_4relay.switch";

void M5Relay4Control::dump_config() { ESP_LOGCONFIG(TAG, "M5Stack 4relay switch"); }

void M5Relay4Control::setup() {
  write_byte(UNIT_4RELAY_CONFIG_REG, this->sync_mode_);
  write_byte(UNIT_4RELAY_RELAY_REG, 0);

  ESP_LOGCONFIG(TAG, "Setting up M5STACK4RELAY (0x%02X)...", this->address_);
  uint8_t component_status = get_relay_states_();
  ESP_LOGD(TAG, "Setup Status 0x%02X", component_status);
  has_been_setup_ = true;
}

uint8_t M5Relay4Control::get_relay_states_() {
  uint8_t result;
  this->read_byte(UNIT_4RELAY_RELAY_REG, &result);
  return result;
}

bool M5Relay4Control::set_relay(RelayBit bit, bool state) {
  uint8_t org = get_relay_states_();

  if (state) {
    org |= (1u << (uint8_t) bit);
  } else {
    org &= ~(1u << (uint8_t) bit);
  }

  return write_byte(UNIT_4RELAY_RELAY_REG, org);
}

void M5Relay4Control::set_sync_mode(bool mode) {
  this->sync_mode_ = mode;
  if (has_been_setup_) {
    write_byte(UNIT_4RELAY_CONFIG_REG, this->sync_mode_);
  }
}

void M5Relay4Switch::write_state(bool state) {
  if (!this->parent_->set_relay(this->bit_, state)) {
    publish_state(false);
  } else {
    publish_state(state);
  }
}

}  // namespace m5stack4relay
}  // namespace esphome
