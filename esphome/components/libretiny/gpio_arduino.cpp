#ifdef USE_LIBRETINY

#include "gpio_arduino.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

// clang-tidy cannot parse the Realtek SDK headers behind wiring_data.h
#if defined(USE_BK72XX) || (defined(USE_RTL87XX) && !defined(CLANG_TIDY))
#include <wiring_data.h>
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

// The ISR copy is the pin itself, so both share the stored mode and level. Pins are never freed.
ISRInternalGPIOPin ArduinoInternalGPIOPin::to_isr() const {
  return ISRInternalGPIOPin(
      const_cast<ArduinoInternalGPIOPin *>(this));  // NOLINT(cppcoreguidelines-pro-type-const-cast)
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
// BDK GPIO config register (REG_GPIO_0_CONFIG, GCFG_OUTPUT_BIT): 0 enables the output, bit 1 is the output level
static constexpr uintptr_t BK_GPIO_CFG_BASE = 0x00802800;
static constexpr uint32_t BK_GPIO_OUTPUT_BIT = 1 << 1;

// LibreTiny's switch to output always starts the pin LOW. Enable the output at its level in one store, then update
// LibreTiny's pin state the way its own switch does.
static bool bk_enter_output(uint8_t pin, bool level) {
  PinInfo *info = pinInfo(pin);
  if (info == nullptr || info->gpio > 31)
    return false;
  PinData *data = pinData(info);
  if (pinEnabled(info, PIN_GPIO) && data->gpioMode == OUTPUT)
    return false;
  pinRemoveMode(info, PIN_PWM);
  auto *cfg =
      reinterpret_cast<volatile uint32_t *>(BK_GPIO_CFG_BASE + info->gpio * 4);  // NOLINT(performance-no-int-to-ptr)
  *cfg = level ? BK_GPIO_OUTPUT_BIT : 0;
  pinEnable(info, PIN_GPIO);
  data->gpioMode = OUTPUT;
  return true;
}
#endif

#if defined(USE_RTL87XX) && !defined(CLANG_TIDY)
// The switch to output keeps the HAL data register, so write the level to it first
static void rtl_preset_level(uint8_t pin, bool level) {
  PinInfo *info = pinInfo(pin);
  if (info == nullptr)
    return;
  if (info->data == nullptr || info->data->gpio == nullptr)
    pinMode(pin, INPUT);  // NOLINT - LibreTiny creates the gpio_t on first use
  if (info->data != nullptr && info->data->gpio != nullptr)
    gpio_write(info->data->gpio, level);
}
#endif

void ArduinoInternalGPIOPin::pin_mode(gpio::Flags flags) {
  const int mode = flags_to_mode(flags);
  this->output_ = flags & gpio::FLAG_OUTPUT;
#ifdef USE_BK72XX
  if (mode == OUTPUT && bk_enter_output(this->pin_, this->level_))
    return;
#endif
#if defined(USE_RTL87XX) && !defined(CLANG_TIDY)
  // The preset avoids a LOW blip when going HIGH, but a LOW only takes after the switch (measured on a BW15),
  // so the write below is still needed
  if (mode == OUTPUT)
    rtl_preset_level(this->pin_, this->level_);
#endif
  pinMode(this->pin_, mode);  // NOLINT
  if (this->output_)
    digitalWrite(this->pin_, this->level_);  // NOLINT
}

size_t ArduinoInternalGPIOPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len, "%u", this->pin_);
}

bool ArduinoInternalGPIOPin::digital_read() {
  return bool(digitalRead(this->pin_)) ^ this->inverted_;  // NOLINT
}
void ArduinoInternalGPIOPin::digital_write(bool value) {
  this->level_ = value ^ this->inverted_;
  if (this->output_)
    digitalWrite(this->pin_, this->level_);  // NOLINT
}
void ArduinoInternalGPIOPin::detach_interrupt() const {
  detachInterrupt(pin_);  // NOLINT
}

}  // namespace esphome::libretiny

namespace esphome {

using namespace libretiny;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  return reinterpret_cast<ArduinoInternalGPIOPin *>(this->arg_)->digital_read();
}
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  reinterpret_cast<ArduinoInternalGPIOPin *>(this->arg_)->digital_write(value);
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  detachInterrupt(reinterpret_cast<ArduinoInternalGPIOPin *>(this->arg_)->get_pin());
}
void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  reinterpret_cast<ArduinoInternalGPIOPin *>(this->arg_)->pin_mode(flags);
}

}  // namespace esphome

#endif  // USE_LIBRETINY
