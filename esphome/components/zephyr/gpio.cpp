#ifdef USE_ZEPHYR
#include "gpio.h"
#include <zephyr/drivers/gpio.h>
#include "esphome/core/log.h"

namespace esphome {
namespace zephyr {

static const char *const TAG = "zephyr";

struct ISRPinArg {
  const device *gpio;
  uint8_t pin;
  uint8_t gpio_size;
  bool inverted;
};

static gpio_flags_t flags_to_mode(gpio::Flags flags, bool inverted, bool value) {
  gpio_flags_t ret = 0;
  if (flags & gpio::FLAG_INPUT) {
    ret |= GPIO_INPUT;
  }
  if (flags & gpio::FLAG_OUTPUT) {
    ret |= GPIO_OUTPUT;
    if (value != inverted) {
      ret |= GPIO_OUTPUT_INIT_HIGH;
    } else {
      ret |= GPIO_OUTPUT_INIT_LOW;
    }
  }
  if (flags & gpio::FLAG_PULLUP) {
    ret |= GPIO_PULL_UP;
  }
  if (flags & gpio::FLAG_PULLDOWN) {
    ret |= GPIO_PULL_DOWN;
  }
  if (flags & gpio::FLAG_OPEN_DRAIN) {
    ret |= GPIO_OPEN_DRAIN;
  }
  return ret;
}

static gpio_flags_t interrupt_type_to_flags(gpio::InterruptType type) {
  switch (type) {
    case gpio::INTERRUPT_RISING_EDGE:
      return GPIO_INT_EDGE_RISING;
    case gpio::INTERRUPT_FALLING_EDGE:
      return GPIO_INT_EDGE_FALLING;
    case gpio::INTERRUPT_ANY_EDGE:
      return GPIO_INT_EDGE_BOTH;
    case gpio::INTERRUPT_LOW_LEVEL:
      return GPIO_INT_LEVEL_LOW;
    case gpio::INTERRUPT_HIGH_LEVEL:
      return GPIO_INT_LEVEL_HIGH;
  }
  return GPIO_INT_EDGE_RISING;
}

// Registry to map callbacks back to ZephyrGPIOPin instances
static ZephyrGPIOPin *gpio_callback_registry[48] = {nullptr};

void ZephyrGPIOPin::zephyr_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
  for (int i = 0; i < 48; i++) {
    if (gpio_callback_registry[i] != nullptr && &gpio_callback_registry[i]->callback_ == cb) {
      if (gpio_callback_registry[i]->isr_func_ != nullptr) {
        gpio_callback_registry[i]->isr_func_(gpio_callback_registry[i]->isr_arg_);
      }
      return;
    }
  }
}

ISRInternalGPIOPin ZephyrGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->gpio = this->gpio_;
  arg->pin = this->pin_;
  arg->gpio_size = this->gpio_size_;
  arg->inverted = this->inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void ZephyrGPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  if (this->gpio_ == nullptr) {
    ESP_LOGE(TAG, "Cannot attach interrupt: GPIO device not ready");
    return;
  }

  // Detach any existing interrupt first
  this->detach_interrupt();

  this->isr_func_ = func;
  this->isr_arg_ = arg;

  uint8_t port_pin = this->pin_ % this->gpio_size_;

  gpio_init_callback(&this->callback_, ZephyrGPIOPin::zephyr_gpio_callback, BIT(port_pin));

  int ret = gpio_add_callback(this->gpio_, &this->callback_);
  if (ret != 0) {
    ESP_LOGE(TAG, "gpio_add_callback failed for pin %u: %d", this->pin_, ret);
    return;
  }

  ret = gpio_pin_interrupt_configure(this->gpio_, port_pin, interrupt_type_to_flags(type));
  if (ret != 0) {
    ESP_LOGE(TAG, "gpio_pin_interrupt_configure failed for pin %u: %d", this->pin_, ret);
    gpio_remove_callback(this->gpio_, &this->callback_);
    return;
  }

  if (this->pin_ < 48) {
    gpio_callback_registry[this->pin_] = const_cast<ZephyrGPIOPin *>(this);
  }

  ESP_LOGD(TAG, "Interrupt attached to pin %u (type=%d)", this->pin_, (int) type);
}

void ZephyrGPIOPin::setup() {
  if (!device_is_ready(this->gpio_)) {
    ESP_LOGE(TAG, "gpio %u is not ready.", this->pin_);
    return;
  }
  this->pin_mode(this->flags_);
}

void ZephyrGPIOPin::pin_mode(gpio::Flags flags) {
  if (nullptr == this->gpio_) {
    return;
  }
  auto ret = gpio_pin_configure(this->gpio_, this->pin_ % this->gpio_size_,
                                flags_to_mode(flags, this->inverted_, this->value_));
  if (ret != 0) {
    ESP_LOGE(TAG, "gpio %u cannot be configured %d.", this->pin_, ret);
  }
}

size_t ZephyrGPIOPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len, "GPIO%u, %s%u", this->pin_, this->pin_name_prefix_, this->pin_ % this->gpio_size_);
}

bool ZephyrGPIOPin::digital_read() {
  if (nullptr == this->gpio_) {
    return false;
  }
  return bool(gpio_pin_get(this->gpio_, this->pin_ % this->gpio_size_) != this->inverted_);
}

void ZephyrGPIOPin::digital_write(bool value) {
  // make sure that value is not ignored since it can be inverted e.g. on switch side
  // that way init state should be correct
  this->value_ = value;
  if (nullptr == this->gpio_) {
    return;
  }
  gpio_pin_set(this->gpio_, this->pin_ % this->gpio_size_, value != this->inverted_ ? 1 : 0);
}

void ZephyrGPIOPin::detach_interrupt() const {
  if (this->gpio_ == nullptr) {
    return;
  }

  uint8_t port_pin = this->pin_ % this->gpio_size_;
  gpio_pin_interrupt_configure(this->gpio_, port_pin, GPIO_INT_DISABLE);
  gpio_remove_callback(this->gpio_, &this->callback_);

  if (this->pin_ < 48) {
    gpio_callback_registry[this->pin_] = nullptr;
  }

  this->isr_func_ = nullptr;
  this->isr_arg_ = nullptr;
}

}  // namespace zephyr

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = (zephyr::ISRPinArg *) this->arg_;
  if (arg == nullptr || arg->gpio == nullptr) {
    return false;
  }
  return bool(gpio_pin_get(arg->gpio, arg->pin % arg->gpio_size) != arg->inverted);
}

}  // namespace esphome

#endif
