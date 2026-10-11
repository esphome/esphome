#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZEPHYR_NRF52_DFU
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

namespace esphome::zephyr {

class UsbBaudRate;

class DeviceFirmwareUpdate final : public Component {
 public:
  explicit DeviceFirmwareUpdate(UsbBaudRate *usb_baud_rate) : usb_baud_rate_(usb_baud_rate) {}
  void setup() override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void dump_config() override;

 protected:
  void on_baud_rate(uint32_t rate);

  UsbBaudRate *usb_baud_rate_;
  GPIOPin *reset_pin_{nullptr};
};

}  // namespace esphome::zephyr

#endif
