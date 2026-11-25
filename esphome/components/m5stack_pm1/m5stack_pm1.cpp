#include "m5stack_pm1.h"

#include <cstddef>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5stack_pm1 {

ESPHOME_LOG_TAG(TAG, "m5stack_pm1");

// The writable part of the GPIO window starts at the drive register; 0x12 is read only.
static constexpr size_t GPIO_CONFIG_OFFSET = offsetof(M5StackPM1GPIORegs, drive);
static constexpr size_t GPIO_CONFIG_LEN = sizeof(M5StackPM1GPIORegs) - GPIO_CONFIG_OFFSET;

/// Set a pin's 2-bit field in a register pair: pins 0-3 use the first register, pin 4 the second.
static constexpr void set_pin_field(uint8_t (&fields)[2], uint8_t pin, uint8_t value) {
  const uint8_t shift = (pin & 0b11) * 2;
  uint8_t &field = fields[pin >> 2];
  field = (field & ~(0b11 << shift)) | (value << shift);
}

/// The controller may still be starting after the power cycle, so retry the first transactions.
template<typename F> static bool retry_while_starting(F &&attempt) {
  static constexpr uint8_t MAX_ATTEMPTS = 6;
  static constexpr uint32_t RETRY_DELAY_MS = 100;
  for (uint8_t i = 1;; i++) {
    if (attempt())
      return true;
    if (i == MAX_ATTEMPTS)
      return false;
    delay(RETRY_DELAY_MS);  // NOLINT
  }
}

void M5StackPM1Component::setup() {
  // The M5Stack PM1 reset/download sequence powers the rails off for 500 ms before turning them on again.
  // Give that sequence time to finish before the first I2C transaction, then tolerate a slow firmware start.
  delay(500);  // NOLINT
  // The controller starts in the 100 kHz mode; SPD selects the 400 kHz mode.
  const uint8_t speed = this->fast_mode_ ? M5STACK_PM1_I2C_CFG_SPD : 0;
  if (!this->setup_check(retry_while_starting([this, speed]() { return this->write_byte(REG_I2C_CFG, speed); }),
                         LOG_STR("Unable to configure I2C speed")) ||
      !this->setup_check(this->read_device_info_(), LOG_STR("Unable to read device information")) ||
      !this->setup_check(
          this->read_bytes(REG_GPIO_MODE, reinterpret_cast<uint8_t *>(&this->gpio_regs_), sizeof(this->gpio_regs_)),
          LOG_STR("Unable to read initial state")) ||
      !this->setup_check(this->update_register(REG_PWR_CFG, M5STACK_PM1_POWER_CONFIG_LED, M5STACK_PM1_POWER_CONFIG_LED),
                         LOG_STR("Unable to enable status LED")) ||
      !this->setup_check(this->write_byte(REG_GPIO_WAKE_EN, 0), LOG_STR("Unable to clear GPIO wake configuration"))) {
    return;
  }
  if (this->wake_enable_ != 0) {
    // Once every component has set up its pins: the edge first, then the enable
    this->defer([this]() { this->apply_wake_(); });
  }

  if (this->interrupt_pin_ != nullptr) {
    // The IRQ output is open drain and its own GPIO interrupt stays masked.
    const uint8_t masks[3] = {this->irq_gpio_mask_, M5STACK_PM1_IRQ_POWER_MASK, M5STACK_PM1_IRQ_SYSTEM_MASK};
    this->gpio_regs_.drive |= 1 << this->interrupt_gpio_;
    set_pin_field(this->gpio_regs_.function, this->interrupt_gpio_,
                  static_cast<uint8_t>(M5StackPM1PinFunction::M5STACK_PM1_PIN_FUNCTION_IRQ));
    if (!this->setup_check(this->write_bytes(REG_IRQ_STAT_1_MASK, masks, sizeof(masks)) &&
                               this->write_bytes(REG_GPIO_DRV, &this->gpio_regs_.drive, GPIO_CONFIG_LEN),
                           LOG_STR("Unable to configure IRQ output"))) {
      return;
    }
  }
  // Attach the IRQ line last: it also decides whether the loop starts enabled.
  this->setup_interrupt_pin_();
}

bool M5StackPM1Component::read_device_info_() {
  return retry_while_starting([this]() {
    uint8_t info[4];
    if (!this->read_bytes(REG_DEVICE_ID, info, sizeof(info)))
      return false;
    if (info[0] != M5STACK_PM1_DEVICE_ID || info[1] != M5STACK_PM1_DEVICE_MODEL) {
      ESP_LOGW(TAG, "Unexpected device ID/model: 0x%02X/0x%02X", info[0], info[1]);
      return false;
    }
    this->hardware_revision_ = info[2];
    this->software_revision_ = info[3];
    return true;
  });
}

void M5StackPM1Component::loop() {
  // Clear the IRQ first, so the base class sees the released line, refreshes the pin cache and stops the loop.
  const bool handled = this->interrupt_pin_ == nullptr || this->process_irq_();
  gpio_expander::CachedGpioExpanderComponent<uint8_t, 8>::loop();
  if (!handled) {
    // Keep looping so the pending IRQ status is read and cleared on a later pass.
    this->enable_loop();
  }
}

bool M5StackPM1Component::process_irq_() {
  uint8_t status[3];
  if (!this->status_comm_check(this->read_bytes(REG_IRQ_STAT_1, status, sizeof(status)))) {
    return false;
  }
  // WAKE_SRC holds the wakeup status bit active, so clear it before IRQ Status 3.
  if ((status[2] & M5STACK_PM1_IRQ_SYSTEM_WAKEUP) != 0 && !this->status_comm_check(this->write_byte(REG_WAKE_SRC, 0))) {
    return false;
  }
  static constexpr uint8_t CLEAR[3] = {0, 0, 0};
  return this->status_comm_result(this->write_bytes(REG_IRQ_STAT_1, CLEAR, sizeof(CLEAR)));
}

void M5StackPM1Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5Stack PM1:\n"
                "  Hardware Revision: %u\n"
                "  Software Revision: %u\n"
                "  Fast Mode: %s",
                this->hardware_revision_, this->software_revision_,
                this->fast_mode_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  if (this->interrupt_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  IRQ GPIO: %u", this->interrupt_gpio_);
  }
}

void M5StackPM1Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (this->is_failed()) {
    return;
  }
  this->pin_flags_[pin] = flags;
  this->apply_pin_config_(pin, flags);
}

void M5StackPM1Component::apply_pin_config_(uint8_t pin, gpio::Flags flags) {
  M5StackPM1GPIORegs &regs = this->gpio_regs_;
  const bool output = (flags & gpio::FLAG_OUTPUT) != 0;
  const uint8_t pin_mask = 1 << pin;

  M5StackPM1PullMode pull_mode = M5StackPM1PullMode::M5STACK_PM1_PULL_MODE_NONE;
  if (flags & gpio::FLAG_PULLUP) {
    pull_mode = M5StackPM1PullMode::M5STACK_PM1_PULL_MODE_UP;
  } else if (flags & gpio::FLAG_PULLDOWN) {
    pull_mode = M5StackPM1PullMode::M5STACK_PM1_PULL_MODE_DOWN;
  }

  assign_bits(regs.mode, pin_mask, output);
  set_pin_field(regs.pull, pin, static_cast<uint8_t>(pull_mode));
  set_pin_field(regs.function, pin, static_cast<uint8_t>(M5StackPM1PinFunction::M5STACK_PM1_PIN_FUNCTION_GPIO));
  if (output) {
    assign_bits(regs.drive, pin_mask, (flags & gpio::FLAG_OPEN_DRAIN) != 0);
  }

  // The mode takes effect only while the pin function is standard GPIO, so write that first.
  bool success = this->write_bytes(REG_GPIO_DRV, &regs.drive, GPIO_CONFIG_LEN);
  success &= this->write_byte(REG_GPIO_MODE, regs.mode);
  if (!output) {
    this->enable_polling_loop_();
  }
  this->report_config_(pin, success);
}

void M5StackPM1Component::apply_wake_() {
  this->report_config_(WAKE_PENDING_BIT, this->write_byte(REG_GPIO_WAKE_CFG, this->wake_rising_) &&
                                             this->write_byte(REG_GPIO_WAKE_EN, this->wake_enable_));
}

void M5StackPM1Component::report_config_(uint8_t bit, bool success) {
  this->set_pin_config_pending_(bit, !success);
  // Keep the warning while anything is still waiting for its configuration.
  if (this->pending_pin_config_mask_ != 0) {
    this->status_comm_check(success);
  } else {
    this->status_comm_result(success);
  }
}

void M5StackPM1Component::set_pin_config_pending_(uint8_t pin, bool pending) {
  if (pending) {
    if (this->pending_pin_config_mask_ == 0) {
      this->set_interval(PIN_CONFIG_RETRY_ID, PIN_CONFIG_RETRY_INTERVAL_MS, [this]() { this->retry_pin_config_(); });
    }
    this->pending_pin_config_mask_ |= 1 << pin;
    return;
  }
  this->pending_pin_config_mask_ &= ~(1 << pin);
  if (this->pending_pin_config_mask_ == 0) {
    this->cancel_interval(PIN_CONFIG_RETRY_ID);
  }
}

void M5StackPM1Component::retry_pin_config_() {
  if (this->is_failed()) {
    this->pending_pin_config_mask_ = 0;
    this->cancel_interval(PIN_CONFIG_RETRY_ID);
    return;
  }
  for (uint8_t pin = 0; pin < M5STACK_PM1_GPIO_COUNT; pin++) {
    if ((this->pending_pin_config_mask_ & (1 << pin)) != 0) {
      this->apply_pin_config_(pin, this->pin_flags_[pin]);
    }
  }
  if ((this->pending_pin_config_mask_ & (1 << WAKE_PENDING_BIT)) != 0)
    this->apply_wake_();
}

bool M5StackPM1Component::digital_read_hw(uint8_t pin) {
  if (this->is_failed()) {
    return false;
  }
  const bool ok = this->read_byte(REG_GPIO_IN, &this->input_[0]);
  // A successful read must not hide a pin that is still waiting for its configuration.
  if (this->pending_pin_config_mask_ != 0) {
    return this->status_comm_check(ok);
  }
  return this->status_comm_result(ok);
}

void M5StackPM1Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed()) {
    return;
  }
  assign_bits(this->gpio_regs_.output, 1 << pin, value);
  this->status_comm_result(this->write_byte(REG_GPIO_OUT, this->gpio_regs_.output));
}

}  // namespace esphome::m5stack_pm1

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::m5stack_pm1::M5StackPM1Component, "%u via M5STACK_PM1");
