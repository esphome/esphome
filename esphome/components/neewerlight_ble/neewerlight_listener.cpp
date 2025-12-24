#include "neewerlight_listener.h"
#include "esphome/core/log.h"

#include <unordered_set>

#ifdef USE_ESP32

namespace esphome::neewerlight_ble {

static const char *const TAG = "neewerlight_ble";

static const std::unordered_set<std::string> KNOWN_PREFIXES = {
    "NEEWER",
    "NW-",
    "SL",
    "NWR",
};

bool NeewerLightListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  for (const auto &prefix : KNOWN_PREFIXES) {
    if (device.get_name().compare(0, prefix.size(), prefix) == 0) {
      ESP_LOGD(TAG, "Found device with matching prefix: name %s (MAC: %s)", device.get_name().c_str(),
               device.address_str().c_str());
      return true;
    }
  }
  return false;
}

}  // namespace esphome::neewerlight_ble

#endif
