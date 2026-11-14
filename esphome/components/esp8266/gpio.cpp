#ifdef USE_ESP8266

#include "gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::esp8266 {

ESPHOME_LOG_TAG(TAG, "esp8266");

static int flags_to_mode(gpio::Flags flags, uint8_t pin) {
  flags =
      flags & (gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN | gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN);
  if (flags == gpio::FLAG_OUTPUT || flags == (gpio::FLAG_OUTPUT | gpio::FLAG_INPUT)) {
    return OUTPUT;
  }
  if (flags == gpio::FLAG_INPUT) {
    return INPUT;
  }
  if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    if (pin == 16) {
      // GPIO16 doesn't have a pullup, so pinMode would fail.
      // However, sometimes this method is called with pullup mode anyway
      // for example from dallas one_wire. For those cases convert this
      // to a INPUT mode.
      return INPUT;
    }
    return INPUT_PULLUP;
  }
  if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN)) {
    return INPUT_PULLDOWN_16;
  }
  if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
    return OUTPUT_OPEN_DRAIN;
  }
  return INPUT;
}

// Slot of each pin's GPF register (0x804 + 4 * slot), so ISR code does not read the core's PROGMEM table.
static constexpr uint32_t GPF_SLOTS_0_7 = 0x76fe4d5c;
static constexpr uint32_t GPF_SLOTS_8_15 = 0x3210ba98;
static inline volatile uint32_t &IRAM_ATTR isr_gpf(uint8_t num) {
  const uint32_t slots = num < 8 ? GPF_SLOTS_0_7 : GPF_SLOTS_8_15;
  return ESP8266_REG(0x804 + 4 * ((slots >> (4 * (num & 7))) & 0xF));
}

ISRInternalGPIOPin ESP8266GPIOPin::to_isr() const { return ISRInternalGPIOPin(this); }

void ESP8266GPIOPin::attach_interrupt_(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  uint8_t arduino_mode = 0;
  switch (type) {
    case gpio::INTERRUPT_RISING_EDGE:
      arduino_mode = inverted_ ? FALLING : RISING;
      break;
    case gpio::INTERRUPT_FALLING_EDGE:
      arduino_mode = inverted_ ? RISING : FALLING;
      break;
    case gpio::INTERRUPT_ANY_EDGE:
      arduino_mode = CHANGE;
      break;
    case gpio::INTERRUPT_LOW_LEVEL:
      arduino_mode = inverted_ ? ONHIGH : ONLOW;
      break;
    case gpio::INTERRUPT_HIGH_LEVEL:
      arduino_mode = inverted_ ? ONLOW : ONHIGH;
      break;
  }

  attachInterruptArg(pin_, func, arg, arduino_mode);
}
void ESP8266GPIOPin::pin_mode(gpio::Flags flags) {
  pinMode(pin_, flags_to_mode(flags, pin_));  // NOLINT
}

size_t ESP8266GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "GPIO%u", this->pin_);
}

bool ESP8266GPIOPin::digital_read() {
  return bool(digitalRead(pin_)) != inverted_;  // NOLINT
}
void ESP8266GPIOPin::digital_write(bool value) {
  digitalWrite(pin_, value != inverted_ ? 1 : 0);  // NOLINT
}
void ESP8266GPIOPin::detach_interrupt() const { detachInterrupt(pin_); }

}  // namespace esphome::esp8266

namespace esphome {

using esp8266::ESP8266GPIOPin;
using esp8266::isr_gpf;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *pin = static_cast<ESP8266GPIOPin *>(this->arg_);
  const uint8_t num = pin->get_pin();
  return bool(num < 16 ? GPI & (1 << num) : GP16I & 1) != pin->is_inverted();
}

void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto *pin = static_cast<ESP8266GPIOPin *>(this->arg_);
  const uint8_t num = pin->get_pin();
  if (num < 16) {
    if (value != pin->is_inverted()) {
      GPOS = 1 << num;
    } else {
      GPOC = 1 << num;
    }
  } else {
    GP16O = value != pin->is_inverted() ? GP16O | 1 : GP16O & ~1;
  }
}

void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  auto *pin = static_cast<ESP8266GPIOPin *>(this->arg_);
  // NOLINTNEXTLINE(clang-analyzer-core.FixedAddressDereference) -- GPIO_REG_WRITE is MMIO at a fixed address
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1UL << pin->get_pin());
}

void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  const uint8_t num = static_cast<ESP8266GPIOPin *>(this->arg_)->get_pin();
  if (num < 16) {
    if (flags & gpio::FLAG_OUTPUT) {
      GPES = 1 << num;
      if (flags & gpio::FLAG_OPEN_DRAIN) {
        GPC(num) = GPC(num) | (1 << GPCD);
      } else {
        GPC(num) &= ~(1 << GPCD);
      }
    } else if (flags & gpio::FLAG_INPUT) {
      GPEC = 1 << num;
    }
    if (flags & gpio::FLAG_PULLUP) {
      isr_gpf(num) = isr_gpf(num) | (1 << GPFPU);
      GPC(num) = GPC(num) | (1 << GPCD);
    } else {
      isr_gpf(num) = isr_gpf(num) & ~(1 << GPFPU);
    }
  } else {
    GP16E = flags & gpio::FLAG_OUTPUT ? GP16E | 1 : GP16E & ~1;
    GPF16 = flags & gpio::FLAG_PULLDOWN ? GPF16 | (1 << GP16FPD) : GPF16 & ~(1 << GP16FPD);
  }
}

}  // namespace esphome

#endif  // USE_ESP8266
