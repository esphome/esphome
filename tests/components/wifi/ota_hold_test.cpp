#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "host_hooks.h"

namespace esphome::wifi::testing {
namespace {

constexpr bssid_t BSSID_A = {0x02, 0x00, 0x00, 0x00, 0x00, 0x0A};
// WIFI_COOLDOWN_WITH_AP_ACTIVE_MS plus one
constexpr uint32_t QUIET_COOLDOWN_MS = 30001;

class WiFiOTAHoldTest : public ::testing::Test {
 protected:
  void SetUp() override { host_hooks = HostHooks{}; }
  void TearDown() override { global_wifi_component = this->previous_global_; }

  /// Brings the AP up the way an AP-only config does, then adds the network that retries target.
  void start_ap_with_network_() {
    WiFiAP ap;
    ap.set_ssid("fallback");
    this->wifi_.set_ap(ap);
    // The clock jump in the cooldown test would otherwise reboot, which exits the host binary
    this->wifi_.set_reboot_timeout(0);
    this->wifi_.start();
    ASSERT_EQ(host_hooks.ap_starts, 1);
    WiFiAP sta;
    sta.set_ssid("home");
    sta.set_password("password1");
    this->wifi_.add_sta(sta);
  }

  /// Runs a scan that finds the network, then handles its results.
  void scan_finding_network_() {
    host_hooks.scan_results = {WiFiScanResult(BSSID_A, "home", strlen("home"), 6, -50, true, false)};
    this->wifi_.start_scanning();
    this->wifi_.check_scanning_finished();
  }

  void ota_(ota::OTAState state) { this->wifi_.on_ota_global_state(state, 0.0f, 0, nullptr); }

  WiFiComponent *previous_global_{global_wifi_component};
  WiFiComponent wifi_;
};

TEST_F(WiFiOTAHoldTest, ScanDuringUploadDoesNotConnect) {
  this->start_ap_with_network_();
  this->ota_(ota::OTA_STARTED);

  this->scan_finding_network_();

  EXPECT_EQ(host_hooks.connects, 0);
}

TEST_F(WiFiOTAHoldTest, RetryDuringUploadWaitsInCooldown) {
  this->start_ap_with_network_();
  this->ota_(ota::OTA_IN_PROGRESS);

  this->wifi_.retry_connect();

  EXPECT_EQ(host_hooks.connects, 0);
  EXPECT_EQ(host_hooks.station_offs, 0);
}

// Native OTA reports an error when the web upload already holds the update lock
TEST_F(WiFiOTAHoldTest, ErrorFromAnotherUploadKeepsTheHold) {
  this->start_ap_with_network_();
  this->ota_(ota::OTA_STARTED);
  this->ota_(ota::OTA_ERROR);

  this->scan_finding_network_();

  EXPECT_EQ(host_hooks.connects, 0);
}

TEST_F(WiFiOTAHoldTest, QuietCooldownReleasesTheHold) {
  this->start_ap_with_network_();
  this->ota_(ota::OTA_STARTED);
  this->wifi_.retry_connect();

  LoopBlockingGuard clock{nullptr, nullptr, millis() + QUIET_COOLDOWN_MS};
  this->wifi_.loop();
  this->scan_finding_network_();

  EXPECT_EQ(host_hooks.connects, 1);
}

TEST_F(WiFiOTAHoldTest, UploadWithoutTheAPDoesNotHold) {
  WiFiAP sta;
  sta.set_ssid("home");
  sta.set_password("password1");
  this->wifi_.add_sta(sta);
  this->ota_(ota::OTA_STARTED);

  this->scan_finding_network_();

  EXPECT_EQ(host_hooks.connects, 1);
}

}  // namespace
}  // namespace esphome::wifi::testing
