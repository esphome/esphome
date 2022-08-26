#include "mopeka_ble.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace mopeka_ble {

static const char *const TAG = "mopeka_ble";
static const uint8_t MANUFACTURER_DATA_LENGTH = 10;
static const uint16_t MANUFACTURER_ID = 0x0059;

/**
 * Parse all incoming BLE payloads to see if it is a Mopeka BLE advertisement.
 * Currently this supports the following products:
 *
 *   Mopeka Pro Check.
 *    If the sync button is pressed, report the MAC so a user can add this as a sensor.
 */

bool MopekaListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  const auto &manu_datas = device.get_manufacturer_datas();

  if (manu_datas.size() != 1) {
    return false;
  }

  const auto &manu_data = manu_datas[0];

  if (manu_data.data.size() != MANUFACTURER_DATA_LENGTH) {
    return false;
  }

  if (manu_data.uuid != esp32_ble_tracker::ESPBTUUID::from_uint16(MANUFACTURER_ID)) {
    return false;
  }

  if (this->parse_sync_button_(manu_data.data)) {
    // button pressed
    ESP_LOGI(TAG, "SENSOR FOUND: %s", device.address_str().c_str());
  }
  return false;
}

bool MopekaListener::parse_sync_button_(const std::vector<uint8_t> &message) { return (message[2] & 0x80) != 0; }

}  // namespace mopeka_ble
}  // namespace esphome

#endif
