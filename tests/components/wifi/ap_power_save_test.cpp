#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/application.h"
#include "host_hooks.h"

namespace esphome::wifi::testing {
namespace {

constexpr uint32_t AP_TIMEOUT_MS = 10000;

class WiFiAPPowerSaveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    host_hooks = HostHooks{};
    WiFiAP ap;
    ap.set_ssid("fallback");
    this->wifi_.set_ap(ap);
  }
  void TearDown() override { global_wifi_component = this->previous_global_; }

  void add_network_() {
    WiFiAP sta;
    sta.set_ssid("home");
    this->wifi_.add_sta(sta);
  }

  WiFiComponent *previous_global_{global_wifi_component};
  WiFiComponent wifi_;
};

// Credentials saved through the portal connect without another start(), so the AP must apply it
TEST_F(WiFiAPPowerSaveTest, APWithoutNetworksAppliesPowerSave) {
  this->wifi_.start();

  EXPECT_EQ(host_hooks.ap_starts, 1);
  EXPECT_EQ(host_hooks.power_save_applies, 1);
}

TEST_F(WiFiAPPowerSaveTest, FailedAPStartDoesNotApplyPowerSave) {
  host_hooks.ap_start_ok = false;

  this->wifi_.start();

  EXPECT_EQ(host_hooks.ap_starts, 1);
  EXPECT_EQ(host_hooks.power_save_applies, 0);
}

TEST_F(WiFiAPPowerSaveTest, FallbackAPLeavesItToTheStationPath) {
  this->add_network_();
  this->wifi_.set_ap_timeout(AP_TIMEOUT_MS);
  this->wifi_.start();
  ASSERT_EQ(host_hooks.power_save_applies, 1);

  LoopBlockingGuard clock{nullptr, nullptr, App.get_loop_component_start_time() + AP_TIMEOUT_MS + 1};
  this->wifi_.loop();

  EXPECT_EQ(host_hooks.ap_starts, 1);
  EXPECT_EQ(host_hooks.power_save_applies, 1);
}

TEST_F(WiFiAPPowerSaveTest, APWithoutNetworksAppliesOutputPower) {
  this->wifi_.set_output_power(17.5f);

  this->wifi_.start();

  EXPECT_EQ(host_hooks.output_powers, std::vector<float>{17.5f});
}

TEST_F(WiFiAPPowerSaveTest, NetworksApplyOutputPower) {
  this->add_network_();
  this->wifi_.set_output_power(17.5f);

  this->wifi_.start();

  EXPECT_EQ(host_hooks.output_powers, std::vector<float>{17.5f});
}

TEST_F(WiFiAPPowerSaveTest, UnsetOutputPowerIsNotApplied) {
  this->wifi_.start();
  this->add_network_();
  this->wifi_.start();

  EXPECT_TRUE(host_hooks.output_powers.empty());
}

}  // namespace
}  // namespace esphome::wifi::testing
