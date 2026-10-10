#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/bthome_mithermometer/bthome_ble.h"

namespace esphome::bthome_mithermometer::testing {

static constexpr uint64_t SENSOR_ADDRESS = 0xA4C1384E1678ULL;

// Encrypted BTHome v2 frames, generated with Python `cryptography` AESCCM(tag_length=4)
// and the bindkey below. Layout: device info (0x41), ciphertext, counter
// (little-endian), MIC. Each plaintext is a single temperature object (0x02).
// Bindkey: eef418daf699a0c188f3bfd17e4565d9
using Frame = std::array<uint8_t, 12>;

// Service data advertisement (UUID 0xFCD2) for `frame`, sent from `address`.
inline ble_device_base::ESPBTDevice advert(const Frame &frame, uint64_t address = SENSOR_ADDRESS) {
  std::vector<uint8_t> adv = {static_cast<uint8_t>(frame.size() + 3), 0x16, 0xD2, 0xFC};
  adv.insert(adv.end(), frame.begin(), frame.end());
  uint8_t mac[6];
  for (size_t i = 0; i < 6; i++)
    mac[i] = static_cast<uint8_t>(address >> (i * 8));
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac, -60, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

struct Harness {
  explicit Harness(bool replay_protection = true) {
    this->thermometer.set_address(SENSOR_ADDRESS);
    this->thermometer.set_bindkey(
        {0xEE, 0xF4, 0x18, 0xDA, 0xF6, 0x99, 0xA0, 0xC1, 0x88, 0xF3, 0xBF, 0xD1, 0x7E, 0x45, 0x65, 0xD9});
    this->thermometer.set_replay_protection(replay_protection);
    this->thermometer.set_temperature(&this->temperature);
  }

  BTHomeMiThermometer thermometer;
  sensor::Sensor temperature;
};

}  // namespace esphome::bthome_mithermometer::testing
