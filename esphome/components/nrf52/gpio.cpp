#ifdef USE_NRF52

#include "gpio.h"
#include "esphome/core/log.h"
#include <functional>
#include <vector>

namespace esphome {
namespace nrf52 {

static const char *const TAG = "nrf52";

static int IRAM_ATTR flags_to_mode(gpio::Flags flags, uint8_t pin) {
// For nRF52 extra modes are available.
// Standard drive is typically 2mA (min 1mA) '0' sink (low) or '1' source (high). High drive (VDD > 2.7V) is typically 10mA low, 9mA high (min 6mA)
// OUTPUT_S0S1 Standard '0', standard '1' same as OUTPUT
// OUTPUT_H0S1 High drive '0', standard '1'
// OUTPUT_S0H1 Standard '0', high drive '1'
// OUTPUT_H0H1 High drive '0', high 'drive '1''
// OUTPUT_D0S1 Disconnect '0' standard '1' (normally used for wired-or connections)
// OUTPUT_D0H1 Disconnect '0', high drive '1' (normally used for wired-or connections)
// OUTPUT_S0D1 Standard '0'. disconnect '1' (normally used for wired-and connections)
// OUTPUT_H0D1 High drive '0', disconnect '1' (normally used for wired-and connections)
// NOTE P0.27 should be only low (standard) drive, low frequency
  if (flags == gpio::FLAG_INPUT) {  // NOLINT(bugprone-branch-clone)
    return INPUT;
  } else if (flags == gpio::FLAG_OUTPUT) {
    return OUTPUT;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    return INPUT_PULLUP;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN)) {
    return INPUT_PULLDOWN;
  } else if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
      return OUTPUT_S0D1;
  } else {
    return INPUT;
  }
}

struct ISRPinArg {
  uint8_t pin;
  bool inverted;
};

//TODO implement
//TODO test
void (*irq_cb)(void *);
void* irq_arg;
static void pin_irq(void){
  irq_cb(irq_arg);
}

ISRInternalGPIOPin NRF52GPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void NRF52GPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  uint32_t mode = ISR_DEFERRED;
  switch (type) {
    case gpio::INTERRUPT_RISING_EDGE:
      mode |= inverted_ ? FALLING : RISING;
      break;
    case gpio::INTERRUPT_FALLING_EDGE:
      mode |= inverted_ ? RISING : FALLING;
      break;
    case gpio::INTERRUPT_ANY_EDGE:
      mode |= CHANGE;
      break;
    default:
      return;
  }

  irq_cb = func;
  irq_arg = arg;
  attachInterrupt(pin_, pin_irq, mode);
}
void NRF52GPIOPin::pin_mode(gpio::Flags flags) {
  pinMode(pin_, flags_to_mode(flags, pin_));  // NOLINT
}

std::string NRF52GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", pin_);
  return buffer;
}

bool NRF52GPIOPin::digital_read() {
  return bool(digitalRead(pin_)) != inverted_;  // NOLINT
}
void NRF52GPIOPin::digital_write(bool value) {
  digitalWrite(pin_, value != inverted_ ? 1 : 0);  // NOLINT
}
void NRF52GPIOPin::detach_interrupt() const {
   detachInterrupt(pin_); 
}

}  // namespace nrf52

// using namespace nrf52;

// TODO seems to not work???
bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<nrf52::ISRPinArg *>(arg_);
  return bool(digitalRead(arg->pin)) != arg->inverted;  // NOLINT
}

}  // namespace esphome

#endif  // USE_NRF52
