#include <gtest/gtest.h>

#include <algorithm>
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

// Stands in for esp_wifi_sta_get_ap_info() and esp_wifi_disconnect(): the AP record is gone after disconnecting
struct Driver {
  bool has_ap{true};
  bool disconnect_ok{true};
  const uint8_t *ap{OLD_AP};

  bool read_bssid(uint8_t *out) const {
    if (!this->has_ap)
      return false;
    std::copy_n(this->ap, 6, out);
    return true;
  }
  bool disconnect() {
    this->has_ap = false;
    return this->disconnect_ok;
  }
};

bool disconnect(ExpectedDisconnect &expected, Driver &driver) {
  return expected.disconnect_and_expect_leave([&driver](uint8_t *out) { return driver.read_bssid(out); },
                                              [&driver]() { return driver.disconnect(); });
}

TEST(ExpectedDisconnectTest, DisconnectExpectsTheLeaveOfTheApItLeft) {
  ExpectedDisconnect expected;
  Driver driver;

  EXPECT_TRUE(disconnect(expected, driver));
  EXPECT_TRUE(expected.consume_disconnect(OLD_AP, true));
}

TEST(ExpectedDisconnectTest, NoApMeansNoExpectation) {
  ExpectedDisconnect expected;
  Driver driver;
  driver.has_ap = false;

  EXPECT_TRUE(disconnect(expected, driver));
  EXPECT_FALSE(expected.consume_disconnect(OLD_AP, true));
}

TEST(ExpectedDisconnectTest, FailedDisconnectExpectsNothing) {
  auto expected = armed();
  Driver driver;
  driver.ap = NEW_AP;
  driver.disconnect_ok = false;

  EXPECT_FALSE(disconnect(expected, driver));
  // The earlier expectation is dropped too
  EXPECT_FALSE(expected.is_previous_ap(OLD_AP));
  EXPECT_FALSE(expected.is_previous_ap(NEW_AP));
}

TEST(AttemptSsidTest, NothingIsOtherBeforeTheFirstAttempt) {
  AttemptSsid attempt;
  EXPECT_FALSE(attempt.is_other(reinterpret_cast<const uint8_t *>("home"), 4));
}

TEST(AttemptSsidTest, OnlyAnotherSsidIsFromAnEarlierAttempt) {
  AttemptSsid attempt;
  attempt.set(reinterpret_cast<const uint8_t *>("home"), 4);
  EXPECT_FALSE(attempt.is_other(reinterpret_cast<const uint8_t *>("home"), 4));
  // The hidden network tried before, reported after the real attempt started
  EXPECT_TRUE(attempt.is_other(reinterpret_cast<const uint8_t *>("hidden"), 6));
  EXPECT_TRUE(attempt.is_other(reinterpret_cast<const uint8_t *>("hom"), 3));
  EXPECT_TRUE(attempt.is_other(reinterpret_cast<const uint8_t *>("homE"), 4));
}

TEST(AttemptSsidTest, FullLengthSsidWithoutTerminator) {
  uint8_t ssid[32];
  std::fill(std::begin(ssid), std::end(ssid), 'a');
  AttemptSsid attempt;
  attempt.set(ssid, sizeof(ssid));
  EXPECT_FALSE(attempt.is_other(ssid, sizeof(ssid)));
  ssid[31] = 'b';
  EXPECT_TRUE(attempt.is_other(ssid, sizeof(ssid)));
}

}  // namespace
}  // namespace esphome::wifi::testing
