#include "m5pm1_switch.h"

namespace esphome::m5pm1 {

static const char *const TAG = "m5pm1.switch";

void AW8737ASwitch::setup() {
  bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);

  // write state before setup
  if (initial_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }

  // configured the pin as output
  this->pin_->setup();

  // write after setup again for other IOs
  if (initial_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

void AW8737ASwitch::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5PM1 Switch: "
                "  Pin: IO%d"
                "  Pulse number: %d",
                this->pin_->get_pin(), this->pulse_num_);
  LOG_SWITCH("  ", "AW8737A Switch", this);
}

void AW8737ASwitch::write_state(bool state) {
  this->parent_->set_aw8737a_pulse(this->pin_->get_pin(), this->pulse_num_, state);
  this->publish_state(state);
}

void M5PM1PowerSwitch::setup() {
  if (this->parent_->is_failed()) {
    this->mark_failed(LOG_STR("M5PM1 parent component is unavailable"));
    return;
  }

  const uint8_t value = this->is_hold_switch_() ? this->parent_->get_power_hold() : this->parent_->get_power_config();
  const bool state = (value & this->get_mask_()) != 0;
  if (state && this->is_gpio_hold_switch_() && this->parent_->is_interrupt_gpio(this->pin_)) {
    ESP_LOGW(TAG, "GPIO%u hold also preserves the IRQ pin function during shutdown", this->pin_);
  }
  this->publish_state(state);
}

void M5PM1PowerSwitch::dump_config() {
  LOG_SWITCH("  ", "M5PM1 Power Switch", this);
  if (this->is_gpio_hold_switch_()) {
    ESP_LOGCONFIG(TAG, "  GPIO: %u", this->pin_);
  }
}

void M5PM1PowerSwitch::write_state(bool state) {
  if (!state && this->type_ == M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_DCDC_3V3) {
    ESP_LOGW(TAG, "Disabling 3.3V DCDC may power down the host MCU");
  }
  if (state && this->is_gpio_hold_switch_() && this->parent_->is_interrupt_gpio(this->pin_)) {
    ESP_LOGW(TAG, "GPIO%u hold also preserves the IRQ pin function during shutdown", this->pin_);
  }

  // Hold bits preserve the current rail state during shutdown; they do not enable the rail.
  bool success;
  if (this->is_gpio_hold_switch_()) {
    success = this->parent_->set_pin_power_hold(this->pin_, state);
  } else if (this->is_hold_switch_()) {
    success = this->parent_->set_power_hold(this->get_mask_(), state);
  } else {
    success = this->parent_->set_power_config(this->get_mask_(), state);
  }
  if (!success) {
    ESP_LOGE(TAG, "Failed to set power switch '%s' to %s", LOG_STR_ARG(this->get_log_name()), ONOFF(state));
    return;
  }
  this->publish_state(state);
}

bool M5PM1PowerSwitch::is_hold_switch_() const {
  return this->type_ == M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_HOLD_LDO_3V3 ||
         this->type_ == M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_HOLD_BOOST_5V || this->is_gpio_hold_switch_();
}

bool M5PM1PowerSwitch::is_gpio_hold_switch_() const {
  return this->type_ == M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_HOLD_GPIO;
}

uint8_t M5PM1PowerSwitch::get_mask_() const {
  switch (this->type_) {
    case M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_CHARGER:
      return M5PM1_POWER_CONFIG_CHARGER;
    case M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_DCDC_3V3:
      return M5PM1_POWER_CONFIG_DCDC_3V3;
    case M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_LDO_3V3:
      return M5PM1_POWER_CONFIG_LDO_3V3;
    case M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_BOOST_5V:
      return M5PM1_POWER_CONFIG_5V_IN_OUT;
    case M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_HOLD_LDO_3V3:
      return M5PM1_POWER_HOLD_LDO_3V3;
    case M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_HOLD_BOOST_5V:
      return M5PM1_POWER_HOLD_5V_IN_OUT;
    case M5PM1PowerSwitchType::M5PM1_POWER_SWITCH_TYPE_HOLD_GPIO:
      return 1 << this->pin_;
  }
  return 0;
}

}  // namespace esphome::m5pm1
