#include "../common.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace esphome::xiaomi_body_scale::testing {

TEST(XiaomiBodyScale, DecodesWeightHeartRateAndLowImpedance) {
  Harness h(SCALE_A, KEY_A);
  ASSERT_TRUE(h.scale.parse_device(advert(SCALE_A, PACKET_1)));
  EXPECT_FLOAT_EQ(h.weight.state, 69.9f);
  EXPECT_FLOAT_EQ(h.impedance_low.state, 543.2f);
  EXPECT_FLOAT_EQ(h.heart_rate.state, 92.0f);
  EXPECT_FLOAT_EQ(h.profile_id.state, 1.0f);
  EXPECT_FALSE(h.impedance_high.has_state());
  EXPECT_FALSE(h.stabilized.state);
}

TEST(XiaomiBodyScale, HighImpedancePacketCompletesTheMeasurement) {
  Harness h(SCALE_A, KEY_A);
  ASSERT_TRUE(h.scale.parse_device(advert(SCALE_A, PACKET_2)));
  EXPECT_FLOAT_EQ(h.impedance_high.state, 497.6f);
  EXPECT_FALSE(h.impedance_low.has_state());
  EXPECT_FALSE(h.weight.has_state());
  EXPECT_TRUE(h.stabilized.state);
}

TEST(XiaomiBodyScale, StabilizedClearsAfterOneSecond) {
  Harness h(SCALE_A, KEY_A);
  ASSERT_TRUE(h.scale.parse_device(advert(SCALE_A, PACKET_2)));
  ASSERT_TRUE(h.stabilized.state);
  const uint32_t start = millis();
  while (h.stabilized.state && millis() - start < 2000) {
    App.scheduler.call(millis());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_FALSE(h.stabilized.state);
  EXPECT_GE(millis() - start, 900u);
}

TEST(XiaomiBodyScale, WeightWithoutImpedanceCompletesTheMeasurement) {
  Harness h(SCALE_B, KEY_B);
  ASSERT_TRUE(h.scale.parse_device(advert(SCALE_B, SOCKS)));
  EXPECT_FLOAT_EQ(h.weight.state, 74.7f);
  EXPECT_FALSE(h.impedance_low.has_state());
  EXPECT_FALSE(h.impedance_high.has_state());
  EXPECT_TRUE(h.stabilized.state);
}

TEST(XiaomiBodyScale, SteppingOffClearsStabilized) {
  Harness h(SCALE_B, KEY_B);
  ASSERT_TRUE(h.scale.parse_device(advert(SCALE_B, SOCKS)));
  ASSERT_TRUE(h.scale.parse_device(advert(SCALE_B, STEP_OFF)));
  EXPECT_FALSE(h.stabilized.state);
  EXPECT_FLOAT_EQ(h.weight.state, 74.7f);  // a zero weight is not published
}

TEST(XiaomiBodyScale, DecodesS200Weight) {
  Harness h(SCALE_S200, KEY_S200);
  ASSERT_TRUE(h.scale.parse_device(advert(SCALE_S200, S200_WEIGHT)));
  EXPECT_FLOAT_EQ(h.weight.state, 62.25f);
  EXPECT_FLOAT_EQ(h.profile_id.state, 1.0f);
  // The S200 has no impedance or heart rate, and does not drive stabilized
  EXPECT_FALSE(h.impedance_low.has_state());
  EXPECT_FALSE(h.impedance_high.has_state());
  EXPECT_FALSE(h.heart_rate.has_state());
  EXPECT_FALSE(h.stabilized.has_state());
}

TEST(XiaomiBodyScale, IgnoresOtherAddresses) {
  Harness h(SCALE_A, KEY_A);
  EXPECT_FALSE(h.scale.parse_device(advert(SCALE_B, PACKET_1)));
  EXPECT_FALSE(h.weight.has_state());
}

TEST(XiaomiBodyScale, RejectsAWrongBindkey) {
  Harness h(SCALE_A, KEY_B);
  EXPECT_FALSE(h.scale.parse_device(advert(SCALE_A, PACKET_1)));
  EXPECT_FALSE(h.weight.has_state());
}

TEST(XiaomiBodyScale, RejectsAPlaintextFrame) {
  Harness h(SCALE_A, KEY_A);
  Frame plain = PACKET_1;
  plain[0] &= ~0x08;
  EXPECT_FALSE(h.scale.parse_device(advert(SCALE_A, plain)));
  EXPECT_FALSE(h.weight.has_state());
}

TEST(XiaomiBodyScale, IgnoresARepeatedFrame) {
  Harness h(SCALE_A, KEY_A);
  ASSERT_TRUE(h.scale.parse_device(advert(SCALE_A, PACKET_1)));
  EXPECT_FALSE(h.scale.parse_device(advert(SCALE_A, PACKET_1)));
}

TEST(XiaomiBodyScale, AcceptsFrameCountFFAsTheFirstFrame) {
  Harness h(SCALE_A, KEY_A);
  ASSERT_TRUE(h.scale.parse_device(advert(SCALE_A, PACKET_1_COUNT_FF)));
  EXPECT_FLOAT_EQ(h.weight.state, 69.9f);
}

TEST(XiaomiBodyScale, AFailedFrameDoesNotBlockTheRealOne) {
  Harness h(SCALE_A, KEY_A);
  Frame forged = PACKET_1;
  forged[23] ^= 0xFF;  // corrupt the tag, same frame count
  EXPECT_FALSE(h.scale.parse_device(advert(SCALE_A, forged)));
  EXPECT_TRUE(h.scale.parse_device(advert(SCALE_A, PACKET_1)));
}

}  // namespace esphome::xiaomi_body_scale::testing
