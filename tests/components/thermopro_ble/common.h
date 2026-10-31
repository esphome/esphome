#pragma once

#include "esphome/components/ble_device_base/ble_device.h"

#include <cstring>
#include <vector>

namespace esphome::thermopro_ble::testing {

// Least significant byte first, as from_scan_result() takes them
static constexpr uint8_t MAC[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
static constexpr uint8_t OTHER_MAC[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x77};
static constexpr int RSSI = -60;

// Manufacturer data records (company ID little-endian, then payload) from the
// Bluetooth-Devices/thermopro-ble test suite.
static const std::vector<uint8_t> TP357 = {0xC2, 0xF1, 0x00, 0x1D, 0x02, 0x2C};
static const std::vector<uint8_t> TP357S = {0xC2, 0xEE, 0x00, 0x29, 0x22, 0x0B, 0x01};
static const std::vector<uint8_t> TP393 = {0xC2, 0xF2, 0x00, 0x35, 0x02, 0x2C};
static const std::vector<uint8_t> TP960R = {0x00, 0x3A, 0x00, 0x30, 0x08, 0x38, 0x00};
static const std::vector<uint8_t> TP970R = {0x00, 0x35, 0x00, 0x46, 0x0B, 0x35, 0x00};
static const std::vector<uint8_t> TP972S = {0x00, 0x72, 0x00, 0x6A, 0x0A, 0x33, 0xD3, 0xB8, 0x42, 0x00, 0x40, 0xAE,
                                            0x42, 0x66, 0x06, 0xAE, 0x42, 0x6C, 0x54, 0x73, 0x77, 0x44, 0xF8};
static const std::vector<uint8_t> TP972S_2 = {0x00, 0x8D, 0x00, 0xA6, 0x0A, 0xCD, 0xEC, 0xFF, 0x42, 0x9A, 0x69, 0x00,
                                              0x43, 0x9A, 0x69, 0x00, 0x43, 0x6C, 0x54, 0x73, 0x77, 0x44, 0xF8};
// TP96-style payload followed by 6 bytes of base station MAC
static const std::vector<uint8_t> TP972S_13B = {0x00, 0x33, 0x00, 0x48, 0x0A, 0x33, 0x00,
                                                0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA};
// Temperature and humidity bytes all 0xFF mark an invalid reading
static const std::vector<uint8_t> TP357S_INVALID = {0xC2, 0xFF, 0xFF, 0xFF, 0x22, 0x0B, 0x01};
static const std::vector<uint8_t> INVALID_TP972 = {0xC2, 0xC7, 0x01, 0x8D, 0x00, 0xE1, 0x0A, 0x33, 0x13,
                                                   0xFA, 0x42, 0x00, 0xC0, 0xFA, 0x42, 0x9A, 0xF9, 0xFA,
                                                   0x42, 0x6C, 0x54, 0x73, 0x77, 0x44, 0xF8};

static ble_device_base::ESPBTDevice device_from(const uint8_t *mac, const char *name,
                                                const std::vector<uint8_t> &record) {
  std::vector<uint8_t> adv;
  if (name != nullptr) {
    size_t len = std::strlen(name);
    adv.push_back(static_cast<uint8_t>(len + 1));
    adv.push_back(0x09);
    adv.insert(adv.end(), name, name + len);
  }
  adv.push_back(static_cast<uint8_t>(record.size() + 1));
  adv.push_back(0xFF);
  adv.insert(adv.end(), record.begin(), record.end());
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac, RSSI, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

}  // namespace esphome::thermopro_ble::testing
