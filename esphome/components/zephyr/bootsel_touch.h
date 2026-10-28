#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZEPHYR_BOOTSEL_TOUCH
#include "esphome/core/component.h"

namespace esphome::zephyr {

class UsbBaudRate;

// A host opening the USB serial port at 1200 baud (the "1200 baud touch") reboots into the ROM USB bootloader;
// 2001 baud reboots into the application.
class BootselTouch final : public Component {
 public:
  explicit BootselTouch(UsbBaudRate *usb_baud_rate) : usb_baud_rate_(usb_baud_rate) {}
  void setup() override;

 protected:
  UsbBaudRate *usb_baud_rate_;
};

}  // namespace esphome::zephyr

#endif
