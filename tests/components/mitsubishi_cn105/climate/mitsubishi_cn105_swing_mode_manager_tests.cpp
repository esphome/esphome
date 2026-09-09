#include "../common.h"

#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105_swing_mode_manager.h"

namespace esphome::mitsubishi_cn105::testing {

static SwingModeManager make_swing_mode_manager(std::initializer_list<climate::ClimateSwingMode> supported_modes) {
  SwingModeManager manager;
  climate::ClimateSwingModeMask supported_swing_modes;
  for (const auto mode : supported_modes)
    supported_swing_modes.insert(mode);
  manager.set_supported_swing_modes(supported_swing_modes);
  return manager;
}

TEST(SwingModeManagerTests, StatusMapsVerticalSwingWhenSupported) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});
  EXPECT_EQ(manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::CENTER),
            std::optional{climate::CLIMATE_SWING_VERTICAL});
}

TEST(SwingModeManagerTests, StatusMapsHorizontalSwingWhenSupported) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL});
  EXPECT_EQ(manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::AUTO, MitsubishiCN105::WideVaneMode::SWING),
            std::optional{climate::CLIMATE_SWING_HORIZONTAL});
}

TEST(SwingModeManagerTests, StatusMapsBothSwingWhenSupported) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});
  EXPECT_EQ(manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::SWING),
            std::optional{climate::CLIMATE_SWING_BOTH});
}

TEST(SwingModeManagerTests, StatusMapsSwingOffWhenNoSwingActive) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});
  EXPECT_EQ(
      manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::POSITION_3, MitsubishiCN105::WideVaneMode::CENTER),
      std::optional{climate::CLIMATE_SWING_OFF});
}

TEST(SwingModeManagerTests, RemembersLastNonSwingPositions) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});
  manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::POSITION_4, MitsubishiCN105::WideVaneMode::RIGHT);
  manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::SWING);
  EXPECT_EQ(manager.vane_from(climate::CLIMATE_SWING_OFF), std::optional{MitsubishiCN105::VaneMode::POSITION_4});
  EXPECT_EQ(manager.wide_vane_from(climate::CLIMATE_SWING_OFF), std::optional{MitsubishiCN105::WideVaneMode::RIGHT});
}

TEST(SwingModeManagerTests, UnknownValuesDoNotOverwriteRememberedPositions) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});
  manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::POSITION_2, MitsubishiCN105::WideVaneMode::LEFT);
  manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::UNKNOWN, MitsubishiCN105::WideVaneMode::UNKNOWN);
  EXPECT_EQ(manager.vane_from(climate::CLIMATE_SWING_OFF), std::optional{MitsubishiCN105::VaneMode::POSITION_2});
  EXPECT_EQ(manager.wide_vane_from(climate::CLIMATE_SWING_OFF), std::optional{MitsubishiCN105::WideVaneMode::LEFT});
}

TEST(SwingModeManagerTests, UnsupportedVerticalSwingStateIsIgnored) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL});
  EXPECT_EQ(manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::CENTER),
            std::optional{climate::CLIMATE_SWING_OFF});
}

TEST(SwingModeManagerTests, UnsupportedHorizontalSwingStateIsIgnored) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});
  EXPECT_EQ(manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::AUTO, MitsubishiCN105::WideVaneMode::SWING),
            std::optional{climate::CLIMATE_SWING_OFF});
}

TEST(SwingModeManagerTests, SwingModeFromReturnsNulloptWhenNoSwingModesSupported) {
  auto manager = make_swing_mode_manager({});
  EXPECT_FALSE(manager.update_and_get_swing_mode(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::SWING)
                   .has_value());
}

TEST(SwingModeManagerTests, VaneFromSwingModeReturnsNulloptWhenVerticalUnsupported) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL});
  EXPECT_FALSE(manager.vane_from(climate::CLIMATE_SWING_VERTICAL).has_value());
}

TEST(SwingModeManagerTests, WideVaneFromSwingModeReturnsNulloptWhenHorizontalUnsupported) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});
  EXPECT_FALSE(manager.wide_vane_from(climate::CLIMATE_SWING_HORIZONTAL).has_value());
}

TEST(SwingModeManagerTests, VaneAndWideVaneFromSwingModeMapSwingModes) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});
  EXPECT_EQ(manager.vane_from(climate::CLIMATE_SWING_VERTICAL), std::optional{MitsubishiCN105::VaneMode::SWING});
  EXPECT_EQ(manager.vane_from(climate::CLIMATE_SWING_BOTH), std::optional{MitsubishiCN105::VaneMode::SWING});
  EXPECT_EQ(manager.wide_vane_from(climate::CLIMATE_SWING_HORIZONTAL),
            std::optional{MitsubishiCN105::WideVaneMode::SWING});
  EXPECT_EQ(manager.wide_vane_from(climate::CLIMATE_SWING_BOTH), std::optional{MitsubishiCN105::WideVaneMode::SWING});
}

}  // namespace esphome::mitsubishi_cn105::testing
