#include "airthings_listener.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace airthings_ble {

static const char *const TAG = "airthings_ble";

bool AirthingsListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  for (auto &it : device.get_manufacturer_datas()) {
    if (it.uuid == esp32_ble_tracker::ESPBTUUID::from_uint32(0x0334)) {
      if (it.data.size() < 4)
        continue;

      uint32_t sn = it.data[0];
      sn |= ((uint32_t) it.data[1] << 8);
      sn |= ((uint32_t) it.data[2] << 16);
      sn |= ((uint32_t) it.data[3] << 24);

      ESP_LOGD(TAG, "Found AirThings device Serial:%u (MAC: %s)", sn, device.address_str().c_str());
      return true;
    }
  }

  return false;
}

}  // namespace airthings_ble
}  // namespace esphome

#endif
