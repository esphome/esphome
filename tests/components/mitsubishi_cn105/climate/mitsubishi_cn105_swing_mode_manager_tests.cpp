#include "../common.h"

#include "esphome/components/mitsubishi_cn105/mitsubishi_cn105_swing_mode_manager.h"

namespace esphome::mitsubishi_cn105::testing {

static SwingModeManager make_swing_mode_manager(std::initializer_list<climate::ClimateSwingMode> supported_modes) {
  SwingModeManager manager;
  for (const auto mode : supported_modes) {
    manager.supported_swing_modes().insert(mode);
  }
  return manager;
}

TEST(SwingModeManagerTests, StatusMapsVerticalSwingWhenSupported) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});

  const auto swing_mode =
      manager.swing_mode_from(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::CENTER);

  EXPECT_EQ(swing_mode, std::optional{climate::CLIMATE_SWING_VERTICAL});
}

TEST(SwingModeManagerTests, StatusMapsHorizontalSwingWhenSupported) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL});

  const auto swing_mode =
      manager.swing_mode_from(MitsubishiCN105::VaneMode::AUTO, MitsubishiCN105::WideVaneMode::SWING);

  EXPECT_EQ(swing_mode, std::optional{climate::CLIMATE_SWING_HORIZONTAL});
}

TEST(SwingModeManagerTests, StatusMapsBothSwingWhenSupported) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});

  const auto swing_mode =
      manager.swing_mode_from(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::SWING);

  EXPECT_EQ(swing_mode, std::optional{climate::CLIMATE_SWING_BOTH});
}

TEST(SwingModeManagerTests, StatusMapsSwingOffWhenNoSwingActive) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});

  const auto swing_mode =
      manager.swing_mode_from(MitsubishiCN105::VaneMode::POSITION_3, MitsubishiCN105::WideVaneMode::CENTER);

  EXPECT_EQ(swing_mode, std::optional{climate::CLIMATE_SWING_OFF});
}

TEST(SwingModeManagerTests, RemembersLastNonSwingPositions) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});

  manager.swing_mode_from(MitsubishiCN105::VaneMode::POSITION_4, MitsubishiCN105::WideVaneMode::RIGHT);
  const auto swing_mode =
      manager.swing_mode_from(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::SWING);

  EXPECT_EQ(swing_mode, std::optional{climate::CLIMATE_SWING_BOTH});
  EXPECT_EQ(manager.vane_from(climate::CLIMATE_SWING_OFF), std::optional{MitsubishiCN105::VaneMode::POSITION_4});
  EXPECT_EQ(manager.wide_vane_from(climate::CLIMATE_SWING_OFF), std::optional{MitsubishiCN105::WideVaneMode::RIGHT});
}

TEST(SwingModeManagerTests, UnknownValuesDoNotOverwriteRememberedPositions) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});

  manager.swing_mode_from(MitsubishiCN105::VaneMode::POSITION_2, MitsubishiCN105::WideVaneMode::LEFT);
  const auto swing_mode =
      manager.swing_mode_from(MitsubishiCN105::VaneMode::UNKNOWN, MitsubishiCN105::WideVaneMode::UNKNOWN);

  EXPECT_EQ(swing_mode, std::optional{climate::CLIMATE_SWING_OFF});
  EXPECT_EQ(manager.vane_from(climate::CLIMATE_SWING_OFF), std::optional{MitsubishiCN105::VaneMode::POSITION_2});
  EXPECT_EQ(manager.wide_vane_from(climate::CLIMATE_SWING_OFF), std::optional{MitsubishiCN105::WideVaneMode::LEFT});
}

TEST(SwingModeManagerTests, UnsupportedVerticalSwingStateIsIgnored) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL});

  const auto swing_mode =
      manager.swing_mode_from(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::CENTER);

  EXPECT_EQ(swing_mode, std::optional{climate::CLIMATE_SWING_OFF});
}

TEST(SwingModeManagerTests, UnsupportedHorizontalSwingStateIsIgnored) {
  auto manager = make_swing_mode_manager({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});

  const auto swing_mode =
      manager.swing_mode_from(MitsubishiCN105::VaneMode::AUTO, MitsubishiCN105::WideVaneMode::SWING);

  EXPECT_EQ(swing_mode, std::optional{climate::CLIMATE_SWING_OFF});
}

TEST(SwingModeManagerTests, SwingModeFromReturnsNulloptWhenNoSwingModesSupported) {
  auto manager = make_swing_mode_manager({});

  const auto swing_mode =
      manager.swing_mode_from(MitsubishiCN105::VaneMode::SWING, MitsubishiCN105::WideVaneMode::SWING);

  EXPECT_FALSE(swing_mode.has_value());
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
