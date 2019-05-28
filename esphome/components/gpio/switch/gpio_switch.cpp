#include "gpio_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gpio {

static const char *TAG = "switch.gpio";

float GPIOSwitch::get_setup_priority() const { return setup_priority::HARDWARE; }
void GPIOSwitch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GPIO Switch '%s'...", this->name_.c_str());

  bool initial_state = false;
  switch (this->restore_mode_) {
    case GPIO_SWITCH_RESTORE_DEFAULT_OFF:
      initial_state = this->get_initial_state().value_or(false);
      break;
    case GPIO_SWITCH_RESTORE_DEFAULT_ON:
      initial_state = this->get_initial_state().value_or(true);
      break;
    case GPIO_SWITCH_ALWAYS_OFF:
      initial_state = false;
      break;
    case GPIO_SWITCH_ALWAYS_ON:
      initial_state = true;
      break;
  }

  // write state before setup
  if (initial_state)
    this->turn_on();
  else
    this->turn_off();
  this->pin_->setup();
  // write after setup again for other IOs
  if (initial_state)
    this->turn_on();
  else
    this->turn_off();
}
void GPIOSwitch::dump_config() {
  LOG_SWITCH("", "GPIO Switch", this);
  LOG_PIN("  Pin: ", this->pin_);
  const char *restore_mode = "";
  switch (this->restore_mode_) {
    case GPIO_SWITCH_RESTORE_DEFAULT_OFF:
      restore_mode = "Restore (Defaults to OFF)";
      break;
    case GPIO_SWITCH_RESTORE_DEFAULT_ON:
      restore_mode = "Restore (Defaults to ON)";
      break;
    case GPIO_SWITCH_ALWAYS_OFF:
      restore_mode = "Always OFF";
      break;
    case GPIO_SWITCH_ALWAYS_ON:
      restore_mode = "Always ON";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Restore Mode: %s", restore_mode);
  if (!this->interlock_.empty()) {
    ESP_LOGCONFIG(TAG, "  Interlocks:");
    for (auto *lock : this->interlock_) {
      if (lock == this)
        continue;
      ESP_LOGCONFIG(TAG, "    %s", lock->get_name().c_str());
    }
  }
}
void GPIOSwitch::write_state(bool state) {
  if (state != this->inverted_) {
    // Turning ON, check interlocking
    for (auto *lock : this->interlock_) {
      if (lock == this)
        continue;

      if (lock->state)
        lock->turn_off();
    }
  }

  this->pin_->digital_write(state);
  this->publish_state(state);
}
void GPIOSwitch::set_restore_mode(GPIOSwitchRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }
void GPIOSwitch::set_interlock(const std::vector<Switch *> &interlock) { this->interlock_ = interlock; }

}  // namespace gpio
}  // namespace esphome
