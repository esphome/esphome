#include "ble_2902.h"
#include "ble_uuid.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_ble {

BLE2902::BLE2902() : BLEDescriptor(ESPBTUUID::from_uint16(0x2902)) {
  this->value_.attr_len = 2;
  uint8_t data[2] = {0, 0};
  memcpy(this->value_.attr_value, data, 2);
}

}  // namespace esp32_ble
}  // namespace esphome

#endif
