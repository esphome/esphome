#include "gpio_switch.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#ifdef USE_GPIO_HOLD
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif

namespace esphome::gpio {

static const char *const TAG = "switch.gpio";
#ifdef USE_GPIO_SWITCH_INTERLOCK
static constexpr uint32_t INTERLOCK_TIMEOUT_ID = 0;
#endif

float GPIOSwitch::get_setup_priority() const { return setup_priority::HARDWARE; }
void GPIOSwitch::setup() {
  bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);

#ifdef USE_GPIO_HOLD
  if (!((this->pin_->get_flags() & gpio::FLAG_HOLD) && deep_sleep::woken_from_deepsleep()))
#endif
  {
    // write state before setup. If waking up from deepsleep and state is held
    // we need to setup first
    if (initial_state) {
      this->turn_on();
    } else {
      this->turn_off();
    }
  }
  this->pin_->setup();
  // write after setup again for other IOs and for held IOs
  if (initial_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}
void GPIOSwitch::dump_config() {
  LOG_SWITCH("", "GPIO Switch", this);
  LOG_PIN("  Pin: ", this->pin_);
#ifdef USE_GPIO_SWITCH_INTERLOCK
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
void GPIOSwitch::write_state(bool state) {
#ifdef USE_GPIO_SWITCH_INTERLOCK
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

  this->pin_->digital_write(state);
  this->publish_state(state);
}

#ifdef USE_GPIO_SWITCH_INTERLOCK
void GPIOSwitch::set_interlock(const std::initializer_list<Switch *> &interlock) { this->interlock_ = interlock; }
#endif

}  // namespace esphome::gpio
