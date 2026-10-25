#ifdef USE_LIBRETINY

#include "gpio_arduino.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::libretiny {

ESPHOME_LOG_TAG(TAG, "libretiny.gpio");

static int IRAM_ATTR flags_to_mode(gpio::Flags flags) {
  flags =
      flags & (gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN | gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN);
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
  ArduinoInternalGPIOPin *parent;  // pins are never freed
};

ISRInternalGPIOPin ArduinoInternalGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = pin_;
  arg->inverted = inverted_;
  arg->parent = const_cast<ArduinoInternalGPIOPin *>(this);  // NOLINT(cppcoreguidelines-pro-type-const-cast)
  return ISRInternalGPIOPin((void *) arg);
}

void ArduinoInternalGPIOPin::attach_interrupt_(void (*func)(void *), void *arg, gpio::InterruptType type) const {
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

void IRAM_ATTR ArduinoInternalGPIOPin::set_mode_(gpio::Flags flags) {
  pinMode(this->pin_, flags_to_mode(flags));  // NOLINT
  this->output_ = flags & gpio::FLAG_OUTPUT;
  if (this->output_)
    digitalWrite(this->pin_, this->level_);  // NOLINT
}
void IRAM_ATTR ArduinoInternalGPIOPin::write_(bool level) {
  this->level_ = level;
  if (this->output_)
    digitalWrite(this->pin_, level);  // NOLINT
}
void ArduinoInternalGPIOPin::pin_mode(gpio::Flags flags) { this->set_mode_(flags); }

size_t ArduinoInternalGPIOPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len, "%u", this->pin_);
}

bool ArduinoInternalGPIOPin::digital_read() {
  return bool(digitalRead(pin_)) ^ inverted_;  // NOLINT
}
void ArduinoInternalGPIOPin::digital_write(bool value) { this->write_(value ^ this->inverted_); }
void ArduinoInternalGPIOPin::detach_interrupt() const {
  detachInterrupt(pin_);  // NOLINT
}

}  // namespace esphome::libretiny

namespace esphome {

using namespace libretiny;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  return bool(digitalRead(arg->pin)) ^ arg->inverted;  // NOLINT
}
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  arg->parent->write_(value ^ arg->inverted);
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  detachInterrupt(arg->pin);
}
void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  arg->parent->set_mode_(flags);
}

}  // namespace esphome

#endif  // USE_LIBRETINY
