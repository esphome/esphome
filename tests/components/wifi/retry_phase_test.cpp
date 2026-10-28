#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/wifi/wifi_component.h"
#include "host_hooks.h"

namespace esphome::wifi::testing {

/// Reaches the retry state machine; WiFiComponent is final, so a friend instead of a subclass.
class WiFiComponentAccess {
 public:
  explicit WiFiComponentAccess(WiFiComponent &wifi) : wifi_(wifi) {}

  /// Finishes a scan that returned these results, as the platform scan-done callback would.
  void finish_scan(std::initializer_list<WiFiScanResult> results) {
    this->wifi_.state_ = WIFI_COMPONENT_STATE_STA_SCANNING;
    this->wifi_.scan_result_ = results;
    this->wifi_.scan_done_ = true;
    this->wifi_.check_scanning_finished();
  }

  WiFiComponentState state() const { return this->wifi_.state_; }
  WiFiRetryPhase phase() const { return this->wifi_.retry_phase_; }

 private:
  WiFiComponent &wifi_;
};

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
  void SetUp() override { host_hooks = HostHooks{}; }
  // The constructor points the global at this instance, which the other components' tests must not see
  void TearDown() override { global_wifi_component = this->previous_global_; }

  void add_network(const WiFiAP &ap) {
    this->wifi_.init_sta(1);
    this->wifi_.add_sta(ap);
  }

  WiFiComponent *previous_global_{global_wifi_component};
  WiFiComponent wifi_;
  WiFiComponentAccess access_{wifi_};
};

TEST_F(WiFiRetryPhaseTest, ScanThatFindsTheNetworkConnectsToItsBssid) {
  this->add_network(make_ap("home"));

  this->access_.finish_scan({make_result("home", BSSID_A)});

  EXPECT_EQ(this->access_.phase(), WiFiRetryPhase::SCAN_CONNECTING);
  EXPECT_EQ(this->access_.state(), WIFI_COMPONENT_STATE_STA_CONNECTING);
  ASSERT_EQ(host_hooks.connects, 1);
  EXPECT_TRUE(host_hooks.last_connect.has_bssid());
  EXPECT_EQ(host_hooks.last_connect.get_bssid(), BSSID_A);
}

TEST_F(WiFiRetryPhaseTest, NetworkMissingFromScanIsTriedAsHidden) {
  this->add_network(make_ap("home"));

  this->access_.finish_scan({make_result("neighbour", BSSID_B)});

  EXPECT_EQ(this->access_.phase(), WiFiRetryPhase::RETRY_HIDDEN);
  EXPECT_EQ(this->access_.state(), WIFI_COMPONENT_STATE_STA_CONNECTING);
  ASSERT_EQ(host_hooks.connects, 1);
  EXPECT_FALSE(host_hooks.last_connect.has_bssid());
}

}  // namespace
}  // namespace esphome::wifi::testing
