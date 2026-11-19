#include <gtest/gtest.h>

#include "esphome/components/ble_device_base/automation.h"

namespace esphome::ble_device_base::testing {

namespace {

// The calls ble_device_base::start_scan makes on a tracker
struct FakeTracker {
  bool scan_running() const { return this->running; }
  bool scan_continuous() const { return this->continuous; }
  void set_scan_continuous(bool value) { this->continuous = value; }
  void start_scan() {
    this->running = true;
    this->starts++;
  }
  void restart_scan_duration() { this->restarts++; }

  bool running{false};
  bool continuous{false};
  int starts{0};
  int restarts{0};
};

}  // namespace

TEST(BleStartScan, IdleTrackerStartsInTheRequestedMode) {
  FakeTracker tracker;
  start_scan(&tracker, true);
  EXPECT_TRUE(tracker.continuous);
  EXPECT_EQ(tracker.starts, 1);
  EXPECT_EQ(tracker.restarts, 0);
}

// A repeated action must not keep a one-shot scan alive by re-anchoring its duration
TEST(BleStartScan, RunningScanInTheSameModeIsLeftAlone) {
  FakeTracker tracker;
  start_scan(&tracker, false);
  start_scan(&tracker, false);
  EXPECT_EQ(tracker.starts, 1);
  EXPECT_EQ(tracker.restarts, 0);
}

TEST(BleStartScan, RunningScanSwitchesModeAndRunsAFullDuration) {
  FakeTracker tracker;
  start_scan(&tracker, true);
  start_scan(&tracker, false);
  EXPECT_FALSE(tracker.continuous);
  EXPECT_EQ(tracker.starts, 1);
  EXPECT_EQ(tracker.restarts, 1);
}

}  // namespace esphome::ble_device_base::testing
