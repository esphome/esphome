#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/ble_device_base/ble_hub.h"

namespace esphome::ble_device_base::testing {

// Exercises the hub contract around RawAdvertisementCallback, not just the
// struct: a hub stores one slot via set_raw_advertisement_callback(), fires it
// only when set ("no subscriber" is the default-constructed slot), and a new
// registration replaces the old ("one consumer at a time").
//
// The in-tree emit site (BK72xxBLETracker::on_scan_report) compiles against
// the Beken SDK and cannot run host-side, so the guard-and-fire semantics are
// pinned here through a minimal host hub carrying only the slot under test.
namespace {

class FakeHub {
 public:
  void set_raw_advertisement_callback(RawAdvertisementCallback callback) { this->callback_ = callback; }

  /// The emit path every tracker implements: fire only when a subscriber is set.
  void emit(const RawAdvertisement &adv) {
    if (this->callback_.is_set())
      this->callback_.invoke(adv);
  }

 protected:
  RawAdvertisementCallback callback_;  // default-constructed: no subscriber
};

struct CapturingSubscriber {
  RawAdvertisement last{};
  int calls{0};

  static void trampoline(void *self, const RawAdvertisement &adv) {
    auto *sub = static_cast<CapturingSubscriber *>(self);
    sub->last = adv;
    sub->calls++;
  }
};

// Device AA:BB:CC:DD:EE:FF, packed the way the API speaks it.
constexpr uint64_t TEST_ADDRESS = 0xAABBCCDDEEFFULL;
const uint8_t ADV_DATA[4] = {0x02, 0x01, 0x06, 0x00};

RawAdvertisement make_test_adv() {
  return RawAdvertisement{
      .address = TEST_ADDRESS, .data = ADV_DATA, .data_len = sizeof(ADV_DATA), .rssi = -63, .addr_type = 1};
}

}  // namespace

TEST(RawAdvertisementCallback, DefaultConstructedSlotIsNotSet) {
  const RawAdvertisementCallback callback{};
  EXPECT_FALSE(callback.is_set());
}

TEST(RawAdvertisementCallback, SubscriberSeesFieldsUnchanged) {
  FakeHub hub;
  CapturingSubscriber subscriber;
  hub.set_raw_advertisement_callback({&subscriber, CapturingSubscriber::trampoline});

  hub.emit(make_test_adv());

  ASSERT_EQ(subscriber.calls, 1);
  EXPECT_EQ(subscriber.last.address, TEST_ADDRESS);
  EXPECT_EQ(subscriber.last.data, ADV_DATA);
  EXPECT_EQ(subscriber.last.data_len, sizeof(ADV_DATA));
  EXPECT_EQ(subscriber.last.rssi, -63);
  EXPECT_EQ(subscriber.last.addr_type, 1);
}

TEST(RawAdvertisementCallback, NoSubscriberDoesNotFire) {
  FakeHub hub;
  // No set_raw_advertisement_callback(): emitting must be a guarded no-op,
  // not a jump through a garbage pointer.
  hub.emit(make_test_adv());
}

TEST(RawAdvertisementCallback, NewSubscriberReplacesOld) {
  FakeHub hub;
  CapturingSubscriber first;
  CapturingSubscriber second;
  hub.set_raw_advertisement_callback({&first, CapturingSubscriber::trampoline});
  hub.set_raw_advertisement_callback({&second, CapturingSubscriber::trampoline});

  hub.emit(make_test_adv());

  EXPECT_EQ(first.calls, 0);  // one consumer at a time
  ASSERT_EQ(second.calls, 1);
  EXPECT_EQ(second.last.rssi, -63);
}

}  // namespace esphome::ble_device_base::testing
