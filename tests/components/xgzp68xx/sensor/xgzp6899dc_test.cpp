#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "../common.h"

namespace esphome::xgzp68xx::testing {

TEST(XGZP6899DCTest, MatchesHardwareReading) {
  Fixture f;
  f.chip.setup();
  ASSERT_FALSE(f.chip.is_failed());
  // About -21.5 Pa and 22.6 C, as read at rest on hardware
  f.bus.set_data(-45088, 3942);
  f.measure();
  EXPECT_NEAR(f.pressure.state, -21.5f, 0.01f);
  EXPECT_FLOAT_EQ(f.temperature.state, 22.59375f);
  EXPECT_FALSE(f.chip.status_has_warning());
}

TEST(XGZP6899DCTest, TransactionsMatchVendorSequence) {
  Fixture f;
  f.chip.setup();
  f.measure();
  // Single-register transactions only, and CFG_OSR is never touched without an oversampling setting
  EXPECT_EQ(f.bus.transactions, (std::vector<Transaction>{
                                    read_of(0x00),
                                    read_of(0x20),
                                    read_of(0x21),
                                    write_of(0x01, 0x01),
                                    read_of(0x04),
                                    read_of(0x05),
                                    read_of(0x06),
                                    read_of(0x07),
                                    read_of(0x08),
                                }));
}

TEST(XGZP6899DCTest, FullScaleAndNegativeOffset) {
  Fixture f;
  // Datasheet example: 0x8D is -8192, 0x46 is shift 7
  f.bus.registers[0x20] = 0x8D;
  f.bus.registers[0x21] = 0x46;
  f.chip.setup();
  f.bus.set_data(1 << 20, -8192 + 128);
  f.measure();
  // 2^20 / 2^21 * 1000 Pa
  EXPECT_FLOAT_EQ(f.pressure.state, 500.0f);
  EXPECT_FLOAT_EQ(f.temperature.state, 26.0f);
}

TEST(XGZP6899DCTest, OversamplingKeepsTemperatureBitsAndSetsNormalMode) {
  Fixture f;
  // OSR_T 111b, OSR_P 000b, MODE 10b
  f.bus.registers[0x02] = 0xE2;
  f.chip.set_pressure_oversampling(5);
  f.chip.setup();
  ASSERT_FALSE(f.chip.is_failed());
  EXPECT_EQ(f.bus.registers[0x02], 0xF5);
}

TEST(XGZP6899DCTest, WrongIdOnlyWarns) {
  Fixture f;
  f.bus.registers[0x00] = 0x00;
  f.chip.setup();
  EXPECT_FALSE(f.chip.is_failed());
}

TEST(XGZP6899DCTest, SilentBusAtSetupFails) {
  Fixture f;
  f.bus.failing.fill(true);
  f.chip.setup();
  EXPECT_TRUE(f.chip.is_failed());
}

TEST(XGZP6899DCTest, FailedCalibrationReadFails) {
  Fixture f;
  f.bus.failing[0x21] = true;
  f.chip.setup();
  EXPECT_TRUE(f.chip.is_failed());
}

TEST(XGZP6899DCTest, FailedOversamplingReadFails) {
  Fixture f;
  f.bus.failing[0x02] = true;
  f.chip.set_pressure_oversampling(5);
  f.chip.setup();
  EXPECT_TRUE(f.chip.is_failed());
}

TEST(XGZP6899DCTest, FailedOversamplingWriteFails) {
  Fixture f;
  f.bus.fail_writes = true;
  f.chip.set_pressure_oversampling(5);
  f.chip.setup();
  EXPECT_TRUE(f.chip.is_failed());
}

TEST(XGZP6899DCTest, FailedStartSetsWarningAndRecovers) {
  Fixture f;
  f.chip.setup();
  f.bus.failing.fill(true);
  f.chip.update();
  EXPECT_TRUE(f.chip.status_has_warning());

  f.bus.failing.fill(false);
  f.bus.set_data(0, 4096);
  f.measure();
  EXPECT_FALSE(f.chip.status_has_warning());
  EXPECT_FLOAT_EQ(f.temperature.state, 25.0f);
}

TEST(XGZP6899DCTest, FailedDataReadPublishesNothing) {
  Fixture f;
  f.chip.setup();
  f.bus.failing[0x07] = true;
  f.measure();
  EXPECT_TRUE(f.chip.status_has_warning());
  EXPECT_FALSE(f.pressure.has_state());
  EXPECT_FALSE(f.temperature.has_state());
}

}  // namespace esphome::xgzp68xx::testing
