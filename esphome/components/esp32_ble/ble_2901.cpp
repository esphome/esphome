#include "ble_2901.h"
#include "ble_uuid.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_ble {

BLE2901::BLE2901(const std::string value) : BLE2901((uint8_t *) value.data(), value.length()) {}
BLE2901::BLE2901(uint8_t *data, size_t length) : BLEDescriptor(ESPBTUUID::from_uint16(0x2901)) {
  this->set_value(data, length);
  this->permissions_ = ESP_GATT_PERM_READ;
}

}  // namespace esp32_ble
}  // namespace esphome

#endif
