#pragma once

#include "esphome/components/xiaomi_body_scale/xiaomi_body_scale.h"
#include "esphome/core/application.h"

#include <array>
#include <vector>

namespace esphome::xiaomi_body_scale::testing {

using Frame = std::array<uint8_t, 24>;

// Real captures and keys from the xiaomi-ble test suite (Bluetooth-Devices/xiaomi-ble)
static constexpr uint64_t SCALE_A = 0x8CD0B2F6BEEFULL;
static constexpr const char *KEY_A = "0728974d657a4b60964c1b1677f35f7c";
// Weight, heart rate and the 50 kHz impedance
static constexpr Frame PACKET_1 = {0x48, 0x59, 0xd5, 0x3b, 0x0a, 0xbc, 0x07, 0x8f, 0xf2, 0x34, 0x8c, 0x84,
                                   0x41, 0x38, 0xe9, 0x30, 0x22, 0x00, 0x00, 0x00, 0x9e, 0x53, 0x85, 0x99};
// Only the 250 kHz impedance, the end of a bare feet measurement
static constexpr Frame PACKET_2 = {0x48, 0x59, 0xd5, 0x3b, 0x0b, 0xd6, 0xef, 0x0b, 0x25, 0xdb, 0x72, 0x78,
                                   0x5e, 0x7e, 0x2f, 0x46, 0xd6, 0x00, 0x00, 0x00, 0xd8, 0x64, 0x2d, 0xf6};
// PACKET_1 re-encrypted with KEY_A and frame count 0xFF
static constexpr Frame PACKET_1_COUNT_FF = {0x48, 0x59, 0xd5, 0x3b, 0xff, 0x6c, 0x2f, 0xd8, 0xdb, 0xaa, 0x70, 0xc7,
                                            0xbb, 0x1f, 0x0a, 0x08, 0x9c, 0x00, 0x00, 0x00, 0x31, 0xce, 0xf1, 0x95};

static constexpr uint64_t SCALE_B = 0x04AE4767C67CULL;
static constexpr const char *KEY_B = "02d2900363ef629c736a4549677acbee";
// Weight without impedance, the end of a measurement with socks
static constexpr Frame SOCKS = {0x48, 0x59, 0xd5, 0x3b, 0x71, 0x53, 0x04, 0x38, 0xb5, 0x89, 0x4b, 0x24,
                                0x2c, 0x20, 0x99, 0x08, 0xda, 0x00, 0x00, 0x00, 0x47, 0x9e, 0xcd, 0xa3};
// All metrics zero, stepped off the scale
static constexpr Frame STEP_OFF = {0x48, 0x59, 0xd5, 0x3b, 0x72, 0x03, 0x6c, 0x67, 0x94, 0x35, 0x5a, 0x19,
                                   0xdb, 0xc8, 0x64, 0xbf, 0xb3, 0x00, 0x00, 0x00, 0xe4, 0x15, 0x1d, 0xc8};

inline ble_device_base::ESPBTDevice advert(uint64_t address, const Frame &frame) {
  // Service data AD structure for UUID 0xFE95
  std::vector<uint8_t> adv = {static_cast<uint8_t>(frame.size() + 3), 0x16, 0x95, 0xFE};
  adv.insert(adv.end(), frame.begin(), frame.end());
  uint8_t mac[6];
  for (size_t i = 0; i < 6; i++)
    mac[i] = static_cast<uint8_t>(address >> (i * 8));
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac, -60, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

// S200: weight only, 62.25 kg for profile 1
static constexpr uint64_t SCALE_S200 = 0xD07B6F27D729ULL;
static constexpr const char *KEY_S200 = "653b1b10e1cb35e4ac5e60fa45f3bf29";
static constexpr Frame S200_WEIGHT = {0x48, 0x59, 0x04, 0x4c, 0x01, 0x9a, 0x80, 0xa2, 0x75, 0x93, 0x90, 0x10,
                                      0xf0, 0xab, 0xc4, 0xfa, 0xdc, 0x06, 0x00, 0x00, 0x3d, 0x29, 0xc0, 0x44};

struct Harness {
  Harness(uint64_t address, const char *key) : scale(address, key) {
    // The test main does not construct App as generated code does; the stabilized reset needs its scheduler
    static const bool app_constructed = (new (&App) Application(), true);
    (void) app_constructed;
    App.pre_setup("test_scale", 10, "", 0);
    this->scale.set_weight(&this->weight);
    this->scale.set_impedance_low(&this->impedance_low);
    this->scale.set_impedance_high(&this->impedance_high);
    this->scale.set_heart_rate(&this->heart_rate);
    this->scale.set_profile_id(&this->profile_id);
    this->scale.set_stabilized(&this->stabilized);
  }

  ~Harness() {
    // Drop the pending stabilized reset (id 0) while the scale is alive; App outlives each test
    App.scheduler.cancel_timeout(&this->scale, 0u);
    App.scheduler.call(millis());
  }

  XiaomiBodyScale scale;
  sensor::Sensor weight, impedance_low, impedance_high, heart_rate, profile_id;
  binary_sensor::BinarySensor stabilized;
};

}  // namespace esphome::xiaomi_body_scale::testing
