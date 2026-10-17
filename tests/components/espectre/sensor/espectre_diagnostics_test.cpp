#include "gtest/gtest.h"
#include "esphome/components/espectre/sensor/espectre_diagnostics.h"

#ifdef ESPECTRE_SDK_TEST_DOUBLE

namespace esphome::espectre::testing {

TEST(ESPectreDiagnosticsTest, PublishOnUpdate) {
  ESPectreComponent component;
  auto *sdk = ::espectre::RuntimeFrontendController::instance;
  sensor::Sensor accepted;
  sensor::Sensor occupancy;
  DiagnosticsUpdater updater(&component);
  updater.set_csi_accepted_rate_sensor(&accepted);
  updater.set_csi_occupancy_sensor(&occupancy);
  component.setup();
  sdk->diagnostics_sample_.csi_accepted_pps = 98.5f;
  sdk->diagnostics_sample_.csi_occupancy_ratio = 0.9f;
  component.loop();
  EXPECT_FALSE(accepted.has_state());
  updater.update();
  EXPECT_FLOAT_EQ(accepted.state, 98.5f);
  EXPECT_FLOAT_EQ(occupancy.state, 90.0f);
}

}  // namespace esphome::espectre::testing

#endif  // ESPECTRE_SDK_TEST_DOUBLE
