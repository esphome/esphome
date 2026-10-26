#include <gtest/gtest.h>

#include "esphome/components/delta_wallbox/delta_wallbox.h"

namespace esphome::delta_wallbox::testing {

// 0x011B response: unit, FC03, byte count 18, then energy (7 bytes, Wh), power (4 bytes, W),
// hours, minutes, seconds, state and 3 unused bytes. The CRC is checked before decoding, so it is left out.
TEST(DeltaWallboxChargingInfo, ReadsPowerAndEnergyFromTheirOwnBytes) {
  const uint8_t frame[] = {
      0x01, 0x03, 0x12,                          // unit, function, byte count
      0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x39,  // energy 12345 Wh
      0x00, 0x00, 0x1C, 0xE8,                    // power 7400 W
      0x01, 0x02, 0x1E,                          // 1 h 2 min 30 s
      0x03,                                      // state
      0x00, 0x00, 0x00,
  };
  const ChargingInfo info = decode_charging_info(frame);
  EXPECT_FLOAT_EQ(info.power_kw, 7.4f);
  EXPECT_FLOAT_EQ(info.energy_kwh, 12.345f);
  EXPECT_FLOAT_EQ(info.charging_minutes, 62.5f);
  EXPECT_EQ(info.state, 3);
}

TEST(DeltaWallboxChargingInfo, ClampsEnergy) {
  const uint8_t frame[] = {
      0x01, 0x03, 0x12, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
      0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const ChargingInfo info = decode_charging_info(frame);
  EXPECT_FLOAT_EQ(info.energy_kwh, 999.9f);
  EXPECT_FLOAT_EQ(info.power_kw, 0.1f);
}

}  // namespace esphome::delta_wallbox::testing
