#ifdef USE_RP2040

#include "gpio.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rp2040 {

static const char *const TAG = "rp2040";

static int flags_to_mode(gpio::Flags flags, uint8_t pin) {
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

struct ISRPinArg {
  uint32_t mask;
  uint8_t pin;
  bool inverted;
};

ISRInternalGPIOPin RP2040GPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = this->pin_;
  arg->inverted = this->inverted_;
  arg->mask = 1 << this->pin_;
  return ISRInternalGPIOPin((void *) arg);
}

void RP2040GPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
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
void RP2040GPIOPin::pin_mode(gpio::Flags flags) {
  pinMode(pin_, flags_to_mode(flags, pin_));  // NOLINT
}

std::string RP2040GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", pin_);
  return buffer;
}

bool RP2040GPIOPin::digital_read() {
  return bool(digitalRead(pin_)) != inverted_;  // NOLINT
}
void RP2040GPIOPin::digital_write(bool value) {
  digitalWrite(pin_, value != inverted_ ? 1 : 0);  // NOLINT
}
void RP2040GPIOPin::detach_interrupt() const { detachInterrupt(pin_); }

}  // namespace rp2040

using namespace rp2040;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(this->arg_);
  return bool(sio_hw->gpio_in & arg->mask) != arg->inverted;
}

void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto *arg = reinterpret_cast<ISRPinArg *>(this->arg_);
  if (value != arg->inverted) {
    sio_hw->gpio_set = arg->mask;
  } else {
    sio_hw->gpio_clr = arg->mask;
  }
}

void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  // TODO: implement
  // auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  // GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1UL << arg->pin);
}

void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto *arg = reinterpret_cast<ISRPinArg *>(this->arg_);
  if (flags & gpio::FLAG_OUTPUT) {
    sio_hw->gpio_oe_set = arg->mask;
  } else if (flags & gpio::FLAG_INPUT) {
    sio_hw->gpio_oe_clr = arg->mask;
    hw_write_masked(&padsbank0_hw->io[arg->pin],
                    (bool_to_bit(flags & gpio::FLAG_PULLUP) << PADS_BANK0_GPIO0_PUE_LSB) |
                        (bool_to_bit(flags & gpio::FLAG_PULLDOWN) << PADS_BANK0_GPIO0_PDE_LSB),
                    PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_PDE_BITS);
  }
}

}  // namespace esphome

#endif  // USE_RP2040
