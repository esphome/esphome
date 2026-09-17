#include "ble_gatt_client.h"

#ifdef USE_BLE_GATT_CLIENT

#include "esphome/core/log.h"

namespace esphome::ble_device_base {

static const char *const TAG = "ble_gatt_client";

const GattCharacteristic *find_characteristic(const GattServiceTable &table, const GattService &service,
                                              const ESPBTUUID &uuid) {
  // 32-bit range math: a corrupt first/count pair cannot wrap past the check.
  uint32_t end = uint32_t(service.first_characteristic) + service.characteristic_count;
  if (end > table.characteristic_count) {
    ESP_LOGW(TAG, "characteristic range out of bounds");
    return nullptr;
  }
  for (uint32_t i = service.first_characteristic; i < end; i++) {
    if (table.characteristics[i].uuid == uuid)
      return &table.characteristics[i];
  }
  return nullptr;
}

const GattDescriptor *find_descriptor(const GattServiceTable &table, const GattCharacteristic &characteristic,
                                      const ESPBTUUID &uuid) {
  uint32_t end = uint32_t(characteristic.first_descriptor) + characteristic.descriptor_count;
  if (end > table.descriptor_count) {
    // Corrupt range, not a missing descriptor.
    ESP_LOGW(TAG, "descriptor range out of bounds");
    return nullptr;
  }
  for (uint32_t i = characteristic.first_descriptor; i < end; i++) {
    if (table.descriptors[i].uuid == uuid)
      return &table.descriptors[i];
  }
  return nullptr;
}

uint16_t find_cccd(const GattServiceTable &table, const GattCharacteristic &characteristic) {
  const GattDescriptor *desc = find_descriptor(table, characteristic, ESPBTUUID::from_uint16(CCCD_UUID));
  return desc != nullptr ? desc->handle : 0;
}

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_GATT_CLIENT
