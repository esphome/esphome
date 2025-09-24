#include "ble_2902.h"
#include "esphome/components/esp32_ble/ble_uuid.h"

#ifdef USE_ESP32

#include <cstring>

namespace esphome {
namespace esp32_ble_server {

BLE2902::BLE2902() : BLEDescriptor(esp32_ble::ESPBTUUID::from_uint16(0x2902)) {
  this->value_.attr_len = 2;
  uint8_t data[2] = {0, 0};
  memcpy(this->value_.attr_value, data, 2);
}

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
