#ifdef USE_LIBRETINY

#include "gpio_arduino.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

// clang-tidy cannot parse the Realtek and LN882x SDK headers behind wiring_data.h.
#if defined(USE_BK72XX) || (!defined(CLANG_TIDY) && (defined(USE_LN882X) || defined(USE_RTL87XX)))
#define LT_HAS_WIRING_DATA
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

// The ISR copy is the pin itself, so both share the stored mode and level.
ISRInternalGPIOPin ArduinoInternalGPIOPin::to_isr() const { return ISRInternalGPIOPin(this); }

#if defined(USE_LIBRETINY_VARIANT_RTL8710B) && !defined(CLANG_TIDY)
// RTL8710B GPIO interrupts on one edge only and LibreTiny maps CHANGE to rising, so flip the edge on every interrupt.
// Mirrors LibreTiny 1.14.0 wiring_irq.c (PinData is the SDK handler id); drop once libretiny-eu/libretiny#155 is fixed.
// Re-reads the level after each write; a pin still bouncing after four tries is corrected at its next edge.
static void IRAM_ATTR rtl8710b_arm_next_edge(gpio_irq_t *irq) {
  uint32_t level = GPIO_ReadDataBit(irq->pin);
  for (int i = 0; i < 4; i++) {
    // gpio_irq_set() without its flash wrapper, so the handler runs from RAM and mask ROM only
    GPIO_INTMode(irq->pin, ENABLE, GPIO_INT_Trigger_EDGE,
                 level ? GPIO_INT_POLARITY_ACTIVE_LOW : GPIO_INT_POLARITY_ACTIVE_HIGH, GPIO_INT_DEBOUNCE_ENABLE);
    const uint32_t now = GPIO_ReadDataBit(irq->pin);
    if (now == level)
      return;
    level = now;
  }
}

static void IRAM_ATTR rtl8710b_change_handler(uint32_t id, gpio_irq_event event) {
  auto *data = reinterpret_cast<PinData *>(id);  // NOLINT(performance-no-int-to-ptr)
  // A later attach with another mode keeps this handler
  if (data->irqMode == CHANGE)
    rtl8710b_arm_next_edge(data->irq);
  if (data->irqHandler != nullptr)
    reinterpret_cast<void (*)(void *)>(data->irqHandler)(data->irqParam);
}
#endif

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
#if defined(USE_LIBRETINY_VARIANT_RTL8710B) && !defined(CLANG_TIDY)
  if (arduino_mode == CHANGE) {
    PinInfo *info = pinInfo(this->pin_);
    PinData *data = info != nullptr ? info->data : nullptr;
    if (data != nullptr && data->irq != nullptr) {
      // The interrupt is already live: keep it from arming an edge between the level read and the write below
      const uint32_t primask = __get_PRIMASK();
      __disable_irq();
      GPIO_UserRegIrq(data->irq->pin, reinterpret_cast<void *>(rtl8710b_change_handler), data);
      rtl8710b_arm_next_edge(data->irq);
      __set_PRIMASK(primask);
    }
  }
#endif
}

#ifdef USE_BK72XX
// BDK GPIO config register (REG_GPIO_0_CONFIG, GCFG_OUTPUT_BIT): 0 enables the output, bit 1 is the output level
static constexpr uintptr_t BK_GPIO_CFG_BASE = 0x00802800;
static constexpr uint32_t BK_GPIO_OUTPUT_BIT = 1 << 1;

// LibreTiny's switch to output always starts the pin LOW. Enable the output at its level in one store, then update
// LibreTiny's pin state the way its own switch does (cores/beken-72xx/arduino/src/wiring_digital.c).
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

bool ArduinoInternalGPIOPin::setup_open_drain(bool pullup) {
  // LibreTiny's pin read switches outputs to input, so keep the input/output fallback here.
#ifdef LT_HAS_WIRING_DATA
  PinInfo *info = pinInfo(this->pin_);
  if (info == nullptr || !pinSupported(info, PIN_GPIO))
    return false;
#endif
  this->flags_ = gpio::FLAG_INPUT;
  if (pullup)
    this->flags_ = this->flags_ | gpio::FLAG_PULLUP;
  this->pin_mode(this->flags_);
  this->level_ = false;
#if !defined(CLANG_TIDY) && (defined(USE_RTL87XX) || defined(USE_LN882X))
  if (info->data == nullptr || info->data->gpio == nullptr)
    return false;
  this->open_drain_data_ = info->data;
#endif
  return true;
}

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
#if defined(USE_LIBRETINY_VARIANT_RTL8720C) && !defined(CLANG_TIDY)
  // The Arduino read switches the pin back to GPIO and frees its interrupt, so read through the interrupt instead.
  // Remove once LibreTiny's Arduino pin read handles this. The IRQ bit is only set once data->irq exists.
  PinInfo *info = pinInfo(this->pin_);
  if (info != nullptr && (info->enabled & PIN_IRQ))
    return bool(gpio_irq_read(info->data->irq)) ^ this->inverted_;
#endif
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

#if defined(USE_LN882X) && !defined(CLANG_TIDY)
// LN882x GPIO register word offsets (SDK reg_gpio.h); the C register helpers cannot be included from C++.
static constexpr uint8_t LN_GPIO_DDR = 0x18 / 4;
static constexpr uint8_t LN_GPIO_IDR = 0x1C / 4;
static constexpr uint8_t LN_GPIO_BSRR = 0x24 / 4;
#endif

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  return reinterpret_cast<ArduinoInternalGPIOPin *>(this->arg_)->digital_read();
}
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  reinterpret_cast<ArduinoInternalGPIOPin *>(this->arg_)->digital_write(value);
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  detachInterrupt(reinterpret_cast<ArduinoInternalGPIOPin *>(this->arg_)->get_pin());
}
#if defined(USE_LIBRETINY_VARIANT_RTL8720C) && !defined(CLANG_TIDY)
static void IRAM_ATTR rtl8720c_set_open_drain_direction(PinData *data, bool output) {
  const auto &adapter = data->gpio->adapter;
  const uint32_t mask = adapter.bit_mask;
  switch (adapter.port_idx) {
    case 0:
      if (output)
        GPIO->port_a_odm_en = mask;
      else
        GPIO->port_a_idm_en = mask;
      break;
    case 1:
      if (output)
        GPIO->port_b_odm_en = mask;
      else
        GPIO->port_b_idm_en = mask;
      break;
    case 2:
      if (output)
        GPIO->port_c_odm_en = mask;
      else
        GPIO->port_c_idm_en = mask;
      break;
    case 3:
      if (output)
        GPIO->port_d_odm_en = mask;
      else
        GPIO->port_d_idm_en = mask;
      break;
  }
}
#endif

void IRAM_ATTR ISRInternalGPIOPin::open_drain_pull_low() {
  auto *pin = static_cast<ArduinoInternalGPIOPin *>(this->arg_);
#if defined(USE_LN882X) && !defined(CLANG_TIDY)
  auto *data = static_cast<PinData *>(pin->open_drain_data_);
  auto *regs = reinterpret_cast<volatile uint32_t *>(data->gpio_base);  // NOLINT(performance-no-int-to-ptr)
  const uint32_t mask = data->gpio->pin;
  regs[LN_GPIO_BSRR] = mask << 16;
  regs[LN_GPIO_DDR] |= mask;
  data->gpioMode = OUTPUT;
#elif defined(USE_LIBRETINY_VARIANT_RTL8710B) && !defined(CLANG_TIDY)
  auto *data = static_cast<PinData *>(pin->open_drain_data_);
  GPIO_WriteBit(data->gpio->pin, GPIO_PIN_LOW);
  GPIO_Direction(data->gpio->pin, GPIO_Mode_OUT);
  data->gpioMode = OUTPUT;
#elif defined(USE_LIBRETINY_VARIANT_RTL8720C) && !defined(CLANG_TIDY)
  auto *data = static_cast<PinData *>(pin->open_drain_data_);
  const auto &adapter = data->gpio->adapter;
  *adapter.out0_port = adapter.bit_mask;
  rtl8720c_set_open_drain_direction(data, true);
  data->gpioMode = OUTPUT;
#else
  pin->digital_write(pin->is_inverted());
  pin->pin_mode(gpio::FLAG_OUTPUT);
#endif
#if !defined(CLANG_TIDY) && (defined(USE_RTL87XX) || defined(USE_LN882X))
  pin->level_ = false;
  pin->output_ = true;
#endif
}

void IRAM_ATTR ISRInternalGPIOPin::open_drain_release() {
  auto *pin = static_cast<ArduinoInternalGPIOPin *>(this->arg_);
#if defined(USE_LN882X) && !defined(CLANG_TIDY)
  auto *data = static_cast<PinData *>(pin->open_drain_data_);
  auto *regs = reinterpret_cast<volatile uint32_t *>(data->gpio_base);  // NOLINT(performance-no-int-to-ptr)
  regs[LN_GPIO_DDR] &= ~static_cast<uint32_t>(data->gpio->pin);
#elif defined(USE_LIBRETINY_VARIANT_RTL8710B) && !defined(CLANG_TIDY)
  auto *data = static_cast<PinData *>(pin->open_drain_data_);
  GPIO_Direction(data->gpio->pin, GPIO_Mode_IN);
#elif defined(USE_LIBRETINY_VARIANT_RTL8720C) && !defined(CLANG_TIDY)
  auto *data = static_cast<PinData *>(pin->open_drain_data_);
  rtl8720c_set_open_drain_direction(data, false);
#else
  pin->pin_mode(pin->get_flags());
#endif
#if !defined(CLANG_TIDY) && (defined(USE_RTL87XX) || defined(USE_LN882X))
  data->gpioMode = (pin->flags_ & gpio::FLAG_PULLUP) ? INPUT_PULLUP : INPUT;
  pin->output_ = false;
#endif
}

bool IRAM_ATTR ISRInternalGPIOPin::open_drain_read() {
  auto *pin = static_cast<ArduinoInternalGPIOPin *>(this->arg_);
  if (pin->output_)
    return false;
#if defined(USE_LN882X) && !defined(CLANG_TIDY)
  auto *data = static_cast<PinData *>(pin->open_drain_data_);
  auto *regs = reinterpret_cast<volatile uint32_t *>(data->gpio_base);  // NOLINT(performance-no-int-to-ptr)
  return (regs[LN_GPIO_IDR] & data->gpio->pin) != 0;
#elif defined(USE_LIBRETINY_VARIANT_RTL8710B) && !defined(CLANG_TIDY)
  auto *data = static_cast<PinData *>(pin->open_drain_data_);
  return GPIO_ReadDataBit(data->gpio->pin) != 0;
#elif defined(USE_LIBRETINY_VARIANT_RTL8720C) && !defined(CLANG_TIDY)
  auto *data = static_cast<PinData *>(pin->open_drain_data_);
  const auto &adapter = data->gpio->adapter;
  return (*adapter.in_port & adapter.bit_mask) != 0;
#else
  return pin->digital_read() != pin->is_inverted();
#endif
}

void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  reinterpret_cast<ArduinoInternalGPIOPin *>(this->arg_)->pin_mode(flags);
}

}  // namespace esphome

#endif  // USE_LIBRETINY
