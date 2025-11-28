#ifdef USE_STM32

#include "gpio.h"
#include "esphome/core/log.h"
#include "gpio_isr.h"

namespace esphome {
namespace stm32 {

static const char *const TAG = "stm32";

uint16_t pin_to_mask(uint8_t pin) { return 1 << (pin & 0xf); }

struct ISRPinArg {
  GPIO_TypeDef *port;
  uint8_t pin;
  gpio::Flags flags;
  bool inverted;
  optional<uint8_t> af;
};

ISRInternalGPIOPin STM32GPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->port = this->port_;
  arg->pin = this->pin_;
  arg->flags = this->flags_;
  arg->inverted = this->inverted_;
  arg->af = this->af_;
  return ISRInternalGPIOPin((void *) arg);
}

void _pin_mode(GPIO_TypeDef *port, uint8_t pin, gpio::Flags flags, optional<uint8_t> af, uint32_t interrupt_type) {
  uint8_t port_index = pin >> 4;
  uint16_t port_mask = 1 << port_index;
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = pin_to_mask(pin);
  if (!af and (flags & gpio::Flags::FLAG_INPUT)) {
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT | interrupt_type;
  } else {
    if (!af) {
      GPIO_InitStruct.Mode = flags & gpio::Flags::FLAG_OPEN_DRAIN ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT_PP;
    } else {
      GPIO_InitStruct.Mode = flags & gpio::Flags::FLAG_OPEN_DRAIN ? GPIO_MODE_AF_OD : GPIO_MODE_AF_PP;
    }
  }
  GPIO_InitStruct.Pull = (flags & gpio::Flags::FLAG_PULLUP)
                             ? GPIO_PULLUP
                             : ((flags & gpio::Flags::FLAG_PULLDOWN) ? GPIO_PULLDOWN : GPIO_NOPULL);
#ifndef F1
  if (af) {
    GPIO_InitStruct.Alternate = *af;
  }
#endif

#ifdef GPIO_SPEED_HIGH
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
#endif

  HAL_GPIO_Init(port, &GPIO_InitStruct);
}

bool _digital_read(GPIO_TypeDef *port, uint8_t pin) { return HAL_GPIO_ReadPin(port, pin_to_mask(pin)); }

void _digital_write(GPIO_TypeDef *port, uint8_t pin, bool value) {
  HAL_GPIO_WritePin(port, pin_to_mask(pin), value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void STM32GPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  uint8_t pin_index = this->pin_ & 0x0f;
  uint32_t interrupt_type = 0;
  switch (type) {
    case gpio::INTERRUPT_RISING_EDGE:
      interrupt_type = this->inverted_ ? GPIO_MODE_IT_FALLING : GPIO_MODE_IT_RISING;
      break;
    case gpio::INTERRUPT_FALLING_EDGE:
      interrupt_type = this->inverted_ ? GPIO_MODE_IT_RISING : GPIO_MODE_IT_FALLING;
      break;
    case gpio::INTERRUPT_ANY_EDGE:
      interrupt_type = GPIO_MODE_IT_RISING_FALLING;
      break;
    case gpio::INTERRUPT_LOW_LEVEL:
    case gpio::INTERRUPT_HIGH_LEVEL:
    default:
      ESP_LOGE(TAG, "not supported interrupt type");
      return;
  }

  stm32_exti_set_handler(pin_index, func, arg);

  _pin_mode(this->port_, this->pin_, this->flags_, this->af_, interrupt_type);
}

void STM32GPIOPin::pin_mode(gpio::Flags flags) { _pin_mode(this->port_, this->pin_, this->flags_, this->af_, 0); }

void STM32GPIOPin::digital_write(bool value) { _digital_write(this->port_, this->pin_, value != this->inverted_); }

std::string STM32GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "P%c%u", 'A' + (this->pin_ >> 4), this->pin_ & 0xf);
  return buffer;
}

bool STM32GPIOPin::digital_read() { return _digital_read(this->port_, this->pin_) != inverted_; }

void STM32GPIOPin::detach_interrupt() const {
  _pin_mode(this->port_, this->pin_, this->flags_, this->af_, 0);
  stm32_exti_set_handler(this->pin_ & 0xf, nullptr, nullptr);
}

}  // namespace stm32

using namespace stm32;

void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto arg = ((struct ISRPinArg *) this->arg_);
  arg->flags = flags;
  _pin_mode(arg->port, arg->pin, arg->flags, arg->af, true);
}

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto arg = ((struct ISRPinArg *) this->arg_);
  return bool(arg->inverted) != _digital_read(arg->port, arg->pin);
}

void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto arg = ((struct ISRPinArg *) this->arg_);
  _digital_write(arg->port, arg->pin, value != bool(arg->inverted));
}

void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  // not supported
}

}  // namespace esphome

#endif  // USE_STM32
