#include "hbridge_switch.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace hbridge {

static const char *const TAG = "switch.hbridge";

float HBridgeSwitch::get_setup_priority() const { return setup_priority::HARDWARE; }
void HBridgeSwitch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up H-Bridge Switch '%s'...", this->name_.c_str());

  optional<bool> initial_state = this->get_initial_state_with_restore_mode();

  // Like GPIOSwitch does, set the pin state both before and after pin setup()
  this->on_pin_->digital_write(false);
  this->on_pin_->setup();
  this->on_pin_->digital_write(false);

  this->off_pin_->digital_write(false);
  this->off_pin_->setup();
  this->off_pin_->digital_write(false);

  if (initial_state.has_value())
    this->write_state(initial_state.value());
}

void HBridgeSwitch::dump_config() {
  LOG_SWITCH("", "H-Bridge Switch", this);
  LOG_PIN("  On Pin: ", this->on_pin_);
  LOG_PIN("  Off Pin: ", this->off_pin_);
  ESP_LOGCONFIG(TAG, "  Pulse length: %" PRId32 " ms", this->pulse_length_);
  if (this->wait_time_)
    ESP_LOGCONFIG(TAG, "  Wait time %" PRId32 " ms", this->wait_time_);
}

void HBridgeSwitch::write_state(bool state) {
  this->desired_state_ = state;
  if (!this->timer_running_)
    this->timer_fn_();
}

void HBridgeSwitch::timer_fn_() {
  uint32_t next_timeout = 0;

  while ((uint8_t) this->desired_state_ != this->relay_state_) {
    switch (this->relay_state_) {
      case RELAY_STATE_ON:
      case RELAY_STATE_OFF:
      case RELAY_STATE_UNKNOWN:
        if (this->desired_state_) {
          this->on_pin_->digital_write(true);
          this->relay_state_ = RELAY_STATE_SWITCHING_ON;
        } else {
          this->off_pin_->digital_write(true);
          this->relay_state_ = RELAY_STATE_SWITCHING_OFF;
        }
        next_timeout = this->pulse_length_;
        if (!this->optimistic_)
          this->publish_state(this->desired_state_);
        break;

      case RELAY_STATE_SWITCHING_ON:
        this->on_pin_->digital_write(false);
        this->relay_state_ = RELAY_STATE_ON;
        if (this->optimistic_)
          this->publish_state(true);
        next_timeout = this->wait_time_;
        break;

      case RELAY_STATE_SWITCHING_OFF:
        this->off_pin_->digital_write(false);
        this->relay_state_ = RELAY_STATE_OFF;
        if (this->optimistic_)
          this->publish_state(false);
        next_timeout = this->wait_time_;
        break;
    }

    if (next_timeout) {
      this->timer_running_ = true;
      this->set_timeout(next_timeout, [this]() { this->timer_fn_(); });
      return;
    }

    // In the case where ON/OFF state has been reached but we need to
    // immediately change back again to reach desired_state_, we loop.
  }
  this->timer_running_ = false;
}

}  // namespace hbridge
}  // namespace esphome
