#include <gtest/gtest.h>

#include "esphome/components/pid/pid_climate.h"
#include "esphome/core/base_automation.h"

namespace esphome::pid {

namespace {

class NullOutput : public output::FloatOutput {
 protected:
  void write_state(float state) override {}
};

constexpr PIDPresetEntry PRESETS[] = {
    {climate::CLIMATE_PRESET_HOME, 22.0f},
    {climate::CLIMATE_PRESET_SLEEP, 19.0f},
};
constexpr PIDCustomPresetEntry CUSTOM_PRESETS[] = {
    {"Frost protection", 7.0f},
};
constexpr const char *CUSTOM_PRESET_NAMES[] = {"Frost protection"};

class PIDClimatePresets : public ::testing::Test {
 protected:
  void SetUp() override {
    this->climate_.set_heat_output(&this->heat_);
    this->climate_.set_preset_config(PRESETS, 2);
    this->climate_.set_custom_preset_config(CUSTOM_PRESETS, 1);
    this->climate_.set_supported_custom_presets_static(CUSTOM_PRESET_NAMES, 1);
    this->climate_.mode = climate::CLIMATE_MODE_HEAT;
    this->climate_.target_temperature = 21.0f;
  }

  NullOutput heat_;
  PIDClimate climate_;
};

/// A controller as one device boots it; a second Room with the same restore mode reads what the first one saved.
struct Room {
  explicit Room(OnBootRestoreFrom restore_from = OnBootRestoreFrom::ON_BOOT_RESTORE_FROM_MEMORY) {
    this->climate.set_heat_output(&this->heat);
    this->climate.set_sensor(&this->sensor);
    this->climate.set_default_target_temperature(21.0f);
    this->climate.set_preset_config(PRESETS, 2);
    this->climate.set_custom_preset_config(CUSTOM_PRESETS, 1);
    this->climate.set_supported_custom_presets_static(CUSTOM_PRESET_NAMES, 1);
    this->climate.set_on_boot_restore_from(restore_from);
    this->climate.add_on_preset_change_callback([this]() { this->preset_changes++; });
    this->climate.add_on_control_callback([this](climate::ClimateCall &) { this->controls++; });
  }

  NullOutput heat;
  sensor::Sensor sensor;
  PIDClimate climate;
  int preset_changes{0};
  int controls{0};
};

}  // namespace

TEST(PIDClimatePresetTraits, NoPresetsConfiguredDeclaresNone) {
  NullOutput heat;
  PIDClimate climate;
  climate.set_heat_output(&heat);

  EXPECT_FALSE(climate.get_traits().get_supports_presets());
}

TEST_F(PIDClimatePresets, TraitsDeclareConfiguredPresets) {
  auto traits = this->climate_.get_traits();

  EXPECT_FALSE(traits.supports_preset(climate::CLIMATE_PRESET_NONE));
  EXPECT_TRUE(traits.supports_preset(climate::CLIMATE_PRESET_HOME));
  EXPECT_TRUE(traits.supports_preset(climate::CLIMATE_PRESET_SLEEP));
  EXPECT_FALSE(traits.supports_preset(climate::CLIMATE_PRESET_ECO));
  EXPECT_TRUE(traits.supports_custom_preset("Frost protection"));
}

TEST_F(PIDClimatePresets, PresetSetsTarget) {
  this->climate_.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();

  EXPECT_EQ(this->climate_.preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_FLOAT_EQ(this->climate_.target_temperature, 19.0f);
  EXPECT_EQ(this->climate_.mode, climate::CLIMATE_MODE_HEAT);
}

TEST_F(PIDClimatePresets, CustomPresetSetsTarget) {
  this->climate_.make_call().set_preset("Frost protection").perform();

  EXPECT_EQ(this->climate_.get_custom_preset(), "Frost protection");
  EXPECT_FALSE(this->climate_.preset.has_value());
  EXPECT_FLOAT_EQ(this->climate_.target_temperature, 7.0f);
  EXPECT_EQ(this->climate_.mode, climate::CLIMATE_MODE_HEAT);
}

TEST_F(PIDClimatePresets, ManualTargetKeepsPreset) {
  this->climate_.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();
  this->climate_.make_call().set_target_temperature(20.0f).perform();

  EXPECT_EQ(this->climate_.preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_FLOAT_EQ(this->climate_.target_temperature, 20.0f);
}

TEST_F(PIDClimatePresets, ManualModeKeepsPreset) {
  this->climate_.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();
  this->climate_.make_call().set_mode(climate::CLIMATE_MODE_OFF).perform();

  EXPECT_EQ(this->climate_.preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_EQ(this->climate_.mode, climate::CLIMATE_MODE_OFF);
}

TEST_F(PIDClimatePresets, ReselectingPresetRestoresItsTarget) {
  this->climate_.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();
  this->climate_.make_call().set_target_temperature(20.0f).perform();
  this->climate_.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();

  EXPECT_FLOAT_EQ(this->climate_.target_temperature, 19.0f);
}

// A restore call carries preset, mode and target together; the saved values must survive.
TEST_F(PIDClimatePresets, RestoreKeepsSavedModeAndTarget) {
  this->climate_.make_call()
      .set_preset("Frost protection")
      .set_mode(climate::CLIMATE_MODE_OFF)
      .set_target_temperature(9.0f)
      .perform();

  EXPECT_EQ(this->climate_.get_custom_preset(), "Frost protection");
  EXPECT_EQ(this->climate_.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_FLOAT_EQ(this->climate_.target_temperature, 9.0f);
}

TEST(PIDClimatePresetBoot, DefaultPresetAppliedOnFirstBoot) {
  Room room;
  room.climate.set_default_preset(climate::CLIMATE_PRESET_SLEEP);
  room.climate.setup();

  EXPECT_EQ(room.climate.preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_FLOAT_EQ(room.climate.target_temperature, 19.0f);
  EXPECT_EQ(room.preset_changes, 1);
  EXPECT_EQ(room.controls, 0);
}

TEST(PIDClimatePresetBoot, DefaultCustomPresetAppliedOnFirstBoot) {
  Room room;
  room.climate.set_default_preset("Frost protection");
  room.climate.setup();

  EXPECT_EQ(room.climate.get_custom_preset(), "Frost protection");
  EXPECT_FLOAT_EQ(room.climate.target_temperature, 7.0f);
  EXPECT_EQ(room.preset_changes, 1);
}

TEST(PIDClimatePresetBoot, NoDefaultPresetUsesDefaultTarget) {
  Room room;
  room.climate.setup();

  EXPECT_FALSE(room.climate.preset.has_value());
  EXPECT_FLOAT_EQ(room.climate.target_temperature, 21.0f);
  EXPECT_EQ(room.preset_changes, 0);
}

TEST(PIDClimatePresetBoot, RestoreKeepsSavedPresetModeAndTarget) {
  {
    Room before;
    before.climate.set_default_preset(climate::CLIMATE_PRESET_HOME);
    before.climate.setup();
    before.climate.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();
    before.climate.make_call().set_target_temperature(20.0f).set_mode(climate::CLIMATE_MODE_OFF).perform();
  }
  Room room;
  room.climate.set_default_preset(climate::CLIMATE_PRESET_HOME);
  room.climate.setup();

  EXPECT_EQ(room.climate.preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_FLOAT_EQ(room.climate.target_temperature, 20.0f);
  EXPECT_EQ(room.climate.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(room.preset_changes, 0);
}

TEST(PIDClimatePresetBoot, RestoreKeepsSavedCustomPreset) {
  {
    Room before;
    before.climate.setup();
    before.climate.make_call().set_preset("Frost protection").perform();
    before.climate.make_call().set_target_temperature(8.0f).perform();
  }
  Room room;
  room.climate.setup();

  EXPECT_EQ(room.climate.get_custom_preset(), "Frost protection");
  EXPECT_FLOAT_EQ(room.climate.target_temperature, 8.0f);
  EXPECT_EQ(room.preset_changes, 0);
}

TEST(PIDClimatePresetBoot, RestoreFromDefaultPresetIgnoresSavedState) {
  {
    Room before;
    before.climate.setup();
    before.climate.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();
  }
  Room room(OnBootRestoreFrom::ON_BOOT_RESTORE_FROM_DEFAULT_PRESET);
  room.climate.set_default_preset(climate::CLIMATE_PRESET_HOME);
  room.climate.setup();

  EXPECT_EQ(room.climate.preset, climate::CLIMATE_PRESET_HOME);
  EXPECT_FLOAT_EQ(room.climate.target_temperature, 22.0f);
}

TEST(PIDClimatePresetBoot, PresetChangeFiresOnlyOnChange) {
  Room room;
  room.climate.setup();

  room.climate.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();
  EXPECT_EQ(room.preset_changes, 1);
  room.climate.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();
  EXPECT_EQ(room.preset_changes, 1);
  room.climate.make_call().set_target_temperature(20.0f).perform();
  EXPECT_EQ(room.preset_changes, 1);
  // The call's own target matches the current one, so nothing changes.
  room.climate.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).set_target_temperature(20.0f).perform();
  EXPECT_EQ(room.preset_changes, 1);
  // Reselecting restores the preset's target, which is a change.
  room.climate.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).perform();
  EXPECT_EQ(room.preset_changes, 2);
  room.climate.make_call().set_preset("Frost protection").perform();
  EXPECT_EQ(room.preset_changes, 3);
}

TEST(PIDClimatePresetBoot, PresetChangeSeesTheAppliedState) {
  Room room;
  room.climate.setup();
  float seen_target = 0.0f;
  room.climate.add_on_preset_change_callback([&]() { seen_target = room.climate.target_temperature; });

  room.climate.make_call().set_preset(climate::CLIMATE_PRESET_SLEEP).set_target_temperature(20.5f).perform();

  EXPECT_FLOAT_EQ(seen_target, 20.5f);
}

}  // namespace esphome::pid
