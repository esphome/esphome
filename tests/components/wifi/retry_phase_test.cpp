#include <gtest/gtest.h>

#include <cstring>
#include <utility>
#include <vector>

#include "esphome/components/wifi/wifi_component.h"
#include "host_hooks.h"

namespace esphome::wifi::testing {
namespace {

constexpr bssid_t BSSID_A = {0x02, 0x00, 0x00, 0x00, 0x00, 0x0A};
constexpr bssid_t BSSID_B = {0x02, 0x00, 0x00, 0x00, 0x00, 0x0B};

WiFiAP make_ap(const char *ssid) {
  WiFiAP ap;
  ap.set_ssid(ssid);
  ap.set_password("password1");
  return ap;
}

WiFiScanResult make_result(const char *ssid, const bssid_t &bssid) {
  return WiFiScanResult(bssid, ssid, strlen(ssid), 6, -50, true, false);
}

class WiFiRetryPhaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    host_hooks = HostHooks{};
    this->wifi_.init_sta(4);
  }
  // The constructor points the global at this instance, which the other components' tests must not see
  void TearDown() override { global_wifi_component = this->previous_global_; }

  void add_network_(const WiFiAP &ap) { this->wifi_.add_sta(ap); }

  /// Runs a scan that finds these networks, then handles its results.
  void scan_(std::vector<WiFiScanResult> results) {
    host_hooks.scan_results = std::move(results);
    this->wifi_.start_scanning();
    this->wifi_.check_scanning_finished();
  }

  WiFiComponent *previous_global_{global_wifi_component};
  WiFiComponent wifi_;
};

TEST_F(WiFiRetryPhaseTest, ScanThatFindsTheNetworkConnectsToItsBssid) {
  this->add_network_(make_ap("home"));

  this->scan_({make_result("home", BSSID_A)});

  ASSERT_EQ(host_hooks.connects, 1);
  EXPECT_TRUE(host_hooks.last_connect.has_bssid());
  EXPECT_EQ(host_hooks.last_connect.get_bssid(), BSSID_A);
}

TEST_F(WiFiRetryPhaseTest, NetworkMissingFromScanIsTriedAsHidden) {
  this->add_network_(make_ap("home"));

  this->scan_({make_result("neighbour", BSSID_B)});

  ASSERT_EQ(host_hooks.connects, 1);
  EXPECT_EQ(host_hooks.last_connect.get_ssid(), "home");
  EXPECT_FALSE(host_hooks.last_connect.has_bssid());
}

TEST_F(WiFiRetryPhaseTest, PinnedBssidMissingFromScanRestartsAtOnce) {
  WiFiAP ap = make_ap("home");
  ap.set_bssid(BSSID_A);
  this->add_network(ap);

  // The SSID is visible, but on another access point, so no hidden network is left to try
  this->scan({make_result("home", BSSID_B)});

  // The adapter restarts now instead of after the scan timeout
  EXPECT_EQ(host_hooks.station_offs, 1);
  EXPECT_EQ(host_hooks.connects, 0);
}

}  // namespace
}  // namespace esphome::wifi::testing
