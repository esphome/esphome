#include "radon_eye_listener.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <vector>

#ifdef USE_ESP32

namespace esphome {
namespace radon_eye_ble {

static const char *const TAG = "radon_eye_ble";

bool RadonEyeListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (not device.get_name().empty()) {
    // Vector containing the prefixes to search for
    std::vector<std::string> prefixes = {"FR:R", "FR:I", "FR:H"};

    // Check if the device name starts with any of the prefixes
    if (std::any_of(prefixes.begin(), prefixes.end(),
                    [&](const std::string &prefix) { return device.get_name().rfind(prefix, 0) == 0; })) {
      // Device found
      ESP_LOGD(TAG, "Found Radon Eye device Name: %s (MAC: %s)", device.get_name().c_str(),
               device.address_str().c_str());
    }
  }
  return false;
}

}  // namespace radon_eye_ble
}  // namespace esphome

#endif
