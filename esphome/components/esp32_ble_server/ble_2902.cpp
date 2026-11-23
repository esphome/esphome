#include "ble_2902.h"
#include "esphome/components/esp32_ble/ble_uuid.h"

#ifdef USE_ESP32

#include <esp_gatt_defs.h>

namespace esphome::esp32_ble_server {

// The client configuration value is always 2 bytes
BLE2902::BLE2902() : BLEDescriptor(esp32_ble::ESPBTUUID::from_uint16(ESP_GATT_UUID_CHAR_CLIENT_CONFIG), 2) {
  this->set_value({0, 0});
}

}  // namespace esphome::esp32_ble_server

#endif
