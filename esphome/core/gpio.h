#pragma once
#include "esphome/core/gpio_pin.h"

// Each platform has exactly one internal pin class, so InternalGPIOPin is an alias for
// it instead of an abstract base class: calls through the alias are direct and can be inlined.
#if defined(USE_ESP32)
#include "esphome/components/esp32/gpio.h"
namespace esphome {
using InternalGPIOPin = esp32::ESP32InternalGPIOPin;
}
#elif defined(USE_ESP8266)
#include "esphome/components/esp8266/gpio.h"
namespace esphome {
using InternalGPIOPin = esp8266::ESP8266GPIOPin;
}
#elif defined(USE_LIBRETINY)
#include "esphome/components/libretiny/gpio_arduino.h"
namespace esphome {
using InternalGPIOPin = libretiny::ArduinoInternalGPIOPin;
}
#elif defined(USE_RP2)
#include "esphome/components/rp2/gpio.h"
namespace esphome {
using InternalGPIOPin = rp2::RP2GPIOPin;
}
#elif defined(USE_HOST)
#include "esphome/components/host/gpio.h"
namespace esphome {
using InternalGPIOPin = host::HostGPIOPin;
}
#elif defined(USE_ZEPHYR)
#include "esphome/components/zephyr/gpio.h"
namespace esphome {
using InternalGPIOPin = zephyr::ZephyrGPIOPin;
}
#else
#error "gpio.h: no internal pin class for this platform"
#endif

namespace esphome {
static_assert(InternalGPIOPinContract<InternalGPIOPin>,
              "The platform's internal pin class is missing part of the InternalGPIOPin surface "
              "(esphome/core/gpio_pin.h)");
}  // namespace esphome
