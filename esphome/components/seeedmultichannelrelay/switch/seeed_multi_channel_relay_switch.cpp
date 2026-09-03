#include "seeed_multi_channel_relay_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace seeed_multi_channel_relay {

static const char *const TAG = "switch.seeed_multi_channel_relay";

#ifdef USE_SWITCH_INTERLOCK
static constexpr uint32_t INTERLOCK_TIMEOUT_ID = 0;
#endif

void seeed_multi_channel_relay_Switch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up seeed_multi_channel_relay Switch '%s'...", this->name_.c_str());

  bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);

  // write state before setup
  if (initial_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

void seeed_multi_channel_relay_Switch::dump_config() {
  LOG_SWITCH("", "seeed_multi_channel_relay Switch", this);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
#ifdef USE_SWITCH_INTERLOCK
  if (!this->interlock_.empty()) {
    ESP_LOGCONFIG(TAG, "  Interlocks:");
    for (auto *lock : this->interlock_) {
      if (lock == this)
        continue;
      ESP_LOGCONFIG(TAG, "    %s", lock->get_name().c_str());
    }
  }
#endif
}

void seeed_multi_channel_relay_Switch::write_state(bool state) {
#ifdef USE_SWITCH_INTERLOCK
  if (state != this->inverted_) {
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
        this->set_timeout(INTERLOCK_TIMEOUT_ID, this->interlock_wait_time_, [this, state] {
          // Don't write directly, call the function again
          // (some other switch may have changed state while we were waiting)
          this->write_state(state);
        });
        return;
    }
  } else if (this->interlock_wait_time_ != 0) {
    // If we are switched off during the interlock wait time, cancel any pending
    // re-activations
    this->cancel_timeout(INTERLOCK_TIMEOUT_ID);
  }
#endif
  // This will be called every time the user requests a state change.
  this->parent_->relay_write(this->channel_, state);

  // Acknowledge new state by publishing it
  this->publish_state(state);
}
#ifdef USE_SWITCH_INTERLOCK
  void seeed_multi_channel_relay_Switch::set_interlock(const std::vector<Switch *> &interlock) { this->interlock_ = interlock;
}
#endif

}  // namespace seeed_multi_channel_relay
}  // namespace esphome
