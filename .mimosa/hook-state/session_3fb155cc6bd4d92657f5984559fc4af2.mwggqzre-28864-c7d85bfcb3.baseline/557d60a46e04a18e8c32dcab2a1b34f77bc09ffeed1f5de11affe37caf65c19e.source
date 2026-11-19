#ifdef USE_RP2

#include "gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rp2 {

ESPHOME_LOG_TAG(TAG, "rp2");

static int flags_to_mode(gpio::Flags flags, uint8_t pin) {
  flags =
      flags & (gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN | gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN);
  if (flags == gpio::FLAG_INPUT) {  // NOLINT(bugprone-branch-clone)
    return INPUT;
  } else if (flags == gpio::FLAG_OUTPUT) {
    return OUTPUT;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    return INPUT_PULLUP;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN)) {
    return INPUT_PULLDOWN;
    // } else if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
    //   return OpenDrain;
  } else {
    return 0;
  }
}

ISRInternalGPIOPin RP2GPIOPin::to_isr() const { return ISRInternalGPIOPin(this); }

void RP2GPIOPin::attach_interrupt_(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  PinStatus arduino_mode = LOW;
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
      arduino_mode = inverted_ ? HIGH : LOW;
      break;
    case gpio::INTERRUPT_HIGH_LEVEL:
      arduino_mode = inverted_ ? LOW : HIGH;
      break;
  }

  attachInterrupt(pin_, func, arduino_mode, arg);
}
void RP2GPIOPin::pin_mode(gpio::Flags flags) {
  pinMode(pin_, flags_to_mode(flags, pin_));  // NOLINT
}

size_t RP2GPIOPin::dump_summary(char *buffer, size_t len) const { return snprintf(buffer, len, "GPIO%u", this->pin_); }

bool RP2GPIOPin::digital_read() {
  return bool(digitalRead(pin_)) != inverted_;  // NOLINT
}
void RP2GPIOPin::digital_write(bool value) {
  digitalWrite(pin_, value != inverted_ ? 1 : 0);  // NOLINT
}
void RP2GPIOPin::detach_interrupt() const { detachInterrupt(pin_); }

}  // namespace rp2

using namespace rp2;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *pin = static_cast<RP2GPIOPin *>(this->arg_);
  return bool(sio_hw->gpio_in & (1u << pin->get_pin())) != pin->is_inverted();
}

void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto *pin = static_cast<RP2GPIOPin *>(this->arg_);
  const uint32_t mask = 1u << pin->get_pin();
  if (value != pin->is_inverted()) {
    sio_hw->gpio_set = mask;
  } else {
    sio_hw->gpio_clr = mask;
  }
}

void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  // TODO: implement
}

void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  const uint8_t num = static_cast<RP2GPIOPin *>(this->arg_)->get_pin();
  if (flags & gpio::FLAG_OUTPUT) {
    sio_hw->gpio_oe_set = 1u << num;
  } else if (flags & gpio::FLAG_INPUT) {
    sio_hw->gpio_oe_clr = 1u << num;
    hw_write_masked(&pads_bank0_hw->io[num],
                    (bool_to_bit(flags & gpio::FLAG_PULLUP) << PADS_BANK0_GPIO0_PUE_LSB) |
                        (bool_to_bit(flags & gpio::FLAG_PULLDOWN) << PADS_BANK0_GPIO0_PDE_LSB),
                    PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_PDE_BITS);
  }
}

}  // namespace esphome

#endif  // USE_RP2
