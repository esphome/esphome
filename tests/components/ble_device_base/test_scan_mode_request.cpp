#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/ble_device_base/ble_hub.h"

namespace esphome::ble_device_base::testing {

// Pins the request_scan_mode() contract: the base default refuses (so hubs
// without a mode switch — and out-of-tree trackers — keep building and
// callers report the real state), while an overriding hub both honors the
// request and applies it.
namespace {

class DefaultHub : public BLEHub {
 public:
  void register_listener(ESPBTDeviceListener *listener) override {}
  void set_raw_advertisement_callback(RawAdvertisementCallback callback) override {}
  HubCapabilities get_capabilities() const override { return {false, false, false}; }
  void get_adapter_mac(uint8_t out[6]) override {}
  bool scan_running() override { return false; }
  // Backed by real state so "changes nothing" is observable: a base default
  // that silently mutated the hub would flip this and fail the assertion.
  bool scan_active() override { return this->active_; }

 protected:
  bool active_{true};
};

class SwitchingHub : public DefaultHub {
 public:
  HubCapabilities get_capabilities() const override { return {true, false, false, /* scan_mode_switch = */ true}; }
  bool request_scan_mode(bool active) override {
    this->active_ = active;
    return true;
  }
};

// The esp32 shape: the controller supports active scanning but the hub keeps
// the refusing default (mode is driven through its own tracker API).
class CapableRefusingHub : public DefaultHub {
 public:
  HubCapabilities get_capabilities() const override { return {true, false, false}; }
};

}  // namespace

TEST(BLEHubScanModeRequest, DefaultRefusesAndChangesNothing) {
  DefaultHub hub;
  EXPECT_TRUE(hub.scan_active());
  EXPECT_FALSE(hub.request_scan_mode(false));
  // Refused, not applied-and-reported-false: the state is untouched.
  EXPECT_TRUE(hub.scan_active());
  EXPECT_FALSE(hub.request_scan_mode(true));
  EXPECT_TRUE(hub.scan_active());
}

TEST(BLEHubScanModeRequest, OverrideHonorsAndApplies) {
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
