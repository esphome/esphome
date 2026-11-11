#include <gtest/gtest.h>

#include "esphome/components/wifi/wifi_component.h"
#include "host_hooks.h"

namespace esphome::wifi::testing {
namespace {

class WiFiAPPowerSaveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    host_hooks = HostHooks{};
    WiFiAP ap;
    ap.set_ssid("fallback");
    this->wifi_.set_ap(ap);
  }
  void TearDown() override { global_wifi_component = this->previous_global_; }

  WiFiComponent *previous_global_{global_wifi_component};
  WiFiComponent wifi_;
};

// Credentials saved through the portal connect without another start(), so the AP must apply it
TEST_F(WiFiAPPowerSaveTest, APWithoutNetworksAppliesPowerSave) {
  this->wifi_.start();

  EXPECT_EQ(host_hooks.power_save_applies, 1);
}

TEST_F(WiFiAPPowerSaveTest, NetworksApplyItOnce) {
  WiFiAP sta;
  sta.set_ssid("home");
  this->wifi_.add_sta(sta);

  this->wifi_.start();

  EXPECT_EQ(host_hooks.power_save_applies, 1);
}

}  // namespace
}  // namespace esphome::wifi::testing
