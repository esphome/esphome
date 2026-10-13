#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/ble_device_base/ble_hub.h"

namespace esphome::ble_device_base::testing {

// Pins the ScannerStateCallback slot semantics, mirroring test_raw_callback:
// a default-constructed slot is "no subscriber", a set slot delivers the
// state, and a new registration replaces the old.
namespace {

struct CapturingSubscriber {
  ScannerState last{ScannerState::IDLE};
  int calls{0};

  static void trampoline(void *self, ScannerState state) {
    auto *sub = static_cast<CapturingSubscriber *>(self);
    sub->last = state;
    sub->calls++;
  }
};

}  // namespace

TEST(ScannerStateCallback, DefaultConstructedSlotIsNotSet) {
  const ScannerStateCallback callback{};
  EXPECT_FALSE(callback.is_set());
}

TEST(ScannerStateCallback, SubscriberSeesState) {
  CapturingSubscriber subscriber;
  ScannerStateCallback callback{&subscriber, CapturingSubscriber::trampoline};
  ASSERT_TRUE(callback.is_set());
  callback.invoke(ScannerState::RUNNING);
  EXPECT_EQ(subscriber.calls, 1);
  EXPECT_EQ(subscriber.last, ScannerState::RUNNING);
}

TEST(ScannerStateCallback, NewSubscriberReplacesOld) {
  CapturingSubscriber first;
  CapturingSubscriber second;
  ScannerStateCallback callback{&first, CapturingSubscriber::trampoline};
  callback = {&second, CapturingSubscriber::trampoline};
  callback.invoke(ScannerState::STOPPED);
  EXPECT_EQ(first.calls, 0);
  EXPECT_EQ(second.calls, 1);
  EXPECT_EQ(second.last, ScannerState::STOPPED);
}

}  // namespace esphome::ble_device_base::testing
