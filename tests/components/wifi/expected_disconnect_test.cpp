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

  EXPECT_TRUE(expected.consume_disconnect(OLD_AP, true));
  EXPECT_FALSE(expected.is_previous_ap(OLD_AP));
  // A second leave from the same AP is a real disconnect
  EXPECT_FALSE(expected.consume_disconnect(OLD_AP, true));
}

TEST(ExpectedDisconnectTest, OtherDisconnectsAreRealAndEndTheExpectation) {
  auto other_ap = armed();
  EXPECT_FALSE(other_ap.consume_disconnect(NEW_AP, true));
  EXPECT_FALSE(other_ap.consume_disconnect(OLD_AP, true));

  auto other_reason = armed();
  EXPECT_FALSE(other_reason.consume_disconnect(OLD_AP, false));
  EXPECT_FALSE(other_reason.consume_disconnect(OLD_AP, true));
}

TEST(ExpectedDisconnectTest, QueuedConnectionFromPreviousApKeepsTheExpectation) {
  auto expected = armed();

  EXPECT_TRUE(expected.is_previous_ap(OLD_AP));
  EXPECT_FALSE(expected.is_previous_ap(NEW_AP));
  EXPECT_TRUE(expected.consume_disconnect(OLD_AP, true));
}

TEST(ExpectedDisconnectTest, NothingIsExpectedUnlessArmed) {
  ExpectedDisconnect expected;
  EXPECT_FALSE(expected.is_previous_ap(OLD_AP));
  EXPECT_FALSE(expected.consume_disconnect(OLD_AP, true));

  auto cleared = armed();
  cleared.clear();
  EXPECT_FALSE(cleared.consume_disconnect(OLD_AP, true));
}

}  // namespace
}  // namespace esphome::wifi::testing
