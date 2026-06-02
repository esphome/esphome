#include "../common.h"

namespace esphome::mitsubishi_cn105::testing {

struct VerticalVaneDirectionSelectTestContext {
  TestableMitsubishiCN105Climate climate;
  MitsubishiCN105VerticalVaneDirectionSelect select;

  VerticalVaneDirectionSelectTestContext() {
    this->select.traits.set_options({"Auto", "1", "2", "3", "4", "5", "Swing"});
    this->select.set_parent(&this->climate);
    this->climate.set_vertical_vane_direction_select(&this->select);
  }
};

TEST(MitsubishiCN105VerticalVaneDirectionSelectTests, MapsIndexesToVaneModes) {
  VerticalVaneDirectionSelectTestContext ctx;

  constexpr std::array expected_modes{
      MitsubishiCN105::VaneMode::AUTO,       MitsubishiCN105::VaneMode::POSITION_1,
      MitsubishiCN105::VaneMode::POSITION_2, MitsubishiCN105::VaneMode::POSITION_3,
      MitsubishiCN105::VaneMode::POSITION_4, MitsubishiCN105::VaneMode::POSITION_5,
      MitsubishiCN105::VaneMode::SWING,
  };

  for (size_t i = 0; i < expected_modes.size(); ++i) {
    SCOPED_TRACE(i);
    ctx.select.control(i);
    EXPECT_EQ(ctx.climate.status().vane_mode, expected_modes[i]);
    EXPECT_TRUE(ctx.climate.is_vane_update_pending());
  }
}

TEST(MitsubishiCN105VerticalVaneDirectionSelectTests, PublishesIncomingVaneModes) {
  VerticalVaneDirectionSelectTestContext ctx;

  size_t publish_count = 0;
  ctx.climate.add_on_state_callback([&publish_count](climate::Climate &) { publish_count++; });

  constexpr std::array modes{
      MitsubishiCN105::VaneMode::AUTO,       MitsubishiCN105::VaneMode::POSITION_1,
      MitsubishiCN105::VaneMode::POSITION_2, MitsubishiCN105::VaneMode::POSITION_3,
      MitsubishiCN105::VaneMode::POSITION_4, MitsubishiCN105::VaneMode::POSITION_5,
      MitsubishiCN105::VaneMode::SWING,
  };

  for (size_t i = 0; i < modes.size(); ++i) {
    SCOPED_TRACE(i);
    ctx.climate.status().vane_mode = modes[i];
    ctx.climate.apply_values_();
    ASSERT_TRUE(ctx.select.active_index().has_value());
    EXPECT_EQ(*ctx.select.active_index(), i);
    EXPECT_EQ(publish_count, i + 1);
  }

  ctx.climate.status().vane_mode = MitsubishiCN105::VaneMode::UNKNOWN;
  ctx.climate.apply_values_();
  ASSERT_TRUE(ctx.select.active_index().has_value());
  EXPECT_EQ(*ctx.select.active_index(), modes.size() - 1);
}

TEST(MitsubishiCN105VerticalVaneDirectionSelectTests, BeforeInitializationDoesNotPublishClimateState) {
  VerticalVaneDirectionSelectTestContext ctx;

  size_t publish_count = 0;
  ctx.climate.add_on_state_callback([&publish_count](climate::Climate &) { publish_count++; });

  ctx.select.control(3);

  EXPECT_EQ(ctx.climate.status().vane_mode, MitsubishiCN105::VaneMode::POSITION_3);
  EXPECT_TRUE(ctx.climate.is_vane_update_pending());
  EXPECT_FALSE(ctx.select.has_state());
  EXPECT_EQ(publish_count, 0);
}

}  // namespace esphome::mitsubishi_cn105::testing
