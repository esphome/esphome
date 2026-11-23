#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "esphome/components/bthome_mithermometer/bthome_ble.h"

namespace esphome::bthome_mithermometer::testing {

static constexpr uint64_t SENSOR_ADDRESS = 0xA4C1384E1678ULL;

// BTHome v2 service data advertisement (UUID 0xFCD2) sent from `address`.
inline ble_device_base::ESPBTDevice advert(const uint8_t *service_data, size_t size,
                                           uint64_t address = SENSOR_ADDRESS) {
  std::vector<uint8_t> adv = {static_cast<uint8_t>(size + 3), 0x16, 0xD2, 0xFC};
  adv.insert(adv.end(), service_data, service_data + size);
  uint8_t mac[6];
  for (size_t i = 0; i < 6; i++)
    mac[i] = static_cast<uint8_t>(address >> (i * 8));
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac, -60, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

inline ble_device_base::ESPBTDevice advert(std::initializer_list<uint8_t> service_data,
                                           uint64_t address = SENSOR_ADDRESS) {
  return advert(service_data.begin(), service_data.size(), address);
}

template<size_t N>
inline ble_device_base::ESPBTDevice advert(const std::array<uint8_t, N> &service_data,
                                           uint64_t address = SENSOR_ADDRESS) {
  return advert(service_data.data(), N, address);
}

}  // namespace esphome::bthome_mithermometer::testing
