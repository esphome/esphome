#pragma once

#include "esphome/core/gpio.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"

namespace esphome::sx1509 {

class SX1509Component;

using SX1509GPIOPin = gpio_expander::ExpanderGPIOPin<SX1509Component>;

}  // namespace esphome::sx1509
