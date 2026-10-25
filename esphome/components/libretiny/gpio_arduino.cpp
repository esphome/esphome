#ifdef USE_LIBRETINY

#include "gpio_arduino.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_BK72XX
extern "C" {
#include <gpio_pub.h>
}
#endif

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

#ifdef USE_BK72XX
// Per-pin config register of the BDK GPIO driver (REG_GPIO_0_CONFIG and GCFG_OUTPUT_BIT in driver/gpio/gpio.h).
// A value of 0 enables the output, and bit 1 holds the output level.
static constexpr uintptr_t BK_GPIO_CFG_BASE = 0x00802800;
static constexpr uint32_t BK_GPIO_OUTPUT_BIT = 1 << 1;
static constexpr uint8_t BK_GPIO_DIRECT_MAX = 31;

// LibreTiny's switch to output writes 0 to the config register, so the pin always starts LOW, and its read
// turns a pin it does not track as an input back into one. Drive simple modes through the BDK instead.
static bool IRAM_ATTR bk_set_mode(uint8_t pin, gpio::Flags flags, bool level) {
  if (pin > BK_GPIO_DIRECT_MAX)
    return false;
  switch (flags &
          (gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN | gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN)) {
    case gpio::FLAG_OUTPUT: {
      gpio_output(pin, level);  // set the level while the pin is still an input
      auto *cfg =
          reinterpret_cast<volatile uint32_t *>(BK_GPIO_CFG_BASE + pin * 4);  // NOLINT(performance-no-int-to-ptr)
      *cfg &= BK_GPIO_OUTPUT_BIT;  // then enable the output in one write that keeps it
      return true;
    }
    case gpio::FLAG_INPUT:
      gpio_config(pin, GMODE_INPUT);
      return true;
    case gpio::FLAG_INPUT | gpio::FLAG_PULLUP:
      gpio_config(pin, GMODE_INPUT_PULLUP);
      return true;
    case gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN:
      gpio_config(pin, GMODE_INPUT_PULLDOWN);
      return true;
    default:
      return false;
  }
}
#endif

void IRAM_ATTR ArduinoInternalGPIOPin::set_mode_(gpio::Flags flags) {
  this->output_ = flags & gpio::FLAG_OUTPUT;
#ifdef USE_BK72XX
  this->direct_ = bk_set_mode(this->pin_, flags, this->level_);
  if (this->direct_)
    return;
#endif
  pinMode(this->pin_, flags_to_mode(flags));  // NOLINT
  if (this->output_)
    digitalWrite(this->pin_, this->level_);  // NOLINT
}
void IRAM_ATTR ArduinoInternalGPIOPin::write_(bool level) {
  this->level_ = level;
  if (!this->output_)
    return;
#ifdef USE_BK72XX
  if (this->direct_) {
    gpio_output(this->pin_, level);
    return;
  }
#endif
  digitalWrite(this->pin_, level);  // NOLINT
}
bool IRAM_ATTR ArduinoInternalGPIOPin::read_() {
#ifdef USE_BK72XX
  if (this->direct_)
    return gpio_input(this->pin_);
#endif
  return digitalRead(this->pin_);  // NOLINT
}
void ArduinoInternalGPIOPin::pin_mode(gpio::Flags flags) { this->set_mode_(flags); }

size_t ArduinoInternalGPIOPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len, "%u", this->pin_);
}

bool ArduinoInternalGPIOPin::digital_read() { return this->read_() ^ this->inverted_; }
void ArduinoInternalGPIOPin::digital_write(bool value) { this->write_(value ^ this->inverted_); }
void ArduinoInternalGPIOPin::detach_interrupt() const {
  detachInterrupt(pin_);  // NOLINT
}

}  // namespace esphome::libretiny

namespace esphome {

using namespace libretiny;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  return arg->parent->read_() ^ arg->inverted;
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
