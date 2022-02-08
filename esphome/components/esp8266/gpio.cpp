#ifdef USE_ESP8266

#include "gpio.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp8266 {

static const char *const TAG = "esp8266";

static int IRAM_ATTR flags_to_mode(gpio::Flags flags, uint8_t pin) {
  if (flags == gpio::FLAG_INPUT) {  // NOLINT(bugprone-branch-clone)
    return INPUT;
  } else if (flags == gpio::FLAG_OUTPUT) {
    return OUTPUT;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    if (pin == 16) {
      // GPIO16 doesn't have a pullup, so pinMode would fail.
      // However, sometimes this method is called with pullup mode anyway
      // for example from dallas one_wire. For those cases convert this
      // to a INPUT mode.
      return INPUT;
    }
    return INPUT_PULLUP;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN)) {
    return INPUT_PULLDOWN_16;
  } else if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
    return OUTPUT_OPEN_DRAIN;
  } else {
    return 0;
  }
}

struct ISRPinArg {
  uint8_t pin;
  bool inverted;
};

ISRInternalGPIOPin ESP8266GPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void ESP8266GPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
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

std::string ESP8266GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", pin_);
  return buffer;
}

bool ESP8266GPIOPin::digital_read() {
  return bool(digitalRead(pin_)) != inverted_;  // NOLINT
}
void ESP8266GPIOPin::digital_write(bool value) {
  digitalWrite(pin_, value != inverted_ ? 1 : 0);  // NOLINT
}
void ESP8266GPIOPin::detach_interrupt() const { detachInterrupt(pin_); }

}  // namespace esp8266

using namespace esp8266;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  return bool(digitalRead(arg->pin)) != arg->inverted;  // NOLINT
}
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  digitalWrite(arg->pin, value != arg->inverted ? 1 : 0);  // NOLINT
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1UL << arg->pin);
}
void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  pinMode(arg->pin, flags_to_mode(flags, arg->pin));  // NOLINT
}

}  // namespace esphome

#endif  // USE_ESP8266
