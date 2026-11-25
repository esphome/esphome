#include "m5stack_pm1.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5stack_pm1 {

ESPHOME_LOG_TAG(TAG, "m5stack_pm1");

void M5StackPM1Component::setup() {
  // The M5Stack PM1 reset/download sequence powers the rails off for 500 ms before turning them on again.
  // Give that sequence time to finish before the first I2C transaction, then tolerate a slow firmware start.
  delay(500);  // NOLINT
  if (!this->setup_check(this->configure_i2c_speed_(), LOG_STR("Unable to configure I2C speed")) ||
      !this->setup_check(this->read_device_info_(), LOG_STR("Unable to read device information")) ||
      !this->setup_check(this->read_initial_state_(), LOG_STR("Unable to read initial state")) ||
      !this->setup_check(this->set_power_config_(M5STACK_PM1_POWER_CONFIG_LED, true),
                         LOG_STR("Unable to enable status LED")) ||
      !this->setup_check(this->write_byte(REG_GPIO_WAKE_EN, 0), LOG_STR("Unable to clear GPIO wake configuration"))) {
    return;
  }

  if (this->interrupt_pin_ != nullptr) {
    if (!this->setup_check(this->valid_pin_(this->interrupt_gpio_), LOG_STR("Invalid IRQ output GPIO"))) {
      return;
    }
    // Keep the IRQ output itself masked and apply all masks in one I2C transaction.
    this->irq_masks_.gpio |= 1 << this->interrupt_gpio_;
    if (!this->setup_check(
            this->set_irq_masks_(this->irq_masks_) &&
                this->update_register(REG_GPIO_DRV, 1 << this->interrupt_gpio_, 1 << this->interrupt_gpio_) &&
                this->set_pin_function(this->interrupt_gpio_, M5StackPM1PinFunction::M5STACK_PM1_PIN_FUNCTION_IRQ),
            LOG_STR("Unable to configure IRQ output"))) {
      return;
    }
  }
  // Attach the IRQ line last: it also decides whether the loop starts enabled.
  this->setup_interrupt_pin_();
}

bool M5StackPM1Component::read_device_info_() {
  static constexpr uint8_t MAX_ATTEMPTS = 6;
  static constexpr uint32_t RETRY_DELAY_MS = 100;

  for (uint8_t attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
    M5StackPM1DeviceInfo device_info{};
    if (this->read_bytes(REG_DEVICE_ID, reinterpret_cast<uint8_t *>(&device_info), sizeof(device_info))) {
      if (device_info.device_id == M5STACK_PM1_DEVICE_ID && device_info.device_model == M5STACK_PM1_DEVICE_MODEL) {
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

void M5StackPM1Component::loop() {
  // The shared expander base refreshes the pin cache and stops the loop once the IRQ line is released.
  gpio_expander::CachedGpioExpanderComponent<uint8_t, 8>::loop();
  if (this->interrupt_pin_ == nullptr) {
    return;
  }
  if (!this->process_irq_()) {
    // Keep looping so the pending IRQ status is read and cleared on a later pass.
    this->enable_loop();
  }
}

bool M5StackPM1Component::process_irq_() {
  M5StackPM1IRQStatus status{};
  if (!this->status_comm_check(this->read_irq_status_(&status))) {
    return false;
  }

  if ((status.system & M5STACK_PM1_IRQ_SYSTEM_WAKEUP) != 0) {
    // WAKE_SRC holds the wakeup status bit active, so clear it before IRQ Status 3.
    if (!this->status_comm_check(this->write_byte(REG_WAKE_SRC, 0))) {
      return false;
    }
  }

  if (!this->status_comm_result(this->clear_irq_status_())) {
    return false;
  }

  // Invalidate once so all GPIO consumers share the next refreshed input value.
  this->reset_pin_cache_();
  return true;
}

void M5StackPM1Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5STACK_PM1:\n"
                "  Device ID: 0x%02X\n"
                "  Device Model: 0x%02X\n"
                "  Hardware Revision: %u\n"
                "  Software Revision: %u\n"
                "  I2C Speed: %s",
                this->device_info_.device_id, this->device_info_.device_model, this->device_info_.hardware_revision,
                this->device_info_.software_revision,
                this->bus_frequency_ >= 400000 ? LOG_STR_LITERAL("400kHz") : LOG_STR_LITERAL("100kHz"));
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  if (this->interrupt_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  M5Stack PM1 IRQ GPIO: %u", this->interrupt_gpio_);
  }
}

bool M5StackPM1Component::configure_i2c_speed_() {
  return this->write_byte(REG_I2C_CFG, this->bus_frequency_ >= 400000 ? M5STACK_PM1_I2C_CFG_SPD : 0);
}

void M5StackPM1Component::set_gpio_interrupt_enabled(uint8_t pin, bool enabled) {
  if (!this->valid_pin_(pin)) {
    return;
  }
  // A set mask bit disables the interrupt
  assign_bits(this->irq_masks_.gpio, 1 << pin, !enabled);
}

bool M5StackPM1Component::read_initial_state_() {
  return this->read_byte(REG_GPIO_OUT, &this->output_mask_) && this->read_byte(REG_PWR_CFG, &this->power_config_);
}

bool M5StackPM1Component::write_pin_field_(uint8_t pin, uint8_t reg0, uint8_t reg1, uint8_t value) {
  if (!this->valid_pin_(pin)) {
    return false;
  }
  const uint8_t reg = pin < 4 ? reg0 : reg1;
  const uint8_t shift = pin < 4 ? pin * 2 : 0;
  return this->update_register(reg, 0b11 << shift, value << shift);
}

bool M5StackPM1Component::set_pin_function(uint8_t pin, M5StackPM1PinFunction function) {
  return this->write_pin_field_(pin, REG_GPIO_FUNC_0, REG_GPIO_FUNC_1, static_cast<uint8_t>(function));
}

void M5StackPM1Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (!this->valid_pin_(pin) || pin == this->interrupt_gpio_) {
    ESP_LOGE(TAG, "GPIO%u is invalid or reserved for IRQ", pin);
    return;
  }

  // 0x10-0x17 is one window; 0x12 is read only, so the write starts at the drive register.
  M5StackPM1GPIORegs regs{};
  if (!this->read_bytes(REG_GPIO_MODE, reinterpret_cast<uint8_t *>(&regs), sizeof(regs))) {
    this->status_comm_result(false);
    return;
  }

  const bool output = (flags & gpio::FLAG_OUTPUT) != 0;
  const uint8_t pin_mask = 1 << pin;
  const uint8_t shift = (pin < 4 ? pin : 0) * 2;
  const uint8_t field_mask = 0b11 << shift;
  uint8_t &pull = pin < 4 ? regs.pull_0 : regs.pull_1;
  uint8_t &function = pin < 4 ? regs.function_0 : regs.function_1;

  M5StackPM1PullMode pull_mode = M5StackPM1PullMode::M5STACK_PM1_PULL_MODE_NONE;
  if (flags & gpio::FLAG_PULLUP) {
    pull_mode = M5StackPM1PullMode::M5STACK_PM1_PULL_MODE_UP;
  } else if (flags & gpio::FLAG_PULLDOWN) {
    pull_mode = M5StackPM1PullMode::M5STACK_PM1_PULL_MODE_DOWN;
  }

  assign_bits(regs.mode, pin_mask, output);
  pull = (pull & ~field_mask) | (static_cast<uint8_t>(pull_mode) << shift);
  function =
      (function & ~field_mask) | (static_cast<uint8_t>(M5StackPM1PinFunction::M5STACK_PM1_PIN_FUNCTION_GPIO) << shift);
  if (output) {
    assign_bits(regs.drive, pin_mask, (flags & gpio::FLAG_OPEN_DRAIN) != 0);
  }

  // The mode takes effect only while the pin function is standard GPIO, so clear that first.
  bool success = this->write_bytes(REG_GPIO_DRV, &regs.drive, 5);
  success &= this->write_byte(REG_GPIO_MODE, regs.mode);
  if (!output) {
    this->enable_polling_loop_();
    if ((this->wakeup_mask_ & pin_mask) != 0) {
      success &= this->apply_pin_wakeup_(pin);
    }
  }
  this->status_comm_result(success);
}

void M5StackPM1Component::set_pin_wakeup(uint8_t pin, M5StackPM1WakeupEdge edge) {
  if (!this->valid_pin_(pin) || pin == 1) {
    return;
  }
  const uint8_t mask = 1 << pin;
  this->wakeup_mask_ |= mask;
  assign_bits(this->wakeup_edge_mask_, mask, edge == M5StackPM1WakeupEdge::M5STACK_PM1_WAKEUP_EDGE_RISING);
}

bool M5StackPM1Component::apply_pin_wakeup_(uint8_t pin) {
  const uint8_t mask = 1 << pin;
  uint8_t wake_enable;
  if (!this->read_byte(REG_GPIO_WAKE_EN, &wake_enable)) {
    return false;
  }
  const uint8_t conflicting_mask = pin == 0 ? 1 << 2 : pin == 2 ? 1 << 0 : pin == 3 ? 1 << 4 : 1 << 3;
  if (wake_enable & conflicting_mask) {
    ESP_LOGE(TAG, "GPIO%u conflicts with an enabled wake source", pin);
    return false;
  }
  if (!this->update_register(REG_GPIO_WAKE_CFG, mask, this->wakeup_edge_mask_ & mask)) {
    return false;
  }
  return this->update_register(REG_GPIO_WAKE_EN, mask, mask);
}

bool M5StackPM1Component::digital_read_hw(uint8_t pin) {
  if (!this->valid_pin_(pin)) {
    return false;
  }
  return this->status_comm_result(this->read_byte(REG_GPIO_IN, &this->input_[0]));
}

void M5StackPM1Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed() || !this->valid_pin_(pin) || pin == this->interrupt_gpio_) {
    return;
  }
  assign_bits(this->output_mask_, 1 << pin, value);
  this->status_comm_result(this->write_byte(REG_GPIO_OUT, this->output_mask_));
}

bool M5StackPM1Component::set_power_config_(uint8_t mask, bool enable) {
  mask &= 0x1F;
  const uint8_t value = enable ? (this->power_config_ | mask) : (this->power_config_ & ~mask);
  if (value == this->power_config_) {
    return true;
  }
  if (!this->status_comm_result(this->write_byte(REG_PWR_CFG, value))) {
    return false;
  }
  this->power_config_ = value;
  return true;
}

bool M5StackPM1Component::read_irq_status_(M5StackPM1IRQStatus *status) {
  uint8_t data[3];
  if (!this->read_bytes(REG_IRQ_STAT_1, data, sizeof(data))) {
    return false;
  }
  status->gpio = data[0] & 0x1F;
  status->power = data[1] & 0x3F;
  status->system = data[2] & 0x07;
  return true;
}

bool M5StackPM1Component::clear_irq_status_() {
  const uint8_t clear_data[3] = {0, 0, 0};
  return this->write_bytes(REG_IRQ_STAT_1, clear_data, sizeof(clear_data));
}

bool M5StackPM1Component::set_irq_masks_(const M5StackPM1IRQStatus &masks) {
  const uint8_t data[3] = {uint8_t(masks.gpio & 0x1F), uint8_t(masks.power & 0x3F), uint8_t(masks.system & 0x07)};
  return this->write_bytes(REG_IRQ_STAT_1_MASK, data, sizeof(data));
}

}  // namespace esphome::m5stack_pm1

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::m5stack_pm1::M5StackPM1Component, "%u via M5STACK_PM1");
