#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZEPHYR_TINYUF2_TOUCH
#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::zephyr {

class UsbBaudRate;

// A host opening the USB serial port at 1200 baud reboots into the TinyUF2 bootloader;
// 2001 baud reboots into the application.
class TinyUf2Touch final : public Component {
 public:
  TinyUf2Touch(UsbBaudRate *usb_baud_rate, uintptr_t dbl_tap_addr)
      : usb_baud_rate_(usb_baud_rate), dbl_tap_addr_(dbl_tap_addr) {}
  void setup() override;

 protected:
  UsbBaudRate *usb_baud_rate_;
  uintptr_t dbl_tap_addr_;
};

}  // namespace esphome::zephyr

#endif
