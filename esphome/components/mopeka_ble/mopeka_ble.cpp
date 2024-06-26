#include "mopeka_ble.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace mopeka_ble {

static const char *const TAG = "mopeka_ble";

// Mopeka Std (CC2540) sensor details
static const uint16_t SERVICE_UUID_CC2540 = 0xADA0;
static const uint16_t MANUFACTURER_CC2540_ID = 0x000D;  // Texas Instruments (TI)
static const uint8_t MANUFACTURER_CC2540_DATA_LENGTH = 23;

// Mopeka Pro (NRF52) sensor details
static const uint16_t SERVICE_UUID_NRF52 = 0xFEE5;
static const uint16_t MANUFACTURER_NRF52_ID = 0x0059;  // Nordic
static const uint8_t MANUFACTURER_NRF52_DATA_LENGTH = 10;

/**
 * Parse all incoming BLE payloads to see if it is a Mopeka BLE advertisement.
 * Currently this supports the following products:
 *
 *  - Mopeka Std Check - uses the chip CC2540 by Texas Instruments (TI)
 *  - Mopeka Pro Check - uses the chip NRF52 by Nordic
 *
 *    If the sync button is pressed, report the MAC so a user can add this as a sensor. Or if user has configured
 * `show_sensors_without_sync_` than report all visible sensors.
 * Three points are used to identify a sensor:
 *
 * - Bluetooth service uuid
 * - Bluetooth manufacturer id
 * - Bluetooth data frame size
 */

bool MopekaListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // Fetch information about BLE device.
  const auto &service_uuids = device.get_service_uuids();
  if (service_uuids.size() != 1) {
    return false;
  }
  const auto &service_uuid = service_uuids[0];

  const auto &manu_datas = device.get_manufacturer_datas();
  if (manu_datas.size() != 1) {
    return false;
  }
  const auto &manu_data = manu_datas[0];

  // Is the device maybe a Mopeka Std (CC2540) sensor.
  if (service_uuid == esp32_ble_tracker::ESPBTUUID::from_uint16(SERVICE_UUID_CC2540)) {
    if (manu_data.uuid != esp32_ble_tracker::ESPBTUUID::from_uint16(MANUFACTURER_CC2540_ID)) {
      return false;
    }

    if (manu_data.data.size() != MANUFACTURER_CC2540_DATA_LENGTH) {
      return false;
    }

    const bool sync_button_pressed = (manu_data.data[3] & 0x80) != 0;

    if (this->show_sensors_without_sync_ || sync_button_pressed) {
      ESP_LOGI(TAG, "MOPEKA STD (CC2540) SENSOR FOUND: %s", device.address_str().c_str());
    }

    // Is the device maybe a Mopeka Pro (NRF52) sensor.
  } else if (service_uuid == esp32_ble_tracker::ESPBTUUID::from_uint16(SERVICE_UUID_NRF52)) {
    if (manu_data.uuid != esp32_ble_tracker::ESPBTUUID::from_uint16(MANUFACTURER_NRF52_ID)) {
      return false;
    }

    if (manu_data.data.size() != MANUFACTURER_NRF52_DATA_LENGTH) {
      return false;
    }

    const bool sync_button_pressed = (manu_data.data[2] & 0x80) != 0;

    if (this->show_sensors_without_sync_ || sync_button_pressed) {
      ESP_LOGI(TAG, "MOPEKA PRO (NRF52) SENSOR FOUND: %s", device.address_str().c_str());
    }
  }

  return false;
}

}  // namespace mopeka_ble
}  // namespace esphome

#endif
