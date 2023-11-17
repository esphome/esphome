#ifdef USE_LIBRETINY

#include "gpio_arduino.h"
#include "esphome/core/log.h"

namespace esphome {
namespace libretiny {

static const char *const TAG = "lt.gpio";

static int IRAM_ATTR flags_to_mode(gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    return INPUT;
  } else if (flags == gpio::FLAG_OUTPUT) {
    return OUTPUT;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    return INPUT_PULLUP;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN)) {
    return INPUT_PULLDOWN;
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

ISRInternalGPIOPin ArduinoInternalGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void ArduinoInternalGPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  PinStatus arduino_mode = (PinStatus) 255;
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

  attachInterruptParam(pin_, func, arduino_mode, arg);
}

void ArduinoInternalGPIOPin::pin_mode(gpio::Flags flags) {
  pinMode(pin_, flags_to_mode(flags));  // NOLINT
}

std::string ArduinoInternalGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u", pin_);
  return buffer;
}

bool ArduinoInternalGPIOPin::digital_read() {
  return bool(digitalRead(pin_)) ^ inverted_;  // NOLINT
}
void ArduinoInternalGPIOPin::digital_write(bool value) {
  digitalWrite(pin_, value ^ inverted_);  // NOLINT
}
void ArduinoInternalGPIOPin::detach_interrupt() const {
  detachInterrupt(pin_);  // NOLINT
}

}  // namespace libretiny

using namespace libretiny;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  return bool(digitalRead(arg->pin)) ^ arg->inverted;  // NOLINT
}
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  digitalWrite(arg->pin, value ^ arg->inverted);  // NOLINT
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  detachInterrupt(arg->pin);
}
void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  pinMode(arg->pin, flags_to_mode(flags));  // NOLINT
}

}  // namespace esphome

#endif  // USE_LIBRETINY
