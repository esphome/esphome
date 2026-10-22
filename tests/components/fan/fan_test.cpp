#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/fan/fan.h"

namespace esphome::fan::testing {

class TestFan : public Fan {
 public:
  FanTraits get_traits() override {
    FanTraits traits(false, true, false, 3);
    this->wire_preset_modes_(traits);
    return traits;
  }

 protected:
  void control(const FanCall &call) override {}
};

static constexpr const char *const PRESET_TABLE[] = {"Eco", "Sleep"};

TEST(FanPresetModesTest, StaticTableIsViewedNotCopied) {
  TestFan fan;
  fan.set_supported_preset_modes_static(PRESET_TABLE, 2);
  auto traits = fan.get_traits();
  ASSERT_EQ(traits.supported_preset_modes().size(), 2u);
  EXPECT_EQ(traits.supported_preset_modes().data(), PRESET_TABLE);
  EXPECT_TRUE(traits.supports_preset_modes());
  EXPECT_EQ(traits.find_preset_mode("Sleep"), PRESET_TABLE[1]);
  EXPECT_EQ(traits.find_preset_mode("Turbo"), nullptr);
}

TEST(FanPresetModesTest, RuntimeListsAreCopiedAndReplaced) {
  TestFan fan;
  std::vector<const char *> modes{"Low", "High"};
  fan.set_supported_preset_modes(modes);
  modes[0] = "Changed";  // the fan keeps its own copy
  EXPECT_STREQ(fan.get_traits().supported_preset_modes()[0], "Low");
  fan.set_supported_preset_modes({});
  EXPECT_FALSE(fan.get_traits().supports_preset_modes());
}

TEST(FanPresetModesTest, CopyFromAnotherFanKeepsItsOwnList) {
  TestFan source;
  source.set_supported_preset_modes({"Auto"});
  TestFan copy;
  copy.set_supported_preset_modes(source.get_traits().supported_preset_modes());
  source.set_supported_preset_modes({"Night"});
  ASSERT_EQ(copy.get_traits().supported_preset_modes().size(), 1u);
  EXPECT_STREQ(copy.get_traits().supported_preset_modes()[0], "Auto");
}

TEST(FanPresetModesTest, TraitsWithoutAFanHaveNoPresets) {
  FanTraits traits;
  EXPECT_TRUE(traits.supported_preset_modes().empty());
  EXPECT_FALSE(traits.supports_preset_modes());
}

}  // namespace esphome::fan::testing
