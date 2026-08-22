#include "common.h"

namespace esphome::mitsubishi_cn105::testing {

TEST(MitsubishiCN105ComponentTests, PublishesVaneStateForEveryValidSnapshot) {
  TestableMitsubishiCN105Component hub;
  size_t callback_count = 0;
  std::optional<VerticalVaneMode> callback_direction;
  hub.add_on_vane_state_callback([&](const VaneState &state) {
    callback_count++;
    callback_direction = state.vertical.direction;
  });

  hub.mutable_status().room_temperature = 20.0f;
  hub.mutable_status().vane_mode = MitsubishiCN105::VaneMode::POSITION_4;
  hub.publish_status();

  EXPECT_EQ(callback_count, 1);
  EXPECT_EQ(callback_direction, std::optional{VERTICAL_VANE_MODE_POSITION_4});

  hub.publish_status();

  EXPECT_EQ(callback_count, 2);
  EXPECT_EQ(callback_direction, std::optional{VERTICAL_VANE_MODE_POSITION_4});
}

TEST(MitsubishiCN105ComponentTests, PublishesUnknownVaneState) {
  TestableMitsubishiCN105Component hub;
  size_t status_callback_count = 0;
  size_t vane_callback_count = 0;
  std::optional<VerticalVaneMode> callback_direction;
  hub.add_on_status_callback([&]() { status_callback_count++; });
  hub.add_on_vane_state_callback([&](const VaneState &state) {
    vane_callback_count++;
    callback_direction = state.vertical.direction;
  });

  hub.mutable_status().room_temperature = 20.0f;
  hub.mutable_status().vane_mode = MitsubishiCN105::VaneMode::UNKNOWN;
  hub.publish_status();

  EXPECT_EQ(status_callback_count, 1);
  EXPECT_EQ(vane_callback_count, 1);
  EXPECT_EQ(callback_direction, std::optional{VERTICAL_VANE_MODE_UNKNOWN});

  hub.mutable_status().vane_mode = MitsubishiCN105::VaneMode::POSITION_4;
  hub.publish_status();

  EXPECT_EQ(status_callback_count, 2);
  EXPECT_EQ(vane_callback_count, 2);
  EXPECT_EQ(callback_direction, std::optional{VERTICAL_VANE_MODE_POSITION_4});
}

TEST(MitsubishiCN105ComponentTests, VaneCallAppliesVerticalDirection) {
  TestableMitsubishiCN105Component hub;

  auto call = hub.make_vane_call();
  call.vertical.set_direction(VERTICAL_VANE_MODE_POSITION_5);
  call.perform();

  EXPECT_EQ(hub.status().vane_mode, MitsubishiCN105::VaneMode::POSITION_5);
}

TEST(MitsubishiCN105ComponentTests, VaneControlActionAppliesConfiguredFields) {
  TestableMitsubishiCN105Component hub;
  VaneControlAction<> action(&hub, [](VaneCall &call) { call.vertical.set_direction(VERTICAL_VANE_MODE_SWING); });

  action.play();

  EXPECT_EQ(hub.status().vane_mode, MitsubishiCN105::VaneMode::SWING);
}

}  // namespace esphome::mitsubishi_cn105::testing
