#include "airthings_listener.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace airthings_ble {

static const char *TAG = "airthings_ble";

bool AirthingsListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  for (auto &it : device.get_manufacturer_datas()) {
    if (it.uuid == esp32_ble_tracker::ESPBTUUID::from_uint16(0x0334)) {
      uint SN = it.data[0];
      SN |= ((uint) it.data[1] << 8);
      SN |= ((uint) it.data[2] << 16);
      SN |= ((uint) it.data[3] << 24);

      ESP_LOGD(TAG, "Found AirThings device Serial:%u (MAC: %s)", SN, device.address_str().c_str());
      return true;
    }
  }

  return false;
}

}  // namespace airthings_ble
}  // namespace esphome

#endif
