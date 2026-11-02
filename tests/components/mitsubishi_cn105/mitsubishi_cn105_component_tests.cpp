#include "common.h"

namespace esphome::mitsubishi_cn105::testing {

TEST(MitsubishiCN105ComponentTests, PublishesVaneStateForEveryValidSnapshot) {
  MitsubishiCN105Component hub;
  size_t callback_count = 0;
  std::optional<VaneState> callback_state;
  hub.add_on_vane_state_callback([&](const VaneState &state) {
    callback_count++;
    callback_state = state;
  });

  hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  hub.set_target_temperature(20.0f);
  hub.set_vane_mode(MitsubishiCN105::VaneMode::POSITION_4);
  hub.set_wide_vane_mode(MitsubishiCN105::WideVaneMode::SPLIT);
  hub.publish_status();

  EXPECT_EQ(callback_count, 1);
  ASSERT_TRUE(callback_state.has_value());
  EXPECT_EQ(callback_state->vertical.direction, VERTICAL_VANE_MODE_POSITION_4);
  EXPECT_EQ(callback_state->horizontal.direction, HORIZONTAL_VANE_MODE_SPLIT);

  // Changing only the wide vane leaves the vertical direction as it was
  hub.set_wide_vane_mode(MitsubishiCN105::WideVaneMode::SWING);
  hub.publish_status();

  EXPECT_EQ(callback_count, 2);
  EXPECT_EQ(callback_state->vertical.direction, VERTICAL_VANE_MODE_POSITION_4);
  EXPECT_EQ(callback_state->horizontal.direction, HORIZONTAL_VANE_MODE_SWING);
}

TEST(MitsubishiCN105ComponentTests, PublishesUnknownVaneState) {
  MitsubishiCN105Component hub;
  size_t status_callback_count = 0;
  size_t vane_callback_count = 0;
  std::optional<VaneState> callback_state;
  hub.add_on_status_callback([&]() { status_callback_count++; });
  hub.add_on_vane_state_callback([&](const VaneState &state) {
    vane_callback_count++;
    callback_state = state;
  });

  hub.set_telemetry_request_min_interval(SCHEDULER_DONT_RUN);
  hub.set_target_temperature(20.0f);
  ASSERT_EQ(hub.status().vane_mode, MitsubishiCN105::VaneMode::UNKNOWN);
  ASSERT_EQ(hub.status().wide_vane_mode, MitsubishiCN105::WideVaneMode::UNKNOWN);
  hub.publish_status();

  EXPECT_EQ(status_callback_count, 1);
  EXPECT_EQ(vane_callback_count, 1);
  ASSERT_TRUE(callback_state.has_value());
  EXPECT_EQ(callback_state->vertical.direction, VERTICAL_VANE_MODE_UNKNOWN);
  EXPECT_EQ(callback_state->horizontal.direction, HORIZONTAL_VANE_MODE_UNKNOWN);

  hub.set_vane_mode(MitsubishiCN105::VaneMode::POSITION_4);
  hub.set_wide_vane_mode(MitsubishiCN105::WideVaneMode::POSITION_5);
  hub.publish_status();

  EXPECT_EQ(status_callback_count, 2);
  EXPECT_EQ(vane_callback_count, 2);
  EXPECT_EQ(callback_state->vertical.direction, VERTICAL_VANE_MODE_POSITION_4);
  EXPECT_EQ(callback_state->horizontal.direction, HORIZONTAL_VANE_MODE_POSITION_5);
}

TEST(MitsubishiCN105ComponentTests, VaneCallAppliesVerticalDirection) {
  MitsubishiCN105Component hub;

  auto call = hub.make_vane_call();
  call.vertical.set_direction(VERTICAL_VANE_MODE_POSITION_5);
  call.perform();

  EXPECT_EQ(hub.status().vane_mode, MitsubishiCN105::VaneMode::POSITION_5);
}

TEST(MitsubishiCN105ComponentTests, VaneCallAppliesHorizontalDirection) {
  MitsubishiCN105Component hub;

  auto call = hub.make_vane_call();
  call.horizontal.set_direction(HORIZONTAL_VANE_MODE_SPLIT);
  call.perform();

  EXPECT_EQ(hub.status().wide_vane_mode, MitsubishiCN105::WideVaneMode::SPLIT);
}

TEST(MitsubishiCN105ComponentTests, VaneCallAppliesBothDirections) {
  MitsubishiCN105Component hub;

  auto call = hub.make_vane_call();
  call.vertical.set_direction(VERTICAL_VANE_MODE_POSITION_2);
  call.horizontal.set_direction(HORIZONTAL_VANE_MODE_POSITION_4);
  call.perform();

  EXPECT_EQ(hub.status().vane_mode, MitsubishiCN105::VaneMode::POSITION_2);
  EXPECT_EQ(hub.status().wide_vane_mode, MitsubishiCN105::WideVaneMode::POSITION_4);
}

}  // namespace esphome::mitsubishi_cn105::testing
