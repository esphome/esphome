#include "../common.h"
#include "esphome/components/mitsubishi_cn105/select/mitsubishi_cn105_vane_select_vertical.h"

namespace esphome::mitsubishi_cn105::testing {

struct VerticalVaneDirectionSelectTestContext {
  MitsubishiCN105Component hub;
  MitsubishiCN105VerticalVaneDirectionSelect select;

  VerticalVaneDirectionSelectTestContext() {
    this->select.traits.set_options({"Auto", "1", "2", "3", "4", "5", "Swing"});
    this->select.set_parent(&this->hub);
    this->select.setup();
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
    ctx.select.make_call().set_index(i).perform();
    EXPECT_EQ(ctx.hub.status().vane_mode, expected_modes[i]);
  }
}

TEST(MitsubishiCN105VerticalVaneDirectionSelectTests, PublishesIncomingVaneModes) {
  VerticalVaneDirectionSelectTestContext ctx;
  ctx.hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  ctx.hub.set_target_temperature(20.0f);

  constexpr std::array modes{
      MitsubishiCN105::VaneMode::AUTO,       MitsubishiCN105::VaneMode::POSITION_1,
      MitsubishiCN105::VaneMode::POSITION_2, MitsubishiCN105::VaneMode::POSITION_3,
      MitsubishiCN105::VaneMode::POSITION_4, MitsubishiCN105::VaneMode::POSITION_5,
      MitsubishiCN105::VaneMode::SWING,
  };

  for (size_t i = 0; i < modes.size(); ++i) {
    SCOPED_TRACE(i);
    ctx.hub.set_vane_mode(modes[i]);
    ctx.hub.publish_status();
    EXPECT_EQ(ctx.select.active_index(), std::optional{i});
  }

  ctx.select.publish_vane_state(MitsubishiCN105::VaneMode::UNKNOWN);
  EXPECT_EQ(ctx.select.active_index(), std::optional{modes.size() - 1});
}

TEST(MitsubishiCN105VerticalVaneDirectionSelectTests, ControlPublishesSelectAndClimateThroughHub) {
  VerticalVaneDirectionSelectTestContext ctx;
  MitsubishiCN105Climate climate_entity;
  climate_entity.set_parent(&ctx.hub);
  climate_entity.set_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);

  ctx.hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  ctx.hub.set_target_temperature(20.0f);
  climate_entity.setup();

  ctx.select.make_call().set_index(6).perform();
  EXPECT_EQ(ctx.select.active_index(), std::optional<size_t>{6});
  EXPECT_EQ(climate_entity.swing_mode, climate::CLIMATE_SWING_VERTICAL);

  ctx.select.make_call().set_index(3).perform();
  EXPECT_EQ(ctx.select.active_index(), std::optional<size_t>{3});
  EXPECT_EQ(climate_entity.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(MitsubishiCN105VerticalVaneDirectionSelectTests, ClimateControlPublishesSelectThroughHub) {
  VerticalVaneDirectionSelectTestContext ctx;
  MitsubishiCN105Climate climate_entity;
  climate_entity.set_parent(&ctx.hub);
  climate_entity.set_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);

  ctx.hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  ctx.hub.set_target_temperature(20.0f);
  climate_entity.setup();

  climate_entity.make_call().set_swing_mode(climate::CLIMATE_SWING_VERTICAL).perform();
  EXPECT_EQ(ctx.select.active_index(), std::optional<size_t>{6});

  climate_entity.make_call().set_swing_mode(climate::CLIMATE_SWING_OFF).perform();
  EXPECT_EQ(ctx.select.active_index(), std::optional<size_t>{0});
}

TEST(MitsubishiCN105VerticalVaneDirectionSelectTests, BeforeInitializationDoesNotPublishSelectState) {
  VerticalVaneDirectionSelectTestContext ctx;

  ctx.select.make_call().set_index(3).perform();

  EXPECT_EQ(ctx.hub.status().vane_mode, MitsubishiCN105::VaneMode::POSITION_3);
  EXPECT_FALSE(ctx.select.has_state());
}
}  // namespace esphome::mitsubishi_cn105::testing
