#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NRF52_DFU
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

namespace esphome::nrf52 {
class DeviceFirmwareUpdate : public Component {
 public:
  void setup() override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void dump_config() override;

 protected:
  GPIOPin *reset_pin_{nullptr};
};

}  // namespace esphome::nrf52

#endif
