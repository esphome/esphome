#ifdef USE_LIBRETINY

#include "gpio_arduino.h"
#include "esphome/core/log.h"

#ifdef USE_BK72XX
// Direct GPIO register access for ISR-safe operations on Beken chips.
// The LibreTiny Arduino GPIO abstraction has guards that silently drop
// writes when the pin is not in OUTPUT mode, breaking timing-critical
// protocols like 1-Wire that set the output register before switching
// pin direction.
extern "C" {
#include <gpio_pub.h>       // gpio_output, gpio_input, gpio_config, GMODE_*
#include <wiring_custom.h>  // pinInfo() for Arduino pin -> GPIO number mapping
}
#endif

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

#ifdef USE_BK72XX
static uint32_t IRAM_ATTR flags_to_bk_mode(gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    return GMODE_INPUT;
  } else if (flags == gpio::FLAG_OUTPUT) {
    return GMODE_OUTPUT;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    return GMODE_INPUT_PULLUP;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN)) {
    return GMODE_INPUT_PULLDOWN;
  } else if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
    return GMODE_SET_HIGH_IMPENDANCE;
  } else {
    return GMODE_INPUT;
  }
}
#endif

struct ISRPinArg {
  uint8_t pin;
  bool inverted;
#ifdef USE_BK72XX
  uint32_t gpio;  // Raw Beken GPIO number for direct register access
#endif
};

ISRInternalGPIOPin ArduinoInternalGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = pin_;
  arg->inverted = inverted_;
#ifdef USE_BK72XX
  arg->gpio = pinInfo(pin_)->gpio;
#endif
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

size_t ArduinoInternalGPIOPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len, "%u", this->pin_);
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
#ifdef USE_BK72XX
  return bool(gpio_input(arg->gpio)) != arg->inverted;
#else
  return bool(digitalRead(arg->pin)) ^ arg->inverted;  // NOLINT
#endif
}
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
#ifdef USE_BK72XX
  gpio_output(arg->gpio, value != arg->inverted);
#else
  digitalWrite(arg->pin, value ^ arg->inverted);       // NOLINT
#endif
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  detachInterrupt(arg->pin);
}
void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
#ifdef USE_BK72XX
  gpio_config(arg->gpio, flags_to_bk_mode(flags));
#else
  pinMode(arg->pin, flags_to_mode(flags));             // NOLINT
#endif
}

}  // namespace esphome

#endif  // USE_LIBRETINY
