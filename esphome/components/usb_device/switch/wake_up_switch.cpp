#include "wake_up_switch.h"
#ifdef USE_ESP32_VARIANT_ESP32S2
#ifdef USE_SWITCH
#include "esphome/core/log.h"
#include "Adafruit_TinyUSB.h"

namespace esphome {
namespace usb_device {

static const char *const TAG = "usb_device";

void WakeUpSwitch::write_state(bool state) {
  if (state) {
    if (!TinyUSBDevice.remoteWakeup()) {
      ESP_LOGW(TAG, "Unable to wakeup USB host");
    }
  }
  this->publish_state(false);
}

void WakeUpSwitch::dump_config() { LOG_SWITCH("", "Wake up switch", this); }

}  // namespace usb_device
}  // namespace esphome
#endif
#endif
