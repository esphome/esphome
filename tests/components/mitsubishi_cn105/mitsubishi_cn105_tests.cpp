#include "common.h"

namespace esphome::mitsubishi_cn105::testing {

struct TestContext {
  MockUARTComponent uart;
  uart::UARTDevice device{&uart};
  TestableMitsubishiCN105 sut{device};
};

struct ConnectionStateCallbackMock {
  MOCK_METHOD(void, call, (bool) );

  void register_with(MitsubishiCN105 &sut) {
    sut.set_connection_state_callback([this](bool is_connected) { this->call(is_connected); });
  }
};

TEST(MitsubishiCN105Tests, InitiallyStatusNotInitialized) {
  auto ctx = TestContext{};
  EXPECT_FALSE(ctx.sut.is_status_initialized());
}

TEST(MitsubishiCN105Tests, InitSendsConnectPacket) {
  auto ctx = TestContext{};

  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::NOT_CONNECTED);
  ASSERT_TRUE(ctx.uart.tx.empty());

  ctx.sut.init();

  EXPECT_FALSE(ctx.sut.is_status_initialized());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8));
}

TEST(MitsubishiCN105Tests, NoResponseTriggersReconnect) {
  auto ctx = TestContext{};

  ::testing::StrictMock<ConnectionStateCallbackMock> connection_state_callback;
  connection_state_callback.register_with(ctx.sut);

  ctx.sut.init();

  // Remove first connect packet bytes.
  ctx.uart.tx.clear();

  // No response (no data read from UART), no retry yet.
  ASSERT_FALSE(ctx.sut.sync());
  EXPECT_FALSE(ctx.sut.is_status_initialized());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  ASSERT_TRUE(ctx.uart.tx.empty());

  // Still no response after 1999ms, no retry yet.
  ctx.sut.current_time_ms = 1999;
  ASSERT_FALSE(ctx.sut.sync());
  EXPECT_FALSE(ctx.sut.is_status_initialized());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  ASSERT_TRUE(ctx.uart.tx.empty());

  // Stop waiting after 2s and retry connect.
  ctx.sut.current_time_ms = 2000;
  EXPECT_CALL(connection_state_callback, call(false)).Times(1);
  ASSERT_FALSE(ctx.sut.sync());
  EXPECT_FALSE(ctx.sut.is_status_initialized());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8));
}

TEST(MitsubishiCN105Tests, ConnectAndUpdateStatus) {
  auto ctx = TestContext{};

  ::testing::StrictMock<ConnectionStateCallbackMock> connection_state_callback;
  connection_state_callback.register_with(ctx.sut);

  ctx.sut.init();
  ctx.uart.tx.clear();  // Remove connect packet bytes from buffer.

  EXPECT_CALL(connection_state_callback, call(true)).Times(1);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);

  // Connect response
  ctx.uart.push_rx({0xFC, 0x7A, 0x01, 0x30, 0x00, 0x55});

  ASSERT_FALSE(ctx.sut.sync());
  EXPECT_FALSE(ctx.sut.is_status_initialized());
  ASSERT_TRUE(ctx.uart.rx.empty());

  // After successful connect we request status, first settings (0x02)
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x42, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B));

  // Clear TX bytes.
  ctx.uart.tx.clear();

  // Settings response
  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x00, 0x08, 0x07,
                    0x00, 0x00, 0x00, 0x00, 0x03, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x99});

  // Settings should still have initial values
  EXPECT_FALSE(ctx.sut.status().settings.power_on);
  EXPECT_THAT(ctx.sut.status().settings.target_temperature, ::testing::IsNan());
  EXPECT_EQ(ctx.sut.status().settings.mode, TestableMitsubishiCN105::Mode::UNKNOWN);
  EXPECT_EQ(ctx.sut.status().settings.fan_mode, TestableMitsubishiCN105::FanMode::UNKNOWN);

  ASSERT_FALSE(ctx.sut.sync());
  // Not all statuses received yet, therefore still false
  EXPECT_FALSE(ctx.sut.is_status_initialized());
  ASSERT_TRUE(ctx.uart.rx.empty());

  // Check settings that we just read from received package
  EXPECT_FALSE(ctx.sut.status().settings.power_on);
  EXPECT_EQ(ctx.sut.status().settings.target_temperature, 24.0f);
  EXPECT_EQ(ctx.sut.status().settings.mode, TestableMitsubishiCN105::Mode::AUTO);
  EXPECT_EQ(ctx.sut.status().settings.fan_mode, TestableMitsubishiCN105::FanMode::AUTO);

  // Now fetch room temperature (0x03)
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x42, 0x01, 0x30, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7A));

  // Clear TX bytes.
  ctx.uart.tx.clear();

  // Room temperature response
  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x03, 0x00, 0x00, 0x0B, 0x00, 0x00,
                    0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA5});

  // Room temperature should still have initial value
  EXPECT_THAT(ctx.sut.status().room_temperature, ::testing::IsNan());

  // Should return true since data changed so we should publish
  ASSERT_TRUE(ctx.sut.sync());
  // All data now initialized
  EXPECT_TRUE(ctx.sut.is_status_initialized());
  // and all data read from rx buffer
  ASSERT_TRUE(ctx.uart.rx.empty());

  // Check room temperature we just read from received package
  EXPECT_EQ(ctx.sut.status().room_temperature, 21.0f);

  // And pause for update_interval_ms_
  ASSERT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
}

TEST(MitsubishiCN105Tests, NextStatusUpdateAfterUpdateIntervalMilliseconds) {
  auto ctx = TestContext{};

  ctx.sut.set_update_interval(2000);
  ctx.sut.current_time_ms = 80000;

  // No scheduled status update
  EXPECT_FALSE(ctx.sut.status_update_start_ms_);

  // Status update completed, schedule next status update
  ctx.sut.state_ = TestableMitsubishiCN105::State::STATUS_UPDATED;
  ctx.sut.set_state(TestableMitsubishiCN105::State::SCHEDULE_NEXT_STATUS_UPDATE);

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_TRUE(ctx.sut.status_update_start_ms_);
  EXPECT_EQ(*ctx.sut.status_update_start_ms_, 80000);

  // Wait for update_interval (ms) before doing another status update
  ASSERT_FALSE(ctx.sut.sync());
  ASSERT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  ctx.sut.current_time_ms = 81999;
  ASSERT_FALSE(ctx.sut.sync());
  ASSERT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  ctx.sut.current_time_ms = 82000;
  ASSERT_FALSE(ctx.sut.sync());
  ASSERT_FALSE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  EXPECT_FALSE(ctx.sut.status_update_start_ms_);
}

}  // namespace esphome::mitsubishi_cn105::testing
