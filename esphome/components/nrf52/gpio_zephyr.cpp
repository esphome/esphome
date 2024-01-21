#ifdef USE_NRF52
#ifdef USE_ZEPHYR

#include "gpio.h"
#include "esphome/core/log.h"
// #include <functional>
// #include <vector>
// #include <device.h>
#include <zephyr/drivers/gpio.h>

namespace esphome {
namespace nrf52 {

static const char *const TAG = "nrf52";

static int flags_to_mode(gpio::Flags flags, uint8_t pin) {
  if (flags == gpio::FLAG_INPUT) {  // NOLINT(bugprone-branch-clone)
    return GPIO_INPUT;
  } else if (flags == gpio::FLAG_OUTPUT) {
    return GPIO_OUTPUT;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    return GPIO_INPUT | GPIO_PULL_UP;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN)) {
    return GPIO_INPUT | GPIO_PULL_DOWN;
  } else if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
      return GPIO_OUTPUT | GPIO_OPEN_DRAIN;
  } else {
    return GPIO_DISCONNECTED;
  }
}

struct ISRPinArg {
  uint8_t pin;
  bool inverted;
};

//TODO implement
//TODO test
// void (*irq_cb)(void *);
// void* irq_arg;
// static void pin_irq(void){
//   irq_cb(irq_arg);
// }

ISRInternalGPIOPin NRF52GPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void NRF52GPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  // uint32_t mode = ISR_DEFERRED;
  // switch (type) {
  //   case gpio::INTERRUPT_RISING_EDGE:
  //     mode |= inverted_ ? FALLING : RISING;
  //     break;
  //   case gpio::INTERRUPT_FALLING_EDGE:
  //     mode |= inverted_ ? RISING : FALLING;
  //     break;
  //   case gpio::INTERRUPT_ANY_EDGE:
  //     mode |= CHANGE;
  //     break;
  //   default:
  //     return;
  // }

  // irq_cb = func;
  // irq_arg = arg;
  // attachInterrupt(pin_, pin_irq, mode);
}

void NRF52GPIOPin::setup() {
  const struct device * gpio = nullptr;
  if(pin_ < 32) {
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
  //TODO why no ready??
  // if (!device_is_ready(gpio)) {
    gpio_ = gpio;
  // } else {
  //   ESP_LOGE(TAG, "gpio %u is not ready.", pin_);
  // }
  gpio_ = gpio;
  pin_mode(flags_); 
}

void NRF52GPIOPin::pin_mode(gpio::Flags flags) {
  if(nullptr == gpio_) {
    return;
  }
  gpio_pin_configure(gpio_, pin_, flags_to_mode(flags, pin_));
}

std::string NRF52GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", pin_);
  return buffer;
}

bool NRF52GPIOPin::digital_read() {
  if(nullptr == gpio_) {
    //TODO invert ??
    return false;
  }
  return bool(gpio_pin_get(gpio_, pin_)) != inverted_;  // NOLINT
}
void NRF52GPIOPin::digital_write(bool value) {
  if(nullptr == gpio_) {
    return;
  }
  gpio_pin_set(gpio_, pin_, value != inverted_ ? 1 : 0);
}
void NRF52GPIOPin::detach_interrupt() const {
  //  detachInterrupt(pin_); 
}

}  // namespace nrf52

// using namespace nrf52;

// TODO seems to not work???
// bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  // auto *arg = reinterpret_cast<nrf52::ISRPinArg *>(arg_);
  // return bool(digitalRead(arg->pin)) != arg->inverted;  // NOLINT
// }

}  // namespace esphome

#endif
#endif  // USE_NRF52
