#include <gtest/gtest.h>

#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "host_hooks.h"

namespace esphome::wifi::testing {
namespace {

constexpr bssid_t BSSID_A = {0x02, 0x00, 0x00, 0x00, 0x00, 0x0A};
constexpr uint32_t HELD_FOR_MS = 120000;

class WiFiOTAHoldTest : public ::testing::Test {
 protected:
  void SetUp() override { host_hooks = HostHooks{}; }
  void TearDown() override { global_wifi_component = this->previous_global_; }

  /// Brings the AP up the way an AP-only config does.
  void start_ap_() {
    WiFiAP ap;
    ap.set_ssid("fallback");
    this->wifi_.set_ap(ap);
    // A clock jump would otherwise reboot, which exits the host binary
    this->wifi_.set_reboot_timeout(0);
    this->wifi_.start();
    ASSERT_EQ(host_hooks.ap_starts, 1);
  }

  /// Runs a scan that finds the network, then handles its results.
  void scan_finding_network_() {
    host_hooks.scan_results = {make_result("home", BSSID_A)};
    this->wifi_.start_scanning();
    this->wifi_.check_scanning_finished();
  }

  WiFiComponent *previous_global_{global_wifi_component};
  WiFiComponent wifi_;
  ota::UpdateLock upload_;
};

TEST_F(WiFiOTAHoldTest, ScanDuringUploadConnectsOnceItEnds) {
  this->start_ap_();
  this->wifi_.add_sta(make_ap("home"));
  ASSERT_TRUE(this->upload_.try_acquire());

  this->scan_finding_network_();
  EXPECT_EQ(host_hooks.connects, 0);

  this->upload_.release();
  this->wifi_.check_scanning_finished();
  ASSERT_EQ(host_hooks.connects, 1);
  EXPECT_TRUE(host_hooks.last_connect.has_bssid());
  EXPECT_EQ(host_hooks.last_connect.get_bssid(), BSSID_A);
}

TEST_F(WiFiOTAHoldTest, RetryDuringUploadWaitsInCooldown) {
  this->start_ap_();
  this->wifi_.add_sta(make_ap("home"));
  ASSERT_TRUE(this->upload_.try_acquire());

  this->wifi_.retry_connect();
  // Well past any cooldown
  LoopBlockingGuard clock{nullptr, nullptr, millis() + HELD_FOR_MS};
  this->wifi_.loop();
  // The held tick restarted the cooldown
  this->upload_.release();
  this->wifi_.loop();

  EXPECT_EQ(host_hooks.scans, 0);
  EXPECT_EQ(host_hooks.connects, 0);
  EXPECT_EQ(host_hooks.station_offs, 0);
}

TEST_F(WiFiOTAHoldTest, EndedUploadResumesTheRetries) {
  this->start_ap_();
  this->wifi_.add_sta(make_ap("home"));
  host_hooks.scan_results = {make_result("home", BSSID_A)};
  ASSERT_TRUE(this->upload_.try_acquire());
  this->wifi_.retry_connect();
  ASSERT_EQ(host_hooks.scans, 0);

  this->upload_.release();
  for (uint32_t ms = HELD_FOR_MS; ms <= 2 * HELD_FOR_MS && host_hooks.connects == 0; ms += HELD_FOR_MS / 4) {
    LoopBlockingGuard clock{nullptr, nullptr, millis() + ms};
    this->wifi_.loop();
  }

  EXPECT_EQ(host_hooks.scans, 1);
  EXPECT_EQ(host_hooks.connects, 1);
}

TEST_F(WiFiOTAHoldTest, UploadWithoutTheAPDoesNotHold) {
  this->wifi_.add_sta(make_ap("home"));
  ASSERT_TRUE(this->upload_.try_acquire());

  this->scan_finding_network_();

  EXPECT_EQ(host_hooks.connects, 1);
}

}  // namespace
}  // namespace esphome::wifi::testing
