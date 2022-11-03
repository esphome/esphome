#include "detach_switch.h"
#ifdef USE_ESP32_VARIANT_ESP32S2
#ifdef USE_SWITCH
#include "esphome/core/log.h"
#include "Adafruit_TinyUSB.h"

namespace esphome {
namespace usb_device {

static const char *const TAG = "usb_device";

void DetachSwitch::write_state(bool state) {
  if (state) {
    TinyUSBDevice.detach();
  } else {
    TinyUSBDevice.attach();
  }
  this->publish_state(state);
}

void DetachSwitch::dump_config() { LOG_SWITCH("", "Detach switch", this); }

}  // namespace usb_device
}  // namespace esphome
#endif
#endif
