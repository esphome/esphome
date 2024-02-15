#ifdef USE_ZEPHYR
#include "gpio.h"
#include "esphome/core/log.h"
#include <zephyr/drivers/gpio.h>

namespace esphome {
namespace zephyr {

static const char *const TAG = "nrf52";

static int flags_to_mode(gpio::Flags flags, uint8_t pin, bool inverted, bool value) {
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

ISRInternalGPIOPin NRF52GPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void NRF52GPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  // TODO
}

void NRF52GPIOPin::setup() {
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

void NRF52GPIOPin::pin_mode(gpio::Flags flags) {
  if (nullptr == gpio_) {
    return;
  }
  gpio_pin_configure(gpio_, pin_, flags_to_mode(flags, pin_, inverted_, value_));
}

std::string NRF52GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", pin_);
  return buffer;
}

bool NRF52GPIOPin::digital_read() {
  if (nullptr == gpio_) {
    return false;
  }
  return bool(gpio_pin_get(gpio_, pin_) != inverted_);
}

void NRF52GPIOPin::digital_write(bool value) {
  // make sure that value is not ignored since it can be inverted e.g. on switch side
  // that way init state should be correct
  value_ = value;
  if (nullptr == gpio_) {
    return;
  }
  gpio_pin_set(gpio_, pin_, value != inverted_ ? 1 : 0);
}
void NRF52GPIOPin::detach_interrupt() const {
  // TODO
}

}  // namespace zephyr

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  // TODO
  return false;
}

}  // namespace esphome

#endif
