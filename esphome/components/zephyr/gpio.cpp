#ifdef USE_ZEPHYR
#include "gpio.h"
#include "esphome/core/log.h"
#include <zephyr/drivers/gpio.h>

namespace esphome {
namespace zephyr {

static const char *const TAG = "zephyr";

static int flags_to_mode(gpio::Flags flags, bool inverted, bool value) {
  int ret = 0;
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

struct ISRPinArg {
  uint8_t pin;
  bool inverted;
};

ISRInternalGPIOPin ZephyrGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = this->pin_;
  arg->inverted = this->inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void ZephyrGPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  // TODO
}

void ZephyrGPIOPin::setup() {
  const struct device *gpio = nullptr;
  if (pin_ < 32) {
#define GPIO0 DT_NODELABEL(gpio0)
#if DT_NODE_HAS_STATUS(GPIO0, okay)
    gpio = DEVICE_DT_GET(GPIO0);
#else
#error "gpio0 is disabled"
#endif
  } else {
#define GPIO1 DT_NODELABEL(gpio1)
#if DT_NODE_HAS_STATUS(GPIO1, okay)
    gpio = DEVICE_DT_GET(GPIO1);
#else
#error "gpio1 is disabled"
#endif
  }
  if (device_is_ready(gpio)) {
    gpio_ = gpio;
  } else {
    ESP_LOGE(TAG, "gpio %u is not ready.", pin_);
    return;
  }
  pin_mode(flags_);
}

void ZephyrGPIOPin::pin_mode(gpio::Flags flags) {
  if (nullptr == this->gpio_) {
    return;
  }
  gpio_pin_configure(this->gpio_, this->pin_ % 32, flags_to_mode(flags, this->inverted_, this->value_));
}

std::string ZephyrGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", this->pin_);
  return buffer;
}

bool ZephyrGPIOPin::digital_read() {
  if (nullptr == this->gpio_) {
    return false;
  }
  return bool(gpio_pin_get(this->gpio_, this->pin_ % 32) != this->inverted_);
}

void ZephyrGPIOPin::digital_write(bool value) {
  // make sure that value is not ignored since it can be inverted e.g. on switch side
  // that way init state should be correct
  this->value_ = value;
  if (nullptr == this->gpio_) {
    return;
  }
  gpio_pin_set(this->gpio_, this->pin_ % 32, value != this->inverted_ ? 1 : 0);
}
void ZephyrGPIOPin::detach_interrupt() const {
  // TODO
}

}  // namespace zephyr

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  // TODO
  return false;
}

}  // namespace esphome

#endif
