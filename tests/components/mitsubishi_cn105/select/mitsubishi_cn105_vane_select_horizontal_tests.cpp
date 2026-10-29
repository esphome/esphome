#include "../common.h"
#include "esphome/components/mitsubishi_cn105/select/mitsubishi_cn105_vane_select_horizontal.h"

namespace esphome::mitsubishi_cn105::testing {

static constexpr std::array HORIZONTAL_VANE_MODES{
    MitsubishiCN105::WideVaneMode::FAR_LEFT,  MitsubishiCN105::WideVaneMode::LEFT,
    MitsubishiCN105::WideVaneMode::CENTER,    MitsubishiCN105::WideVaneMode::RIGHT,
    MitsubishiCN105::WideVaneMode::FAR_RIGHT, MitsubishiCN105::WideVaneMode::LEFT_RIGHT,
    MitsubishiCN105::WideVaneMode::SWING,
};

struct HorizontalVaneDirectionSelectTestContext {
  MitsubishiCN105Component hub;
  MitsubishiCN105HorizontalVaneDirectionSelect select;

  HorizontalVaneDirectionSelectTestContext() {
    this->select.traits.set_options({"←←", "←", "|", "→", "→→", "←→", "Swing"});
    this->select.set_parent(&this->hub);
    this->select.setup();
  }
};

TEST(MitsubishiCN105HorizontalVaneDirectionSelectTests, MapsIndexesToWideVaneModes) {
  HorizontalVaneDirectionSelectTestContext ctx;

  for (size_t i = 0; i < HORIZONTAL_VANE_MODES.size(); ++i) {
    SCOPED_TRACE(i);
    ctx.select.make_call().set_index(i).perform();
    EXPECT_EQ(ctx.hub.status().wide_vane_mode, HORIZONTAL_VANE_MODES[i]);
  }
}

TEST(MitsubishiCN105HorizontalVaneDirectionSelectTests, PublishesIncomingWideVaneModes) {
  HorizontalVaneDirectionSelectTestContext ctx;
  ctx.hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  ctx.hub.set_target_temperature(20.0f);

  for (size_t i = 0; i < HORIZONTAL_VANE_MODES.size(); ++i) {
    SCOPED_TRACE(i);
    ctx.hub.set_wide_vane_mode(HORIZONTAL_VANE_MODES[i]);
    ctx.hub.publish_status();
    EXPECT_EQ(ctx.select.active_index(), std::optional{i});
  }

  ctx.select.publish_wide_vane_state(MitsubishiCN105::WideVaneMode::UNKNOWN);
  EXPECT_EQ(ctx.select.active_index(), std::optional{HORIZONTAL_VANE_MODES.size() - 1});
}

TEST(MitsubishiCN105HorizontalVaneDirectionSelectTests, ControlPublishesSelectAndClimateThroughHub) {
  HorizontalVaneDirectionSelectTestContext ctx;
  MitsubishiCN105Climate climate_entity;
  climate_entity.set_parent(&ctx.hub);
  climate_entity.set_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

  ctx.hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  ctx.hub.set_target_temperature(20.0f);
  climate_entity.setup();

  ctx.select.make_call().set_index(6).perform();
  EXPECT_EQ(ctx.select.active_index(), std::optional<size_t>{6});
  EXPECT_EQ(climate_entity.swing_mode, climate::CLIMATE_SWING_HORIZONTAL);

  ctx.select.make_call().set_index(3).perform();
  EXPECT_EQ(ctx.select.active_index(), std::optional<size_t>{3});
  EXPECT_EQ(climate_entity.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST(MitsubishiCN105HorizontalVaneDirectionSelectTests, ClimateControlPublishesSelectThroughHub) {
  HorizontalVaneDirectionSelectTestContext ctx;
  MitsubishiCN105Climate climate_entity;
  climate_entity.set_parent(&ctx.hub);
  climate_entity.set_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

  ctx.hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  ctx.hub.set_target_temperature(20.0f);
  climate_entity.setup();

  climate_entity.make_call().set_swing_mode(climate::CLIMATE_SWING_HORIZONTAL).perform();
  EXPECT_EQ(ctx.select.active_index(), std::optional<size_t>{6});

  // Swing off restores the last non-swing wide vane position, CENTER by default.
  climate_entity.make_call().set_swing_mode(climate::CLIMATE_SWING_OFF).perform();
  EXPECT_EQ(ctx.select.active_index(), std::optional<size_t>{2});
}

TEST(MitsubishiCN105HorizontalVaneDirectionSelectTests, BeforeInitializationDoesNotPublishSelectState) {
  HorizontalVaneDirectionSelectTestContext ctx;

  ctx.select.make_call().set_index(3).perform();

  EXPECT_EQ(ctx.hub.status().wide_vane_mode, MitsubishiCN105::WideVaneMode::RIGHT);
  EXPECT_FALSE(ctx.select.has_state());
}
}  // namespace esphome::mitsubishi_cn105::testing
