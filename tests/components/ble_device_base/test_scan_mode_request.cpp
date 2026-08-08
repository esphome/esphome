#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/ble_device_base/ble_hub.h"

namespace esphome::ble_device_base::testing {

// Pins the request_scan_mode() contract documented in ble_hub.h: a hub
// without a mode switch refuses without changing any state (so callers report
// the real state back), while a switching hub both honors the request and
// applies it. The contract is duck-typed (BLEHub is a per-platform alias),
// so the shapes are pinned through minimal host hubs mirroring the in-tree
// tracker stubs.
namespace {

class RefusingHub {
 public:
  static constexpr HubCapabilities get_capabilities() { return {false, false, false}; }
  bool scan_active() { return this->active_; }
  // The refusing shape every non-switching tracker provides: no state touched.
  bool request_scan_mode(bool active) { return false; }

 protected:
  bool active_{true};
};

class SwitchingHub : public RefusingHub {
 public:
  static constexpr HubCapabilities get_capabilities() { return {true, false, false, /* scan_mode_switch = */ true}; }
  bool request_scan_mode(bool active) {
    this->active_ = active;
    return true;
  }
};

// The esp32 shape: the controller supports active scanning but the hub keeps
// the refusing stub (mode is driven through its own tracker API).
class CapableRefusingHub : public RefusingHub {
 public:
  static constexpr HubCapabilities get_capabilities() { return {true, false, false}; }
};

}  // namespace

TEST(BLEHubScanModeRequest, RefusingHubChangesNothing) {
  RefusingHub hub;
  EXPECT_TRUE(hub.scan_active());
  EXPECT_FALSE(hub.request_scan_mode(false));
  // Refused, not applied-and-reported-false: the state is untouched.
  EXPECT_TRUE(hub.scan_active());
  EXPECT_FALSE(hub.request_scan_mode(true));
  EXPECT_TRUE(hub.scan_active());
}

TEST(BLEHubScanModeRequest, SwitchingHubHonorsAndApplies) {
  SwitchingHub hub;
  EXPECT_TRUE(hub.get_capabilities().scan_mode_switch);
  EXPECT_TRUE(hub.request_scan_mode(true));
  EXPECT_TRUE(hub.scan_active());
  EXPECT_TRUE(hub.request_scan_mode(false));
  EXPECT_FALSE(hub.scan_active());
}

TEST(BLEHubScanModeRequest, CapabilityAndSwitchAreIndependent) {
  CapableRefusingHub hub;
  EXPECT_TRUE(hub.get_capabilities().active_scan);
  // The esp32 shape advertises no runtime switch, and the request refuses.
  EXPECT_FALSE(hub.get_capabilities().scan_mode_switch);
  EXPECT_FALSE(hub.request_scan_mode(false));
  EXPECT_TRUE(hub.scan_active());
}

}  // namespace esphome::ble_device_base::testing
