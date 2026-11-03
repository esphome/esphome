#include "../common.h"
#include "esphome/components/select/select.h"

namespace esphome::mitsubishi_cn105::testing {

// Forwards to the hub the way the generated ApplySelect does: apply="control_*_vane_direction(index)"
template<void (MitsubishiCN105Component::*CONTROL)(size_t)> class VaneSelect final : public select::Select {
 public:
  explicit VaneSelect(MitsubishiCN105Component *hub) : hub_(hub) {}

 protected:
  void control(size_t index) override { (this->hub_->*CONTROL)(index); }

  MitsubishiCN105Component *hub_;
};

static constexpr std::array VERTICAL_VANE_MODES{
    MitsubishiCN105::VaneMode::AUTO,       MitsubishiCN105::VaneMode::POSITION_1, MitsubishiCN105::VaneMode::POSITION_2,
    MitsubishiCN105::VaneMode::POSITION_3, MitsubishiCN105::VaneMode::POSITION_4, MitsubishiCN105::VaneMode::POSITION_5,
    MitsubishiCN105::VaneMode::SWING,
};

struct VerticalVaneDirectionSelectTestContext {
  MitsubishiCN105Component hub;
  VaneSelect<&MitsubishiCN105Component::control_vertical_vane_direction> select{&hub};

  VerticalVaneDirectionSelectTestContext() {
    this->select.traits.set_options({"Auto", "1", "2", "3", "4", "5", "Swing"});
    this->hub.set_vertical_vane_direction_select(&this->select);
  }
};

TEST(MitsubishiCN105VerticalVaneDirectionSelectTests, MapsIndexesToVaneModes) {
  VerticalVaneDirectionSelectTestContext ctx;

  for (size_t i = 0; i < VERTICAL_VANE_MODES.size(); ++i) {
    SCOPED_TRACE(i);
    ctx.select.make_call().set_index(i).perform();
    EXPECT_EQ(ctx.hub.status().vane_mode, VERTICAL_VANE_MODES[i]);
  }
}

TEST(MitsubishiCN105VerticalVaneDirectionSelectTests, PublishesIncomingVaneModes) {
  VerticalVaneDirectionSelectTestContext ctx;
  ctx.hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  ctx.hub.set_target_temperature(20.0f);

  for (size_t i = 0; i < VERTICAL_VANE_MODES.size(); ++i) {
    SCOPED_TRACE(i);
    ctx.hub.set_vane_mode(VERTICAL_VANE_MODES[i]);
    ctx.hub.publish_status();
    EXPECT_EQ(ctx.select.active_index(), std::optional{i});
  }
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

static constexpr std::array HORIZONTAL_VANE_MODES{
    MitsubishiCN105::WideVaneMode::POSITION_1, MitsubishiCN105::WideVaneMode::POSITION_2,
    MitsubishiCN105::WideVaneMode::POSITION_3, MitsubishiCN105::WideVaneMode::POSITION_4,
    MitsubishiCN105::WideVaneMode::POSITION_5, MitsubishiCN105::WideVaneMode::SPLIT,
    MitsubishiCN105::WideVaneMode::SWING,
};

struct HorizontalVaneDirectionSelectTestContext {
  MitsubishiCN105Component hub;
  VaneSelect<&MitsubishiCN105Component::control_horizontal_vane_direction> select{&hub};

  HorizontalVaneDirectionSelectTestContext() {
    this->select.traits.set_options({"1", "2", "3", "4", "5", "Split", "Swing"});
    this->hub.set_horizontal_vane_direction_select(&this->select);
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

  // Swing off restores the last non-swing wide vane position, POSITION_3 by default.
  climate_entity.make_call().set_swing_mode(climate::CLIMATE_SWING_OFF).perform();
  EXPECT_EQ(ctx.select.active_index(), std::optional<size_t>{2});
}

TEST(MitsubishiCN105HorizontalVaneDirectionSelectTests, BeforeInitializationDoesNotPublishSelectState) {
  HorizontalVaneDirectionSelectTestContext ctx;

  ctx.select.make_call().set_index(3).perform();

  EXPECT_EQ(ctx.hub.status().wide_vane_mode, MitsubishiCN105::WideVaneMode::POSITION_4);
  EXPECT_FALSE(ctx.select.has_state());
}

}  // namespace esphome::mitsubishi_cn105::testing
