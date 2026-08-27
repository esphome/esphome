#include "radon_eye_listener.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::radon_eye_ble {

static const char *const TAG = "radon_eye_ble";

bool RadonEyeListener::parse_device(const ble_device_base::ESPBTDevice &device) {
  // Radon Eye devices have names starting with "FR:"
  if (device.get_name().starts_with("FR:")) {
    char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    ESP_LOGD(TAG, "Found Radon Eye device Name: %s (MAC: %s)", device.get_name().c_str(),
             device.address_str_to(addr_buf));
  }
  return false;
}

}  // namespace esphome::radon_eye_ble
