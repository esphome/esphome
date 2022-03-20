#include "radon_eye_listener.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace radon_eye_ble {

static const char *const TAG = "radon_eye_ble";

bool RadonEyeListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (not device.get_name().empty()) {
    if (device.get_name().rfind("FR:R20:SN", 0) == 0) {
      // This is an RD200, I think
      ESP_LOGD(TAG, "Found Radon Eye RD200 device Name: %s (MAC: %s)", device.get_name().c_str(),
               device.address_str().c_str());
    }
  }
  return false;
}

}  // namespace radon_eye_ble
}  // namespace esphome

#endif
