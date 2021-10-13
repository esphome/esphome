#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "gpio_arduino.h"
#include "esphome/core/log.h"
#include <esp32-hal-gpio.h>

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32";

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
  uint8_t arduino_mode = DISABLED;
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
void ArduinoInternalGPIOPin::pin_mode(gpio::Flags flags) {
  uint8_t mode;
  if (flags == gpio::FLAG_INPUT) {
    mode = INPUT;
  } else if (flags == gpio::FLAG_OUTPUT) {
    mode = OUTPUT;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    mode = INPUT_PULLUP;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN)) {
    mode = INPUT_PULLDOWN;
  } else if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
    mode = OUTPUT_OPEN_DRAIN;
  } else {
    return;
  }
  pinMode(pin_, mode);  // NOLINT
}

std::string ArduinoInternalGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", pin_);
  return buffer;
}

bool ArduinoInternalGPIOPin::digital_read() {
  return bool(digitalRead(pin_)) != inverted_;  // NOLINT
}
void ArduinoInternalGPIOPin::digital_write(bool value) {
  digitalWrite(pin_, value != inverted_ ? 1 : 0);  // NOLINT
}
void ArduinoInternalGPIOPin::detach_interrupt() const {
  detachInterrupt(pin_);  // NOLINT
}

}  // namespace esp32

using namespace esp32;

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
#ifdef CONFIG_IDF_TARGET_ESP32C3
  GPIO.status_w1tc.val = 1UL << arg->pin;
#else
  if (arg->pin < 32) {
    GPIO.status_w1tc = 1UL << arg->pin;
  } else {
    GPIO.status1_w1tc.intr_st = 1UL << (arg->pin - 32);
  }
#endif
}

}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
