#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/wifi/expected_disconnect.h"

namespace esphome::wifi::testing {
namespace {

constexpr uint8_t OLD_AP[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
constexpr uint8_t NEW_AP[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};

ExpectedDisconnect armed() {
  ExpectedDisconnect expected;
  expected.arm(OLD_AP);
  return expected;
}

TEST(ExpectedDisconnectTest, PreviousApLeaveIsExpectedOnce) {
  auto expected = armed();

  EXPECT_TRUE(expected.consume_disconnect(OLD_AP, true, false));
  EXPECT_FALSE(expected.is_armed());
  // A second leave from the same AP is a real disconnect
  EXPECT_FALSE(expected.consume_disconnect(OLD_AP, true, false));
}

TEST(ExpectedDisconnectTest, OtherDisconnectsAreRealAndEndTheExpectation) {
  auto other_ap = armed();
  EXPECT_FALSE(other_ap.consume_disconnect(NEW_AP, true, false));
  EXPECT_FALSE(other_ap.is_armed());

  auto other_reason = armed();
  EXPECT_FALSE(other_reason.consume_disconnect(OLD_AP, false, false));
  EXPECT_FALSE(other_reason.is_armed());
}

TEST(ExpectedDisconnectTest, RoamingDisconnectKeepsTheExpectation) {
  auto expected = armed();

  EXPECT_FALSE(expected.consume_disconnect(OLD_AP, false, true));
  EXPECT_TRUE(expected.is_armed());
  EXPECT_TRUE(expected.consume_disconnect(OLD_AP, true, false));
}

TEST(ExpectedDisconnectTest, QueuedConnectionFromPreviousApIsRecognised) {
  auto expected = armed();

  EXPECT_TRUE(expected.is_previous_ap(OLD_AP));
  EXPECT_FALSE(expected.is_previous_ap(NEW_AP));
  EXPECT_TRUE(expected.is_armed());
}

TEST(ExpectedDisconnectTest, NothingIsExpectedUnlessArmed) {
  ExpectedDisconnect expected;
  EXPECT_FALSE(expected.is_previous_ap(OLD_AP));
  EXPECT_FALSE(expected.consume_disconnect(OLD_AP, true, false));

  auto cleared = armed();
  cleared.clear();
  EXPECT_FALSE(cleared.consume_disconnect(OLD_AP, true, false));
}

}  // namespace
}  // namespace esphome::wifi::testing
