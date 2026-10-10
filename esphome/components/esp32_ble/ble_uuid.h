#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_UUID

// The BLE UUID type is owned by the platform-neutral ble_device_base layer;
// this header re-exports it under the historical esp32_ble name (esp32 only).
// The full historical API surface — including from_uuid()/get_uuid() with the
// ESP-IDF esp_bt_uuid_t type — is preserved on esp32 builds.

#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::esp32_ble {

using ble_device_base::UUID_STR_LEN;
using ESPBTUUID = ble_device_base::ESPBTUUID;

}  // namespace esphome::esp32_ble

#endif  // USE_ESP32_BLE_UUID
#endif  // USE_ESP32
