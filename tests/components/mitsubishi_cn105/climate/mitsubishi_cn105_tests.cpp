#include "../common.h"

namespace esphome::mitsubishi_cn105::testing {

struct TestContext {
  MockUARTComponent uart;
  uart::UARTDevice device{&uart};
  TestableMitsubishiCN105 sut{device};

  TestContext() { this->sut.set_current_time(0); }
};

TEST(MitsubishiCN105Tests, InitSendsConnectPacket) {
  auto ctx = TestContext{};

  ctx.sut.set_current_time(123);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::NOT_CONNECTED);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_FALSE(ctx.sut.write_timeout_start_ms_.has_value());

  ctx.sut.initialize();

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8));
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{123});
}

TEST(MitsubishiCN105Tests, ConnectAndUpdateStatus) {
  auto ctx = TestContext{};

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{0});
  EXPECT_FALSE(ctx.sut.status_update_start_ms_.has_value());

  // Connect response
  ctx.uart.push_rx({0xFC, 0x7A, 0x01, 0x30, 0x00, 0x55});

  ctx.sut.set_current_time(200);
  ASSERT_FALSE(ctx.sut.update());

  // All bytes from UART should be consumed
  EXPECT_TRUE(ctx.uart.rx.empty());
  // After successful connect we request status, first settings (0x02)
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x42, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B));
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{200});
  EXPECT_FALSE(ctx.sut.status_update_start_ms_.has_value());

  // Clear TX bytes.
  ctx.uart.tx.clear();

  // Settings response
  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x00, 0x08, 0x07,
                    0x00, 0x00, 0x00, 0x00, 0x03, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x99});

  // Settings should still have initial values
  EXPECT_FALSE(ctx.sut.status().power_on);
  EXPECT_THAT(ctx.sut.status().target_temperature, ::testing::IsNan());
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::UNKNOWN);
  EXPECT_EQ(ctx.sut.status().fan_mode, MitsubishiCN105::FanMode::UNKNOWN);

  ctx.sut.set_current_time(300);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.rx.empty());

  // Check settings that we just read from received package
  EXPECT_FALSE(ctx.sut.status().power_on);
  EXPECT_EQ(ctx.sut.status().target_temperature, 24.0f);
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::AUTO);
  EXPECT_EQ(ctx.sut.status().fan_mode, MitsubishiCN105::FanMode::AUTO);

  // Now fetch room temperature (0x03)
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x42, 0x01, 0x30, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7A));
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{300});
  EXPECT_FALSE(ctx.sut.status_update_start_ms_.has_value());

  // Clear TX bytes.
  ctx.uart.tx.clear();

  // Room temperature response
  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x03, 0x00, 0x00, 0x0B, 0x00, 0x00,
                    0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA5});

  // Room temperature should still have initial value
  EXPECT_THAT(ctx.sut.status().room_temperature, ::testing::IsNan());

  ctx.sut.set_current_time(400);
  EXPECT_FALSE(ctx.sut.is_status_initialized());
  ASSERT_TRUE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.rx.empty());
  EXPECT_TRUE(ctx.sut.is_status_initialized());

  // Check room temperature we just read from received package
  EXPECT_EQ(ctx.sut.status().room_temperature, 21.0f);

  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_FALSE(ctx.sut.write_timeout_start_ms_.has_value());
  EXPECT_EQ(ctx.sut.status_update_start_ms_, std::optional<uint32_t>{400});
}

TEST(MitsubishiCN105Tests, NoResponseTriggersReconnect) {
  auto ctx = TestContext{};

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  // No response (no RX data), no retry yet
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{0});

  // Still no response after 1999ms, no retry yet
  ctx.sut.set_current_time(1999);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{0});

  // Stop waiting after 2s and retry connect
  ctx.sut.set_current_time(2000);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8));
  EXPECT_EQ(ctx.sut.write_timeout_start_ms_, std::optional<uint32_t>{2000});
}

TEST(MitsubishiCN105Tests, RxWatchdogLimitsProcessingPerUpdate) {
  auto ctx = TestContext{};

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  // RX noise/unexpected traffic
  ctx.uart.push_rx({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
                    0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
                    0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                    0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46});

  // Make sure we have enough bytes in buffer.
  ASSERT_GT(ctx.uart.rx.size(), 64);

  // No valid response, no state change expected
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());

  // Watchdog interrupts reading (max. 64 bytes at once) so we do not spend the whole loop draining UART
  EXPECT_FALSE(ctx.uart.rx.empty());

  // Next update will read remaining bytes, no state change expected
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_TRUE(ctx.uart.rx.empty());
}

TEST(MitsubishiCN105Tests, ParserHandlesMixedRxStream) {
  auto ctx = TestContext{};

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  // Mixed RX stream with partial, malformed, and oversized frames to test parser robustness
  ctx.uart.push_rx({// ─────────────────────────────
                    // Noise (no 0xFC) -> should be ignored via preamble reset
                    // ────────────────────────────
                    0x01, 0x02, 0x03, 0x04, 0x05,

                    // ─────────────────────────────
                    // Partial frame (declares payload len=5, but we cut it short)
                    // Later bytes will eventually force checksum mismatch and reset
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0x05, 0xAA, 0xBB,

                    // ─────────────────────────────
                    // Invalid header (header byte 3 should be 0x01, header byte 4 should be 0x30)
                    // Should reset quickly on header mismatch
                    // ─────────────────────────────
                    0xFC, 0x62, 0xFF, 0xFF, 0x02, 0x01, 0x02, 0x00,

                    // ─────────────────────────────
                    // Oversized length field (rejected by payload-too-large check at HEADER_LEN)
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0xFE, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
                    0x1B, 0x1C, 0x1D, 0x1E, 0x1F,

                    // ─────────────────────────────
                    // Valid unknown-type frame (type=0x62), should be parsed successfully then ignored
                    // Frame: FC 62 01 30 02 AA BB 30
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0x02, 0xAA, 0xBB, 0x30,

                    // ─────────────────────────────
                    // Invalid checksum (should be rejected at checksum check)
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0x02, 0x10, 0x20, 0xFF,

                    // ─────────────────────────────
                    // Back-to-back VALID frames (unknown type=0x62) to stress boundary handling.
                    // Frame A: FC 62 01 30 01 02 6C
                    // Frame B: FC 62 01 30 01 03 6B
                    // ─────────────────────────────
                    0xFC, 0x62, 0x01, 0x30, 0x01, 0x02, 0x6C, 0xFC, 0x62, 0x01, 0x30, 0x01, 0x03, 0x6B,

                    // ─────────────────────────────
                    // Trailing noise
                    // ─────────────────────────────
                    0x55, 0x66, 0x77, 0x88});

  // Drain RX - no valid response, no state change expected
  int iterations = 0;
  while (!ctx.uart.rx.empty() && iterations++ < 10) {
    ASSERT_FALSE(ctx.sut.update());
    EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
    EXPECT_TRUE(ctx.uart.tx.empty());
  }

  EXPECT_TRUE(ctx.uart.rx.empty());
}

TEST(MitsubishiCN105Tests, NextStatusUpdateAfterUpdateIntervalMilliseconds) {
  auto ctx = TestContext{};

  ctx.sut.set_update_interval(2000);
  ctx.sut.set_current_time(80000);

  // No scheduled status update
  EXPECT_FALSE(ctx.sut.status_update_start_ms_.has_value());

  // Status update completed, schedule next status update
  ctx.sut.state_ = TestableMitsubishiCN105::State::STATUS_UPDATED;
  ctx.sut.set_state(TestableMitsubishiCN105::State::SCHEDULE_NEXT_STATUS_UPDATE);

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_EQ(ctx.sut.status_update_start_ms_, std::optional<uint32_t>{80000});

  // Wait for update_interval (ms) before doing another status update
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  ctx.sut.set_current_time(81999);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  ctx.sut.set_current_time(82000);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_FALSE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  EXPECT_FALSE(ctx.sut.status_update_start_ms_.has_value());
}

TEST(MitsubishiCN105Tests, DecodeStatusSettingsPackageTempEncodedA) {
  auto ctx = TestContext{};

  ctx.uart.push_rx(
      {0xFC, 0x62, 0x01, 0x30, 0x0C, 0x02, 0x00, 0x00, 0x01, 0x03, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55});

  ctx.sut.update();

  EXPECT_TRUE(ctx.sut.status().power_on);
  EXPECT_FALSE(ctx.sut.use_temperature_encoding_b_);
  EXPECT_EQ(ctx.sut.status().target_temperature, 26.0f);
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::COOL);
  EXPECT_EQ(ctx.sut.status().fan_mode, MitsubishiCN105::FanMode::QUIET);
}

TEST(MitsubishiCN105Tests, DecodeStatusSettingsPackageTempEncodedB) {
  auto ctx = TestContext{};

  ctx.uart.push_rx(
      {0xFC, 0x62, 0x01, 0x30, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x07, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0xA5, 0xAD});

  ctx.sut.update();

  EXPECT_FALSE(ctx.sut.status().power_on);
  EXPECT_TRUE(ctx.sut.use_temperature_encoding_b_);
  EXPECT_EQ(ctx.sut.status().target_temperature, 18.5f);
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::FAN_ONLY);
  EXPECT_EQ(ctx.sut.status().fan_mode, MitsubishiCN105::FanMode::SPEED_4);
}

TEST(MitsubishiCN105Tests, DecodeStatusRoomTempPackageTempEncodedA) {
  auto ctx = TestContext{};

  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x07, 0x03, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x5D});

  ctx.sut.update();

  EXPECT_EQ(ctx.sut.status().room_temperature, 16.0f);
}

TEST(MitsubishiCN105Tests, DecodeStatusRoomTempPackageTempEncodedB) {
  auto ctx = TestContext{};

  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBC, 0xA7});

  ctx.sut.update();

  EXPECT_EQ(ctx.sut.status().room_temperature, 30.0f);
}

TEST(MitsubishiCN105Tests, ApplySettingsPowerOn) {
  auto ctx = TestContext{};

  ctx.sut.set_power(true);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B));
}

TEST(MitsubishiCN105Tests, ApplySettingsTemperatureEncodedA) {
  auto ctx = TestContext{};

  ctx.sut.set_target_temperature(23.0f);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x04, 0x00, 0x00, 0x00, 0x08,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x71));
}

TEST(MitsubishiCN105Tests, ApplySettingsTemperatureEncodedB) {
  auto ctx = TestContext{};

  ctx.sut.use_temperature_encoding_b_ = true;
  ctx.sut.set_target_temperature(26.0f);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x00, 0xC5));
}

TEST(MitsubishiCN105Tests, ApplySettingsHalfDegreeTemperatureEncodedB) {
  auto ctx = TestContext{};

  ctx.sut.use_temperature_encoding_b_ = true;
  ctx.sut.set_target_temperature(26.5f);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB5, 0x00, 0xC4));
}

TEST(MitsubishiCN105Tests, ApplyModeCool) {
  auto ctx = TestContext{};

  ctx.sut.set_mode(MitsubishiCN105::Mode::COOL);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x02, 0x00, 0x00, 0x03, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78));
}

TEST(MitsubishiCN105Tests, ApplyFanModeSpeed1) {
  auto ctx = TestContext{};

  ctx.sut.set_fan_mode(MitsubishiCN105::FanMode::SPEED_1);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00,
                                                  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73));
}

TEST(MitsubishiCN105Tests, WriteInterruptsWaitingForNextStatusUpdate) {
  auto ctx = TestContext{};

  ctx.sut.set_update_interval(2000);
  ctx.sut.set_current_time(5000);

  // Waiting for next scheduled status update
  ctx.sut.state_ = TestableMitsubishiCN105::State::STATUS_UPDATED;
  ctx.sut.set_state(TestableMitsubishiCN105::State::SCHEDULE_NEXT_STATUS_UPDATE);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_EQ(ctx.sut.status_update_start_ms_, std::optional<uint32_t>{5000});
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);

  // Nothing to do in update (rx empty, no timeout)
  ctx.sut.set_current_time(5500);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.status_update_start_ms_, std::optional<uint32_t>{5000});
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);

  // Write new values
  ctx.sut.use_temperature_encoding_b_ = true;
  ctx.sut.set_power(false);
  ctx.sut.set_target_temperature(25.0f);
  ctx.sut.set_mode(MitsubishiCN105::Mode::HEAT);
  ctx.sut.set_fan_mode(MitsubishiCN105::FanMode::AUTO);

  // Waiting for next status update must be interrupted and new values send to AC
  ctx.sut.set_current_time(6000);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_FALSE(ctx.sut.status_update_start_ms_.has_value());
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 1000);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x0F, 0x00, 0x00, 0x01, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB2, 0x00, 0xBB));

  // Write ACK response
  ctx.uart.push_rx({0xFC, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E});
  ctx.sut.set_current_time(6500);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_EQ(ctx.sut.status_update_start_ms_, std::optional<uint32_t>{6500 - 1000});
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);
}

TEST(MitsubishiCN105Tests, SetAndClearRemoteRoomTemp) {
  auto ctx = TestContext{};

  // Set remote temperature
  ctx.sut.set_remote_temperature(28.5f);

  ctx.sut.state_ = TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE;
  ctx.sut.set_state(TestableMitsubishiCN105::State::APPLYING_SETTINGS);

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x07, 0x01, 0x29, 0xB9, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x94));

  // Write ACK response
  ctx.uart.push_rx({0xFC, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E});
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  ctx.uart.tx.clear();

  // Clear remote temperature
  ctx.sut.clear_remote_temperature();

  ctx.sut.set_state(TestableMitsubishiCN105::State::APPLYING_SETTINGS);

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x07, 0x00, 0x00, 0x80, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7));

  // Write ACK response
  ctx.uart.push_rx({0xFC, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E});
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
}

TEST(MitsubishiCN105Tests, ApplyQueuedSettingsThenRemoteRoomTempInSecondWrite) {
  auto ctx = TestContext{};

  // Queue normal settings plus remote temperature together.
  ctx.sut.use_temperature_encoding_b_ = true;
  ctx.sut.set_power(false);
  ctx.sut.set_target_temperature(25.0f);
  ctx.sut.set_mode(MitsubishiCN105::Mode::HEAT);
  ctx.sut.set_fan_mode(MitsubishiCN105::FanMode::AUTO);
  ctx.sut.set_remote_temperature(28.5f);

  // First apply sends only the normal settings write.
  ctx.sut.state_ = TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE;
  ctx.sut.set_state(TestableMitsubishiCN105::State::APPLYING_SETTINGS);

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x0F, 0x00, 0x00, 0x01, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB2, 0x00, 0xBB));
  EXPECT_TRUE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::REMOTE_TEMPERATURE));
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::POWER));
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::TEMPERATURE));
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::MODE));
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::FAN));

  // ACK the first write. Remote temperature should still be pending afterward.
  ctx.uart.tx.clear();
  ctx.uart.push_rx({0xFC, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E});
  ASSERT_FALSE(ctx.sut.update());

  EXPECT_TRUE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::REMOTE_TEMPERATURE));

  // The next apply sends the remote-temperature packet and clears the last pending flag.
  ctx.uart.tx.clear();
  ctx.sut.set_state(TestableMitsubishiCN105::State::APPLYING_SETTINGS);

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x07, 0x01, 0x29, 0xB9, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x94));
  EXPECT_FALSE(ctx.sut.pending_updates_.any());
}

TEST(MitsubishiCN105Tests, WriteTimeoutClearsStatusUpdateWaitCreditOnReconnect) {
  auto ctx = TestContext{};
  ctx.sut.set_update_interval(2000);
  ctx.sut.set_current_time(5000);

  // Start in the scheduled status update wait state.
  ctx.sut.state_ = TestableMitsubishiCN105::State::STATUS_UPDATED;
  ctx.sut.set_state(TestableMitsubishiCN105::State::SCHEDULE_NEXT_STATUS_UPDATE);
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  ASSERT_EQ(ctx.sut.status_update_start_ms_, std::optional<uint32_t>{5000});
  ASSERT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);

  // Interrupt that wait with a write so credit is accumulated.
  ctx.sut.use_temperature_encoding_b_ = true;
  ctx.sut.set_power(false);
  ctx.sut.set_target_temperature(25.0f);
  ctx.sut.set_mode(MitsubishiCN105::Mode::HEAT);
  ctx.sut.set_fan_mode(MitsubishiCN105::FanMode::AUTO);
  ctx.sut.set_current_time(6000);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  ASSERT_FALSE(ctx.sut.status_update_start_ms_.has_value());
  ASSERT_EQ(ctx.sut.status_update_wait_credit_ms_, 1000);

  // Do not ACK the write. Advance time far enough to force timeout/reconnect
  // handling and verify that stale wait credit is cleared during recovery.
  ctx.sut.set_current_time(36000);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_NE(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);
  EXPECT_FALSE(ctx.sut.status_update_start_ms_.has_value());
}

TEST(MitsubishiCN105Tests, SetOutOfRangeRemoteRoomTempIsIgnored) {
  auto ctx = TestContext{};

  ctx.sut.set_remote_temperature(7.0f);
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::REMOTE_TEMPERATURE));

  ctx.sut.set_remote_temperature(40.0f);
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::REMOTE_TEMPERATURE));

  ctx.sut.set_remote_temperature(NAN);
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::REMOTE_TEMPERATURE));
}

TEST(MitsubishiCN105Tests, SetMinRemoteRoomTemp) {
  auto ctx = TestContext{};
  ctx.sut.set_remote_temperature(8.0f);
  EXPECT_TRUE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::REMOTE_TEMPERATURE));
}

TEST(MitsubishiCN105Tests, SetMaxRemoteRoomTemp) {
  auto ctx = TestContext{};
  ctx.sut.set_remote_temperature(39.5f);
  EXPECT_TRUE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::UpdateFlag::REMOTE_TEMPERATURE));
}

}  // namespace esphome::mitsubishi_cn105::testing
