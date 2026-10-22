#include "m5pm1.h"

#include <algorithm>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5pm1 {

ESPHOME_LOG_TAG(TAG, "m5pm1");

void M5PM1Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5PM1...");

  // The M5PM1 reset/download sequence powers the rails off for 500 ms before turning them on again.
  // Give that sequence time to finish before the first I2C transaction, then tolerate a slow firmware start.
  delay(500);  // NOLINT
  if (!this->read_device_info_()) {
    this->mark_failed(LOG_STR("Unable to read device information"));
    return;
  }

  if (!this->read_initial_state_()) {
    this->mark_failed(LOG_STR("Unable to read initial state"));
    return;
  }

  // The status LED is always enabled independently of configurable power rails.
  if (!this->set_power_config(M5PM1_POWER_CONFIG_LED, true)) {
    this->mark_failed(LOG_STR("Unable to enable status LED"));
    return;
  }

  // write pwm configuration
  if (!this->update_pwm_frequency(this->pwm_frequency_)) {
    this->mark_failed(LOG_STR("Unable to write PWM frequency"));
    return;
  }

  // Restore the configured button behavior because M5PM1 reset and download commands reset these registers.
  if (!this->set_button_timing(this->single_click_timing_, this->long_press_timing_, this->double_click_timing_) ||
      !this->set_single_click_reset_disabled(!this->single_click_reset_) ||
      !this->set_double_click_poweroff_disabled(!this->double_click_poweroff_) ||
      !this->set_download_mode_locked(!this->long_press_download_)) {
    this->mark_failed(LOG_STR("Unable to configure power button"));
    return;
  }

  if (this->interrupt_pin_ != nullptr) {
    if (!this->valid_pin_(this->interrupt_gpio_)) {
      this->mark_failed(LOG_STR("Invalid IRQ output GPIO"));
      return;
    }
    // Keep the IRQ output itself masked and apply all event masks in one I2C transaction.
    this->irq_masks_.gpio |= 1 << this->interrupt_gpio_;
    const uint8_t enabled_power_interrupts = ~this->irq_masks_.power & 0x3F;
    if ((enabled_power_interrupts & (M5PM1_IRQ_POWER_BATTERY_INSERTED | M5PM1_IRQ_POWER_BATTERY_REMOVED)) != 0 &&
        (this->power_config_ & M5PM1_POWER_CONFIG_CHARGER) != 0) {
      ESP_LOGW(TAG, "Battery IRQ events require the charger to be disabled");
    }
    if ((enabled_power_interrupts & (M5PM1_IRQ_POWER_VINOUT_INSERTED | M5PM1_IRQ_POWER_VINOUT_REMOVED)) != 0 &&
        (this->power_config_ & M5PM1_POWER_CONFIG_5V_IN_OUT) != 0) {
      ESP_LOGW(TAG, "5V IN/OUT IRQ events require 5V boost to be disabled");
    }
    if (!this->set_irq_masks(this->irq_masks_) ||
        !this->update_register_(REG_GPIO_DRV, 1 << this->interrupt_gpio_, 1 << this->interrupt_gpio_) ||
        !this->set_pin_function(this->interrupt_gpio_, M5PM1PinFunction::M5PM1_PIN_FUNCTION_IRQ)) {
      this->mark_failed(LOG_STR("Unable to configure IRQ output"));
      return;
    }
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(&M5PM1Component::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
    this->set_invalidate_on_read_(false);
    // A pending IRQ may already hold the line low before the host interrupt is attached.
    if (this->interrupt_pin_->digital_read()) {
      this->disable_loop();
    } else {
      this->irq_pending_ = true;
    }
  }
}

bool M5PM1Component::read_device_info_() {
  static constexpr uint8_t MAX_ATTEMPTS = 6;
  static constexpr uint32_t RETRY_DELAY_MS = 100;

  for (uint8_t attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
    M5PM1DeviceInfo device_info{};
    if (this->read_bytes(REG_DEVICE_ID, reinterpret_cast<uint8_t *>(&device_info), sizeof(device_info))) {
      if (device_info.device_id == M5PM1_DEVICE_ID && device_info.device_model == M5PM1_DEVICE_MODEL) {
        this->device_info_ = device_info;
        return true;
      }
      ESP_LOGW(TAG, "Unexpected device ID/model on attempt %u: 0x%02X/0x%02X", attempt + 1, device_info.device_id,
               device_info.device_model);
    }
    if (attempt + 1 < MAX_ATTEMPTS) {
      delay(RETRY_DELAY_MS);  // NOLINT
    }
  }
  return false;
}

void M5PM1Component::loop() {
  if (this->interrupt_pin_ == nullptr) {
    return;
  }
  if (this->irq_pending_) {
    // Snapshot and clear all three IRQ groups together before publishing events.
    this->irq_pending_ = false;
    if (!this->process_irq_()) {
      this->irq_pending_ = true;
      return;
    }
  }
  // Sleep this component again once the M5PM1 has released the IRQ line.
  if (this->interrupt_pin_->digital_read() && !this->irq_pending_) {
    this->disable_loop();
  } else if (!this->interrupt_pin_->digital_read()) {
    // The line may still be low if another event arrived while statuses were cleared.
    this->irq_pending_ = true;
  }
}

bool M5PM1Component::process_irq_() {
  M5PM1IRQStatus status{};
  if (!this->read_irq_status(&status)) {
    this->set_warning_("Failed to read IRQ status");
    return false;
  }

  this->pending_irq_status_.gpio |= status.gpio;
  this->pending_irq_status_.power |= status.power;
  this->pending_irq_status_.system |= status.system;

  if ((status.system & M5PM1_IRQ_SYSTEM_WAKEUP) != 0) {
    uint8_t wake_source;
    if (!this->read_wake_source(&wake_source)) {
      this->set_warning_("Failed to read wake source");
      return false;
    }
    this->pending_wake_source_ |= wake_source & 0x7F;
    // WAKE_SRC holds the wakeup status bit active, so clear it before IRQ Status 3.
    if (!this->write_byte(REG_WAKE_SRC, 0)) {
      this->set_warning_("Failed to clear wake source");
      return false;
    }
    this->wake_source_ = 0;
  }

  if (!this->clear_irq_status()) {
    this->set_warning_("Failed to clear IRQ status");
    return false;
  }

  if (this->pending_irq_status_.gpio != 0) {
    // Invalidate once so all GPIO consumers share the next refreshed input value.
    this->reset_pin_cache_();
  }
  this->dispatch_irq_events_();
  this->status_clear_warning();
  return true;
}

void M5PM1Component::dispatch_irq_events_() {
  const M5PM1IRQStatus status = this->pending_irq_status_;
  const uint8_t wake_source = this->pending_wake_source_;
  this->pending_irq_status_ = {};
  this->pending_wake_source_ = 0;

  if (status.power != 0) {
    this->power_interrupt_callback_.call(status.power);
  }
  if (status.system != 0) {
    this->system_interrupt_callback_.call(status.system, wake_source);
  }
}

void M5PM1Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5PM1:\n"
                "  Device ID: 0x%02X\n"
                "  Device Model: 0x%02X\n"
                "  Hardware Revision: %u\n"
                "  Software Revision: %u",
                this->device_info_.device_id, this->device_info_.device_model, this->device_info_.hardware_revision,
                this->device_info_.software_revision);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  if (this->interrupt_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  M5PM1 IRQ GPIO: %u", this->interrupt_gpio_);
  }
  static constexpr uint16_t button_timing_ms[] = {125, 250, 500, 1000};
  static constexpr uint16_t long_press_timing_ms[] = {1000, 2000, 3000, 4000};
  ESP_LOGCONFIG(TAG,
                "  Power Button:\n"
                "    Single click: %u ms, reset: %s, IRQ: %s\n"
                "    Double click: %u ms, power off: %s, IRQ: %s\n"
                "    Long press: %u ms, download: %s",
                button_timing_ms[static_cast<uint8_t>(this->single_click_timing_)], YESNO(this->single_click_reset_),
                YESNO((this->irq_masks_.system & M5PM1_IRQ_SYSTEM_SINGLE_CLICK) == 0),
                button_timing_ms[static_cast<uint8_t>(this->double_click_timing_)], YESNO(this->double_click_poweroff_),
                YESNO((this->irq_masks_.system & M5PM1_IRQ_SYSTEM_DOUBLE_CLICK) == 0),
                long_press_timing_ms[static_cast<uint8_t>(this->long_press_timing_)],
                YESNO(this->long_press_download_));
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void IRAM_ATTR M5PM1Component::gpio_intr(M5PM1Component *arg) {
  arg->irq_pending_ = true;
  arg->enable_loop_soon_any_context();
}

void M5PM1Component::set_gpio_interrupt_enabled(uint8_t pin, bool enabled) {
  if (!this->valid_pin_(pin)) {
    return;
  }
  const uint8_t mask = 1 << pin;
  if (enabled) {
    this->irq_masks_.gpio &= ~mask;
  } else {
    this->irq_masks_.gpio |= mask;
  }
}

void M5PM1Component::set_power_interrupt_enabled(uint8_t mask) { this->irq_masks_.power &= ~(mask & 0x3F); }

void M5PM1Component::set_system_interrupt_enabled(uint8_t mask) { this->irq_masks_.system &= ~(mask & 0x07); }

void M5PM1Component::set_single_click_interrupt_enabled(bool enabled) {
  if (enabled) {
    this->irq_masks_.system &= ~M5PM1_IRQ_SYSTEM_SINGLE_CLICK;
  } else {
    this->irq_masks_.system |= M5PM1_IRQ_SYSTEM_SINGLE_CLICK;
  }
}

void M5PM1Component::set_double_click_interrupt_enabled(bool enabled) {
  if (enabled) {
    this->irq_masks_.system &= ~M5PM1_IRQ_SYSTEM_DOUBLE_CLICK;
  } else {
    this->irq_masks_.system |= M5PM1_IRQ_SYSTEM_DOUBLE_CLICK;
  }
}

bool M5PM1Component::read_initial_state_() {
  bool success = this->read_byte(REG_GPIO_MODE, &this->mode_mask_);
  success &= this->read_byte(REG_GPIO_OUT, &this->output_mask_);
  success &= this->read_byte(REG_GPIO_DRV, &this->drive_mask_);
  success &= this->read_byte(REG_PWR_CFG, &this->power_config_);
  success &= this->read_byte(REG_HOLD_CFG, &this->power_hold_);
  success &= this->read_byte(REG_WAKE_SRC, &this->wake_source_);
  return success;
}

void M5PM1Component::set_warning_(const char *message) {
  ESP_LOGW(TAG, "%s", message);
  this->status_set_warning(message);
}

bool M5PM1Component::read_u16_(uint8_t reg, uint16_t *value) {
  uint8_t data[2];
  if (!this->read_bytes(reg, data, sizeof(data))) {
    this->set_warning_("Failed to read register");
    return false;
  }
  *value = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
  this->status_clear_warning();
  return true;
}

bool M5PM1Component::write_u16_(uint8_t reg, uint16_t value) {
  const uint8_t data[2] = {uint8_t(value), uint8_t(value >> 8)};
  if (!this->write_bytes(reg, data, sizeof(data))) {
    this->set_warning_("Failed to write register");
    return false;
  }
  this->status_clear_warning();
  return true;
}

bool M5PM1Component::update_register_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current;
  if (!this->read_byte(reg, &current) || !this->write_byte(reg, (current & ~mask) | (value & mask))) {
    this->set_warning_("Failed to update register");
    return false;
  }
  this->status_clear_warning();
  return true;
}

bool M5PM1Component::write_pin_field_(uint8_t pin, uint8_t reg0, uint8_t reg1, uint8_t value) {
  if (!this->valid_pin_(pin)) {
    return false;
  }
  const uint8_t reg = pin < 4 ? reg0 : reg1;
  const uint8_t shift = pin < 4 ? pin * 2 : 0;
  return this->update_register_(reg, 0b11 << shift, value << shift);
}

bool M5PM1Component::set_pin_function(uint8_t pin, M5PM1PinFunction function) {
  return this->write_pin_field_(pin, REG_GPIO_FUNC_0, REG_GPIO_FUNC_1, static_cast<uint8_t>(function));
}

void M5PM1Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (!this->valid_pin_(pin) || pin == this->interrupt_gpio_) {
    ESP_LOGE(TAG, "GPIO%u is invalid or reserved for IRQ", pin);
    return;
  }

  bool success = this->set_pin_function(pin, M5PM1PinFunction::M5PM1_PIN_FUNCTION_GPIO);
  const bool output = (flags & gpio::FLAG_OUTPUT) != 0;
  const uint8_t pin_mask = 1 << pin;
  if (output) {
    this->mode_mask_ |= pin_mask;
  } else {
    this->mode_mask_ &= ~pin_mask;
  }
  success &= this->write_byte(REG_GPIO_MODE, this->mode_mask_);

  M5PM1PullMode pull = M5PM1PullMode::M5PM1_PULL_MODE_NONE;
  if (flags & gpio::FLAG_PULLUP) {
    pull = M5PM1PullMode::M5PM1_PULL_MODE_UP;
  } else if (flags & gpio::FLAG_PULLDOWN) {
    pull = M5PM1PullMode::M5PM1_PULL_MODE_DOWN;
  }
  success &= this->write_pin_field_(pin, REG_GPIO_PU_PD_0, REG_GPIO_PU_PD_1, static_cast<uint8_t>(pull));

  if (output) {
    if (flags & gpio::FLAG_OPEN_DRAIN) {
      this->drive_mask_ |= pin_mask;
    } else {
      this->drive_mask_ &= ~pin_mask;
    }
    success &= this->write_byte(REG_GPIO_DRV, this->drive_mask_);
  }
  if (!success) {
    this->set_warning_("Failed to configure GPIO");
  }
}

bool M5PM1Component::set_pin_wakeup(uint8_t pin, bool enable, M5PM1WakeupEdge edge) {
  if (!this->valid_pin_(pin) || pin == 1) {
    return false;
  }
  const uint8_t mask = 1 << pin;
  uint8_t wake_enable;
  if (!this->read_byte(REG_GPIO_WAKE_EN, &wake_enable)) {
    return false;
  }
  const uint8_t conflicting_mask = pin == 0 ? 1 << 2 : pin == 2 ? 1 << 0 : pin == 3 ? 1 << 4 : 1 << 3;
  if (enable && (wake_enable & conflicting_mask)) {
    ESP_LOGE(TAG, "GPIO%u conflicts with an enabled wake source", pin);
    return false;
  }
  if (!this->update_register_(REG_GPIO_WAKE_CFG, mask, edge == M5PM1WakeupEdge::M5PM1_WAKEUP_EDGE_RISING ? mask : 0)) {
    return false;
  }
  return this->update_register_(REG_GPIO_WAKE_EN, mask, enable ? mask : 0);
}

bool M5PM1Component::set_pin_power_hold(uint8_t pin, bool enable) {
  if (!this->valid_pin_(pin)) {
    return false;
  }
  return this->set_power_hold(1 << pin, enable);
}

bool M5PM1Component::digital_read_hw(uint8_t pin) {
  if (!this->valid_pin_(pin) || !this->read_byte(REG_GPIO_IN, &this->input_mask_)) {
    this->set_warning_("Failed to read GPIO input");
    return false;
  }
  this->status_clear_warning();
  return true;
}

bool M5PM1Component::digital_read_cache(uint8_t pin) { return (this->input_mask_ & (1 << pin)) != 0; }

void M5PM1Component::digital_write_hw(uint8_t pin, bool value) {
  if (!this->valid_pin_(pin) || pin == this->interrupt_gpio_) {
    return;
  }
  if (value) {
    this->output_mask_ |= 1 << pin;
  } else {
    this->output_mask_ &= ~(1 << pin);
  }
  if (!this->write_byte(REG_GPIO_OUT, this->output_mask_)) {
    this->set_warning_("Failed to write GPIO output");
  } else {
    this->status_clear_warning();
  }
}

bool M5PM1Component::read_power_source(uint8_t *value) { return this->read_byte(REG_PWR_SRC, value); }

bool M5PM1Component::read_wake_source(uint8_t *value) {
  if (!this->read_byte(REG_WAKE_SRC, value)) {
    return false;
  }
  this->wake_source_ = *value;
  return true;
}

bool M5PM1Component::clear_wake_source(uint8_t mask) {
  uint8_t wake_source;
  if (!this->read_byte(REG_WAKE_SRC, &wake_source) || !this->write_byte(REG_WAKE_SRC, wake_source & ~(mask & 0x7F))) {
    return false;
  }
  this->wake_source_ = wake_source & ~mask;
  return true;
}

bool M5PM1Component::read_power_config(uint8_t *value) {
  if (!this->read_byte(REG_PWR_CFG, value)) {
    return false;
  }
  this->power_config_ = *value;
  return true;
}

bool M5PM1Component::set_power_config(uint8_t mask, bool enable) {
  mask &= 0x1F;

  if (!this->read_power_config(&this->power_config_)) {
    return false;
  }

  const uint8_t value = enable ? (this->power_config_ | mask) : (this->power_config_ & ~mask);
  if (value == this->power_config_) {
    return true;
  }

  if (enable && (mask & M5PM1_POWER_CONFIG_5V_IN_OUT) != 0) {
    uint8_t power_source;
    uint16_t voltage;
    // Never drive the bidirectional 5V rail against an external supply.
    if (!this->read_power_source(&power_source) || !this->read_5vout_voltage(&voltage)) {
      ESP_LOGE(TAG, "Refusing to enable 5V boost because external power detection failed");
      return false;
    }
    if ((power_source & M5PM1_POWER_SOURCE_5V_IN_OUT) != 0 || voltage >= 4000) {
      ESP_LOGE(TAG, "Refusing to enable 5V boost while 5V IN/OUT is externally powered");
      ESP_LOGV(TAG, "PowerSource: 0x%02" PRIx8 ", voltage %d", power_source, voltage);
      return false;
    }
  }

  if (!this->write_byte(REG_PWR_CFG, value)) {
    return false;
  }
  this->power_config_ = value;
  return true;
}

bool M5PM1Component::read_power_hold(uint8_t *value) {
  if (!this->read_byte(REG_HOLD_CFG, value)) {
    return false;
  }
  this->power_hold_ = *value;
  return true;
}

bool M5PM1Component::set_power_hold(uint8_t mask, bool enable) {
  mask &= 0x7F;
  if (!this->read_power_hold(&this->power_hold_)) {
    return false;
  }
  const uint8_t value = enable ? (this->power_hold_ | mask) : (this->power_hold_ & ~mask);
  if (!this->write_byte(REG_HOLD_CFG, value)) {
    return false;
  }
  this->power_hold_ = value;
  return true;
}

bool M5PM1Component::set_battery_low_voltage(uint16_t millivolts) {
  if (millivolts < 2000 || millivolts > 3992) {
    return false;
  }
  const uint8_t factor = std::min<uint16_t>(255, ((millivolts - 2000) * 100 + 390) / 781);
  return this->write_byte(REG_BATT_LVP, factor);
}

bool M5PM1Component::set_i2c_sleep_timeout(uint8_t seconds) {
  if (seconds > 15) {
    return false;
  }
  return this->update_register_(REG_I2C_CFG, 0x0F, seconds);
}

bool M5PM1Component::set_watchdog_timeout(uint8_t seconds) { return this->write_byte(REG_WDT_CNT, seconds); }

bool M5PM1Component::feed_watchdog() { return this->write_byte(REG_WDT_KEY, WDT_KEY); }

bool M5PM1Component::system_command(M5PM1SystemCommand command) {
  // REBOOT is a PM1 power/GPIO reset, not a host-side software reset. The PM1
  // removes power for 500 ms and then powers the rails back on.
  return this->write_byte(REG_SYS_CMD, SYS_CMD_KEY | static_cast<uint8_t>(command));
}

bool M5PM1Component::read_vref_voltage(uint16_t *millivolts) { return this->read_u16_(REG_ADC_VREF_L, millivolts); }

bool M5PM1Component::read_battery_voltage(uint16_t *millivolts) { return this->read_u16_(REG_ADC_VBAT_L, millivolts); }

bool M5PM1Component::read_vin_voltage(uint16_t *millivolts) { return this->read_u16_(REG_ADC_5V_IN_L, millivolts); }

bool M5PM1Component::read_5vout_voltage(uint16_t *millivolts) { return this->read_u16_(REG_ADC_5V_OUT_L, millivolts); }

bool M5PM1Component::read_adc(M5PM1ADCChannel channel, uint16_t *value) {
  const uint8_t channel_value = static_cast<uint8_t>(channel);
  if (channel_value != 1 && channel_value != 2 && channel_value != 6) {
    return false;
  }
  if (!this->write_byte(REG_ADC_CTRL, (channel_value << 1) | 1)) {
    return false;
  }
  uint8_t control = 1;
  for (uint8_t tries = 0; tries < 50 && (control & 1); tries++) {
    delay(10);
    if (!this->read_byte(REG_ADC_CTRL, &control)) {
      return false;
    }
  }
  if (control & 1) {
    this->set_warning_("ADC conversion timed out");
    return false;
  }
  if (!this->read_u16_(REG_ADC_RES_L, value)) {
    return false;
  }
  *value &= 0x0FFF;
  return true;
}

bool M5PM1Component::update_pwm_frequency(uint16_t frequency) {
  return frequency != 0 && this->write_u16_(REG_PWM_FREQ_L, frequency);
}

bool M5PM1Component::get_pwm_frequency(uint16_t *frequency) { return this->read_u16_(REG_PWM_FREQ_L, frequency); }

bool M5PM1Component::set_pwm(M5PM1PWMChannel channel, uint16_t duty, M5PM1PWMPolarity polarity, bool enable) {
  const uint8_t channel_value = static_cast<uint8_t>(channel);
  if (channel_value > 1 || duty > 0x0FFF) {
    return false;
  }
  const uint8_t pin = channel_value + 3;
  if (!this->set_pin_function(pin, M5PM1PinFunction::M5PM1_PIN_FUNCTION_ALTERNATE)) {
    return false;
  }
  uint8_t data[2] = {uint8_t(duty), uint8_t((duty >> 8) & 0x0F)};
  if (enable) {
    data[1] |= PWM_ENABLE;
  }
  if (polarity == M5PM1PWMPolarity::M5PM1_PWM_POLARITY_ACTIVE_LOW) {
    data[1] |= PWM_ACTIVE_LOW;
  }
  return this->write_bytes(channel_value == 0 ? REG_PWM_0_L : REG_PWM_1_L, data, sizeof(data));
}

bool M5PM1Component::get_pwm(M5PM1PWMChannel channel, uint16_t *duty, M5PM1PWMPolarity *polarity, bool *enable) {
  const uint8_t channel_value = static_cast<uint8_t>(channel);
  if (channel_value > 1) {
    return false;
  }
  uint8_t data[2];
  if (!this->read_bytes(channel_value == 0 ? REG_PWM_0_L : REG_PWM_1_L, data, sizeof(data))) {
    return false;
  }
  *duty = uint16_t(data[0]) | (uint16_t(data[1] & 0x0F) << 8);
  *polarity = (data[1] & PWM_ACTIVE_LOW) ? M5PM1PWMPolarity::M5PM1_PWM_POLARITY_ACTIVE_LOW
                                         : M5PM1PWMPolarity::M5PM1_PWM_POLARITY_ACTIVE_HIGH;
  *enable = (data[1] & PWM_ENABLE) != 0;
  return true;
}

bool M5PM1Component::set_timer(uint32_t seconds, M5PM1TimerAction action) {
  if (seconds > M5PM1_TIMER_MAX_SECONDS || static_cast<uint8_t>(action) > 4) {
    return false;
  }
  const uint8_t data[4] = {uint8_t(seconds), uint8_t(seconds >> 8), uint8_t(seconds >> 16),
                           uint8_t((seconds >> 24) & 0x7F)};
  if (!this->write_bytes(REG_TIM_CNT_BYTE_0, data, sizeof(data)) ||
      !this->write_byte(REG_TIM_CFG, (1 << 3) | static_cast<uint8_t>(action))) {
    return false;
  }
  return this->write_byte(REG_TIM_KEY, TIMER_KEY);
}

bool M5PM1Component::clear_timer() {
  return this->write_byte(REG_TIM_CFG, 0) && this->write_byte(REG_TIM_KEY, TIMER_KEY);
}

bool M5PM1Component::read_irq_status(M5PM1IRQStatus *status) {
  uint8_t data[3];
  if (!this->read_bytes(REG_IRQ_STAT_1, data, sizeof(data))) {
    return false;
  }
  status->gpio = data[0] & 0x1F;
  status->power = data[1] & 0x3F;
  status->system = data[2] & 0x07;
  return true;
}

bool M5PM1Component::clear_irq_status() {
  const uint8_t clear_data[3] = {0, 0, 0};
  return this->write_bytes(REG_IRQ_STAT_1, clear_data, sizeof(clear_data));
}

bool M5PM1Component::set_irq_masks(const M5PM1IRQStatus &masks) {
  const uint8_t data[3] = {uint8_t(masks.gpio & 0x1F), uint8_t(masks.power & 0x3F), uint8_t(masks.system & 0x07)};
  return this->write_bytes(REG_IRQ_STAT_1_MASK, data, sizeof(data));
}

bool M5PM1Component::get_irq_masks(M5PM1IRQStatus *masks) {
  uint8_t data[3];
  if (!this->read_bytes(REG_IRQ_STAT_1_MASK, data, sizeof(data))) {
    return false;
  }
  masks->gpio = data[0] & 0x1F;
  masks->power = data[1] & 0x3F;
  masks->system = data[2] & 0x07;
  return true;
}

bool M5PM1Component::get_button_state(bool *pressed, bool *pressed_since_last_read) {
  uint8_t value;
  if (!this->read_byte(REG_BTN_STAT, &value)) {
    return false;
  }
  *pressed = (value & 0x01) != 0;
  *pressed_since_last_read = (value & 0x80) != 0;
  return true;
}

bool M5PM1Component::set_button_timing(M5PM1ButtonTiming single, M5PM1LongPressTiming long_press,
                                       M5PM1ButtonTiming double_click) {
  const uint8_t mask = 0x7E;
  const uint8_t value = (static_cast<uint8_t>(single) << 1) | (static_cast<uint8_t>(long_press) << 3) |
                        (static_cast<uint8_t>(double_click) << 5);
  return this->update_register_(REG_BTN_CFG_1, mask, value);
}

bool M5PM1Component::set_single_click_reset_disabled(bool disabled) {
  return this->update_register_(REG_BTN_CFG_1, 0x01, disabled ? 0x01 : 0);
}

bool M5PM1Component::set_double_click_poweroff_disabled(bool disabled) {
  return this->update_register_(REG_BTN_CFG_2, 0x01, disabled ? 0x01 : 0);
}

bool M5PM1Component::set_download_mode_locked(bool locked) {
  return this->update_register_(REG_BTN_CFG_1, 0x80, locked ? 0x80 : 0);
}

bool M5PM1Component::set_neopixel_count(uint8_t count) {
  return count <= M5PM1_NEOPIXEL_COUNT && this->write_byte(REG_NEO_CFG, count);
}

bool M5PM1Component::write_neopixel(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
  if (index >= M5PM1_NEOPIXEL_COUNT) {
    return false;
  }
  const uint16_t color = (uint16_t(red & 0xF8) << 8) | (uint16_t(green & 0xFC) << 3) | (blue >> 3);
  return this->write_u16_(REG_NEO_DATA + index * 2, color);
}

bool M5PM1Component::write_neopixels(const uint16_t *rgb565, size_t count) {
  if (rgb565 == nullptr || count > M5PM1_NEOPIXEL_COUNT) {
    return false;
  }
  uint8_t data[M5PM1_NEOPIXEL_COUNT * 2];
  for (size_t i = 0; i < count; i++) {
    data[i * 2] = rgb565[i];
    data[i * 2 + 1] = rgb565[i] >> 8;
  }
  return this->write_bytes(REG_NEO_DATA, data, count * 2);
}

bool M5PM1Component::refresh_neopixels() {
  uint8_t config;
  return this->read_byte(REG_NEO_CFG, &config) && this->write_byte(REG_NEO_CFG, config | NEO_REFRESH);
}

bool M5PM1Component::set_aw8737a_pulse(uint8_t pin, uint8_t pulse_count, bool refresh) {
  if (!this->valid_pin_(pin) || pulse_count > 3) {
    return false;
  }

  uint8_t value = pin | (pulse_count << 5);
  if (refresh) {
    value |= AW8737A_REFRESH;
  }
  return this->write_byte(REG_AW8737A_CFG, value);
}

bool M5PM1Component::write_rtc_memory(uint8_t offset, const uint8_t *data, size_t length) {
  if (data == nullptr || length == 0 || offset >= M5PM1_RTC_MEMORY_SIZE || length > M5PM1_RTC_MEMORY_SIZE - offset) {
    return false;
  }
  return this->write_bytes(REG_RTC_DATA + offset, data, length);
}

bool M5PM1Component::read_rtc_memory(uint8_t offset, uint8_t *data, size_t length) {
  if (data == nullptr || length == 0 || offset >= M5PM1_RTC_MEMORY_SIZE || length > M5PM1_RTC_MEMORY_SIZE - offset) {
    return false;
  }
  return this->read_bytes(REG_RTC_DATA + offset, data, length);
}

void M5PM1GPIOPin::setup() {
  this->pin_mode(this->flags_);
  // Wake settings survive host resets, so initialize both enabled and disabled pins from YAML.
  if (this->pin_ != 1 && !this->parent_->set_pin_wakeup(this->pin_, this->wakeup_, this->wakeup_edge_)) {
    ESP_LOGE(TAG, "Failed to configure GPIO%u wakeup", this->pin_);
  }
}

void M5PM1GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }

bool M5PM1GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }

void M5PM1GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

size_t M5PM1GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via M5PM1", this->pin_);
}

}  // namespace esphome::m5pm1
