#include "esphome/core/log.h"
#include "m5stack_4relay_switch.h"

namespace esphome {
namespace m5stack_4relay {

static const char *const TAG = "switch.M5Stack_4_Relay";

float M5Stack4RelaySwitch::get_setup_priority() const { return setup_priority::HARDWARE; }

void M5Stack4RelaySwitch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack_4_relay Switch '%s'...", this->name_.c_str());

  bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);

  // write state before setup
  if (initial_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

void M5Stack4RelaySwitch::dump_config() {
  LOG_SWITCH("", "M5Stack_4Relay Switch", this);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);

  if (!this->interlock_.empty()) {
    ESP_LOGCONFIG(TAG, "  Interlocks:");
    for (auto *lock : this->interlock_) {
      if (lock == this)
        continue;
      ESP_LOGCONFIG(TAG, "    %s", lock->get_name().c_str());
    }
  }
}

void M5Stack4RelaySwitch::write_state(bool state) {
  if (state != this->inverted_) {
    // Turning ON, check interlocking

    bool found = false;
    for (auto *lock : this->interlock_) {
      if (lock == this)
        continue;

      if (lock->state) {
        lock->turn_off();
        found = true;
      }
    }
    if (found && this->interlock_wait_time_ != 0) {
      this->set_timeout("interlock", this->interlock_wait_time_, [this, state] {
        // Don't write directly, call the function again
        // (some other switch may have changed state while we were waiting)
        this->write_state(state);
      });
      return;
    }
  } else if (this->interlock_wait_time_ != 0) {
    // If we are switched off during the interlock wait time, cancel any pending
    // re-activations
    this->cancel_timeout("interlock");
  }

  // This will be called every time the user requests a state change.
  this->parent_->relay_write(this->channel_ - 1, state);

  // Acknowledge new state by publishing it
  this->publish_state(state);
}

void M5Stack4RelaySwitch::set_interlock(const std::vector<Switch *> &interlock) { this->interlock_ = interlock; }

}  // namespace m5stack_4relay
}  // namespace esphome
