#include "../common.h"

namespace esphome::mitsubishi_cn105::testing {

struct MitsubishiCN105TestsContext {
  MockUARTComponent uart;
  uart::UARTDevice device{&uart};
  TestableMitsubishiCN105 sut{device};

  MitsubishiCN105TestsContext() { this->sut.set_current_time(0); }
};

TEST(MitsubishiCN105Tests, InitSendsConnectPacket) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_current_time(123);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::NOT_CONNECTED);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.operation_start_ms_, 0);

  ctx.sut.initialize();

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8));
  EXPECT_EQ(ctx.sut.operation_start_ms_, 123);
}

TEST(MitsubishiCN105Tests, ConnectAndUpdateStatus) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_EQ(ctx.sut.operation_start_ms_, 0);

  // Connect response
  ctx.uart.push_rx({0xFC, 0x7A, 0x01, 0x30, 0x00, 0x55});

  ctx.sut.set_current_time(200);
  ASSERT_FALSE(ctx.sut.update());

  // All bytes from UART should be consumed
  EXPECT_TRUE(ctx.uart.rx.empty());
  // Defer the first settings request (0x02) until the next update.
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::DEFERRED_STATUS_REQUEST);
  EXPECT_TRUE(ctx.uart.tx.empty());

  ctx.sut.set_current_time(201);
  ASSERT_FALSE(ctx.sut.update());

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x42, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B));
  EXPECT_EQ(ctx.sut.operation_start_ms_, 201);

  // Clear TX bytes.
  ctx.uart.tx.clear();

  // Settings response
  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x00, 0x08, 0x07,
                    0x00, 0x04, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C});

  // Settings should still have initial values
  EXPECT_FALSE(ctx.sut.status().power_on);
  EXPECT_THAT(ctx.sut.status().target_temperature, ::testing::IsNan());
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::UNKNOWN);
  EXPECT_EQ(ctx.sut.status().fan_mode, MitsubishiCN105::FanMode::UNKNOWN);
  EXPECT_EQ(ctx.sut.status().vane_mode, MitsubishiCN105::VaneMode::UNKNOWN);
  EXPECT_EQ(ctx.sut.status().wide_vane_mode, MitsubishiCN105::WideVaneMode::UNKNOWN);

  ctx.sut.set_current_time(300);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.rx.empty());

  // Check settings that we just read from received package
  EXPECT_FALSE(ctx.sut.status().power_on);
  EXPECT_EQ(ctx.sut.status().target_temperature, 24.0f);
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::AUTO);
  EXPECT_EQ(ctx.sut.status().fan_mode, MitsubishiCN105::FanMode::AUTO);
  EXPECT_EQ(ctx.sut.status().vane_mode, MitsubishiCN105::VaneMode::POSITION_4);
  EXPECT_EQ(ctx.sut.status().wide_vane_mode, MitsubishiCN105::WideVaneMode::SWING);

  // Defer the telemetry request (0x03) until the next update.
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::DEFERRED_STATUS_REQUEST);
  EXPECT_TRUE(ctx.uart.tx.empty());

  ctx.sut.set_current_time(301);
  ASSERT_FALSE(ctx.sut.update());

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x42, 0x01, 0x30, 0x10, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7A));
  EXPECT_EQ(ctx.sut.operation_start_ms_, 301);

  // Clear TX bytes.
  ctx.uart.tx.clear();

  // Queue a setting while waiting for telemetry.
  ctx.sut.set_power(true);

  // Telemetry response
  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x03, 0x00, 0x00, 0x0B, 0x00, 0x00,
                    0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA5});

  // Room temperature from telemetry should still have initial value
  EXPECT_THAT(ctx.sut.status().room_temperature, ::testing::IsNan());

  ctx.sut.set_current_time(400);
  EXPECT_FALSE(ctx.sut.is_status_initialized());
  ASSERT_TRUE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.rx.empty());
  EXPECT_TRUE(ctx.sut.is_status_initialized());

  // Check room temperature we just read from telemetry package
  EXPECT_EQ(ctx.sut.status().room_temperature, 21.0f);

  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_EQ(ctx.sut.operation_start_ms_, 400);

  // Apply the pending setting on the next update, outside RX processing.
  ctx.sut.set_current_time(401);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  EXPECT_FALSE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.operation_start_ms_, 401);
}

// Sends the deferred status request and returns its message type.
static uint8_t send_deferred_status_request(MitsubishiCN105TestsContext &ctx, uint32_t now) {
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::DEFERRED_STATUS_REQUEST);
  ctx.uart.tx.clear();
  ctx.sut.set_current_time(now);
  EXPECT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  return ctx.uart.tx.size() > 5 ? ctx.uart.tx[5] : 0;
}

// Answers the pending 0x02 request at `now` and the 0x03 request, leaving the 0x06 request sent at now + 3 ms.
static void answer_until_operating_status_request(MitsubishiCN105TestsContext &ctx, uint32_t now, bool power_on = false,
                                                  uint8_t raw_mode = 0x08) {
  push_settings_response(ctx.uart, power_on, raw_mode);
  ctx.sut.set_current_time(now);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(send_deferred_status_request(ctx, now + 1), 0x03);
  push_room_temperature_response(ctx.uart);
  ctx.sut.set_current_time(now + 2);
  // Initialized, but the publish waits for the end of the chain.
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_TRUE(ctx.sut.is_status_initialized());
  ASSERT_EQ(send_deferred_status_request(ctx, now + 3), 0x06);
  ctx.uart.tx.clear();
}

// Connects and answers 0x02 and 0x03, leaving the 0x06 request sent at 105 ms.
static void run_until_operating_status_request(MitsubishiCN105TestsContext &ctx, bool power_on = false,
                                               uint8_t raw_mode = 0x08) {
  ctx.sut.initialize();
  ctx.uart.push_rx({0xFC, 0x7A, 0x01, 0x30, 0x00, 0x55});
  ctx.sut.set_current_time(100);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(send_deferred_status_request(ctx, 101), 0x02);
  answer_until_operating_status_request(ctx, 102, power_on, raw_mode);
}

// Completes the first chain with operating = true, publishing at 106 ms.
static void run_until_status_published(MitsubishiCN105TestsContext &ctx, bool power_on = false,
                                       uint8_t raw_mode = 0x08) {
  run_until_operating_status_request(ctx, power_on, raw_mode);
  push_operating_status_response(ctx.uart, true);
  ctx.sut.set_current_time(106);
  ASSERT_TRUE(ctx.sut.update());
}

// Runs the next scheduled cycle from `now` (telemetry due), leaving the 0x06 request sent at now + 4 ms.
static void run_cycle_until_operating_status_request(MitsubishiCN105TestsContext &ctx, uint32_t now) {
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  ctx.sut.set_current_time(now);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(ctx.uart.tx[5], 0x02);
  answer_until_operating_status_request(ctx, now + 1);
}

// Times out the pending 0x06 request sent at `sent`, which ends the chain without reconnecting.
static void miss_operating_status(MitsubishiCN105TestsContext &ctx, uint32_t sent) {
  ctx.sut.set_current_time(sent + 1999);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  ctx.sut.set_current_time(sent + 2000);
  ctx.sut.update();
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
}

TEST(MitsubishiCN105Tests, OperatingStatusEndsTelemetryChainWithOnePublish) {
  MitsubishiCN105TestsContext ctx;
  run_until_status_published(ctx);
  EXPECT_TRUE(ctx.sut.status().operating);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  // Telemetry is not due on the next cycle, so it ends after the unchanged settings.
  ctx.sut.set_current_time(1106);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.uart.tx[5], 0x02);
  push_settings_response(ctx.uart);
  ctx.sut.set_current_time(1107);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
}

TEST(MitsubishiCN105Tests, UnitWithoutOperatingStatusInitializesAndStopsRequestingItAfterThreeMisses) {
  MitsubishiCN105TestsContext ctx;
  ctx.sut.set_telemetry_request_min_interval(0);
  run_until_operating_status_request(ctx);

  // The first miss publishes the initial status instead of reconnecting.
  ctx.sut.set_current_time(2104);
  ASSERT_FALSE(ctx.sut.update());
  ctx.sut.set_current_time(2105);
  ASSERT_TRUE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_FALSE(ctx.sut.is_operating_status_unsupported_());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  run_cycle_until_operating_status_request(ctx, 3105);
  miss_operating_status(ctx, 3109);
  EXPECT_FALSE(ctx.sut.is_operating_status_unsupported_());

  run_cycle_until_operating_status_request(ctx, 6109);
  miss_operating_status(ctx, 6113);
  EXPECT_TRUE(ctx.sut.is_operating_status_unsupported_());

  // After the third miss the telemetry chain ends after 0x03.
  ctx.sut.set_current_time(9113);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.uart.tx[5], 0x02);
  push_settings_response(ctx.uart);
  ctx.sut.set_current_time(9114);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(send_deferred_status_request(ctx, 9115), 0x03);
  ctx.uart.tx.clear();
  push_room_temperature_response(ctx.uart);
  ctx.sut.set_current_time(9116);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_TRUE(ctx.uart.tx.empty());

  // A mode change does not request an early refresh either.
  ctx.sut.set_telemetry_request_min_interval(60000);
  ctx.sut.set_current_time(10116);
  ASSERT_FALSE(ctx.sut.update());
  ctx.uart.tx.clear();
  push_settings_response(ctx.uart, false, 0x01);
  ctx.sut.set_current_time(10117);
  ASSERT_TRUE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_TRUE(ctx.uart.tx.empty());
}

TEST(MitsubishiCN105Tests, OperatingStatusAnswerResetsMissCount) {
  MitsubishiCN105TestsContext ctx;
  ctx.sut.set_telemetry_request_min_interval(0);
  run_until_operating_status_request(ctx);
  miss_operating_status(ctx, 105);
  run_cycle_until_operating_status_request(ctx, 3105);
  miss_operating_status(ctx, 3109);

  run_cycle_until_operating_status_request(ctx, 6109);
  push_operating_status_response(ctx.uart, true);
  ctx.sut.set_current_time(6114);
  ASSERT_TRUE(ctx.sut.update());
  EXPECT_TRUE(ctx.sut.status().operating);

  // Two more misses are not three in a row, and do not reconnect either.
  run_cycle_until_operating_status_request(ctx, 7114);
  miss_operating_status(ctx, 7118);
  run_cycle_until_operating_status_request(ctx, 10118);
  miss_operating_status(ctx, 10122);
  EXPECT_FALSE(ctx.sut.is_operating_status_unsupported_());
  run_cycle_until_operating_status_request(ctx, 13122);
}

TEST(MitsubishiCN105Tests, ModeChangeRefreshesOperatingStatusOncePerInterval) {
  MitsubishiCN105TestsContext ctx;
  run_until_status_published(ctx);

  // The unit reports HEAT instead of AUTO: 0x06 is read again right away, without 0x03.
  ctx.sut.set_current_time(1106);
  ASSERT_FALSE(ctx.sut.update());
  ctx.uart.tx.clear();
  push_settings_response(ctx.uart, false, 0x01);
  ctx.sut.set_current_time(1107);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::HEAT);
  ASSERT_EQ(send_deferred_status_request(ctx, 1108), 0x06);
  ctx.uart.tx.clear();
  push_operating_status_response(ctx.uart, true);
  ctx.sut.set_current_time(1109);
  ASSERT_TRUE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  // Another change within the same interval does not trigger a second early read.
  ctx.sut.set_current_time(2109);
  ASSERT_FALSE(ctx.sut.update());
  ctx.uart.tx.clear();
  push_settings_response(ctx.uart);
  ctx.sut.set_current_time(2110);
  ASSERT_TRUE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::AUTO);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_TRUE(ctx.uart.tx.empty());

  // The regular read at the interval clears the limit.
  run_cycle_until_operating_status_request(ctx, 60106);
}

TEST(MitsubishiCN105Tests, LocalModeChangeRefreshesOperatingStatus) {
  MitsubishiCN105TestsContext ctx;
  run_until_status_published(ctx);

  ctx.sut.set_mode(MitsubishiCN105::Mode::COOL);
  ctx.sut.set_current_time(107);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  ctx.uart.tx.clear();
  ctx.uart.push_rx({0xFC, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E});
  ctx.sut.set_current_time(108);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  // The next cycle reads 0x06 although the telemetry interval has not elapsed.
  ctx.sut.set_current_time(1108);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(ctx.uart.tx[5], 0x02);
  ctx.uart.tx.clear();
  push_settings_response(ctx.uart, false, 0x03);
  ctx.sut.set_current_time(1109);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(send_deferred_status_request(ctx, 1110), 0x06);
}

static constexpr uint8_t RAW_MODE_HEAT = 0x01;
static constexpr uint8_t RAW_MODE_COOL = 0x03;
static constexpr uint8_t RAW_MODE_AUTO = 0x08;
static constexpr uint32_t CYCLE_MS = 1010;

// Runs `cycles` scheduled cycles CYCLE_MS apart from `now`, answering every request with the unit reporting
// `power_on`, `raw_mode` and `running`. Returns the cycle times at which 0x06 was read; `now` moves past the last one.
static std::vector<uint32_t> run_cycles(MitsubishiCN105TestsContext &ctx, uint32_t &now, int cycles, bool power_on,
                                        uint8_t raw_mode, bool running) {
  std::vector<uint32_t> reads;
  for (int i = 0; i < cycles; ++i, now += CYCLE_MS) {
    EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
    ctx.uart.tx.clear();
    ctx.sut.set_current_time(now);
    ctx.sut.update();
    if (ctx.uart.tx.size() <= 5 || ctx.uart.tx[5] != 0x02) {
      ADD_FAILURE() << "no settings request at " << now;
      return reads;
    }
    push_settings_response(ctx.uart, power_on, raw_mode);
    uint32_t t = now + 1;
    ctx.sut.set_current_time(t);
    ctx.sut.update();
    while (ctx.sut.state_ == TestableMitsubishiCN105::State::DEFERRED_STATUS_REQUEST) {
      if (send_deferred_status_request(ctx, ++t) == 0x03) {
        push_room_temperature_response(ctx.uart);
      } else {
        reads.push_back(now);
        push_operating_status_response(ctx.uart, running);
      }
      ctx.sut.set_current_time(++t);
      ctx.sut.update();
    }
  }
  return reads;
}

TEST(MitsubishiCN105Tests, ModeChangeWhileRunningHoldsWindDownUntilStopped) {
  MitsubishiCN105TestsContext ctx;
  run_until_status_published(ctx, true, RAW_MODE_HEAT);
  ASSERT_TRUE(ctx.sut.status().operating);

  // HEAT -> COOL: "running" is wind-down from heating, and 0x06 is read every 10 s instead of once a minute.
  uint32_t now = 1106;
  EXPECT_THAT(run_cycles(ctx, now, 24, true, RAW_MODE_COOL, true), ::testing::ElementsAre(1106, 11206, 21306));
  EXPECT_FALSE(ctx.sut.status().operating);

  // A "stopped" report ends the hold.
  EXPECT_THAT(run_cycles(ctx, now, 7, true, RAW_MODE_COOL, false), ::testing::ElementsAre(31406));
  EXPECT_FALSE(ctx.sut.status().operating);

  // Back to the regular schedule, where "running" is cooling.
  EXPECT_TRUE(run_cycles(ctx, now, 28, true, RAW_MODE_COOL, true).empty());
  EXPECT_THAT(run_cycles(ctx, now, 1, true, RAW_MODE_COOL, true), ::testing::ElementsAre(60696));
  EXPECT_TRUE(ctx.sut.status().operating);
}

TEST(MitsubishiCN105Tests, WindDownHoldEndsAfterNineReads) {
  MitsubishiCN105TestsContext ctx;
  run_until_status_published(ctx, true, RAW_MODE_HEAT);

  // Nine held reads, the regular one at 60 s among them, then the tenth is trusted.
  uint32_t now = 1106;
  EXPECT_THAT(run_cycles(ctx, now, 81, true, RAW_MODE_COOL, true),
              ::testing::ElementsAre(1106, 11206, 21306, 31406, 41506, 51606, 60696, 70796, 80896));
  EXPECT_FALSE(ctx.sut.status().operating);
  EXPECT_THAT(run_cycles(ctx, now, 10, true, RAW_MODE_COOL, true), ::testing::ElementsAre(90996));
  EXPECT_TRUE(ctx.sut.status().operating);

  // The regular schedule follows.
  EXPECT_TRUE(run_cycles(ctx, now, 20, true, RAW_MODE_COOL, true).empty());
}

TEST(MitsubishiCN105Tests, PowerOffWhileRunningHoldsWindDownAfterPowerOn) {
  MitsubishiCN105TestsContext ctx;
  run_until_status_published(ctx, true, RAW_MODE_HEAT);

  uint32_t now = 1106;
  EXPECT_THAT(run_cycles(ctx, now, 3, false, RAW_MODE_HEAT, true), ::testing::ElementsAre(1106));
  EXPECT_FALSE(ctx.sut.status().operating);

  // Turned back on within the hold: the leftover run is still not reported, and the faster reads go on.
  EXPECT_THAT(run_cycles(ctx, now, 10, true, RAW_MODE_HEAT, true), ::testing::ElementsAre(11206));
  EXPECT_FALSE(ctx.sut.status().operating);
  EXPECT_THAT(run_cycles(ctx, now, 10, true, RAW_MODE_HEAT, false), ::testing::ElementsAre(21306));
  EXPECT_TRUE(run_cycles(ctx, now, 10, true, RAW_MODE_HEAT, true).empty());
  EXPECT_FALSE(ctx.sut.status().operating);
}

TEST(MitsubishiCN105Tests, LocalModeChangeWhileRunningHoldsWindDown) {
  MitsubishiCN105TestsContext ctx;
  run_until_status_published(ctx, true, RAW_MODE_HEAT);

  ctx.sut.set_mode(MitsubishiCN105::Mode::COOL);
  EXPECT_FALSE(ctx.sut.status().operating);
  ctx.sut.set_current_time(107);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  ctx.uart.push_rx({0xFC, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E});
  ctx.sut.set_current_time(108);
  ASSERT_FALSE(ctx.sut.update());

  uint32_t now = 1108;
  EXPECT_THAT(run_cycles(ctx, now, 11, true, RAW_MODE_COOL, true), ::testing::ElementsAre(1108, 11208));
  EXPECT_FALSE(ctx.sut.status().operating);
}

TEST(MitsubishiCN105Tests, AutoAndHeatOrCoolSwitchesKeepTheRun) {
  MitsubishiCN105TestsContext ctx;
  run_until_status_published(ctx, true, RAW_MODE_HEAT);

  // HEAT -> AUTO, AUTO -> COOL and COOL -> AUTO keep "running"; only the one early read follows.
  uint32_t now = 1106;
  EXPECT_THAT(run_cycles(ctx, now, 1, true, RAW_MODE_AUTO, true), ::testing::ElementsAre(1106));
  EXPECT_TRUE(ctx.sut.status().operating);
  EXPECT_TRUE(run_cycles(ctx, now, 1, true, RAW_MODE_COOL, true).empty());
  EXPECT_TRUE(run_cycles(ctx, now, 20, true, RAW_MODE_AUTO, true).empty());
  EXPECT_TRUE(ctx.sut.status().operating);

  // The same holds for local changes, until one ends the run.
  ctx.sut.set_mode(MitsubishiCN105::Mode::HEAT);
  EXPECT_TRUE(ctx.sut.status().operating);
  ctx.sut.set_mode(MitsubishiCN105::Mode::COOL);
  EXPECT_FALSE(ctx.sut.status().operating);
}

TEST(MitsubishiCN105Tests, ModeChangeWhileNotRunningStartsNoHold) {
  MitsubishiCN105TestsContext ctx;
  run_until_operating_status_request(ctx, true, RAW_MODE_HEAT);
  push_operating_status_response(ctx.uart, false);
  ctx.sut.set_current_time(106);
  ASSERT_TRUE(ctx.sut.update());

  uint32_t now = 1106;
  EXPECT_THAT(run_cycles(ctx, now, 1, true, RAW_MODE_COOL, true), ::testing::ElementsAre(1106));
  EXPECT_TRUE(ctx.sut.status().operating);
  EXPECT_TRUE(run_cycles(ctx, now, 20, true, RAW_MODE_COOL, true).empty());
}

TEST(MitsubishiCN105Tests, UnitWithoutOperatingStatusNeverStartsWindDownReads) {
  MitsubishiCN105TestsContext ctx;
  ctx.sut.set_telemetry_request_min_interval(0);
  run_until_operating_status_request(ctx, true, RAW_MODE_HEAT);
  miss_operating_status(ctx, 105);
  run_cycle_until_operating_status_request(ctx, 3105);
  miss_operating_status(ctx, 3109);
  run_cycle_until_operating_status_request(ctx, 6109);
  miss_operating_status(ctx, 6113);
  ASSERT_TRUE(ctx.sut.is_operating_status_unsupported_());
  ctx.sut.set_telemetry_request_min_interval(60000);
  uint32_t now = 9113;

  // Mode and power changes read neither 0x03 early nor 0x06.
  EXPECT_TRUE(run_cycles(ctx, now, 15, true, RAW_MODE_COOL, true).empty());
  EXPECT_TRUE(run_cycles(ctx, now, 15, false, RAW_MODE_COOL, true).empty());
  EXPECT_FALSE(ctx.sut.status().operating);
}

TEST(MitsubishiCN105Tests, NoResponseTriggersReconnect) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.initialize();
  ctx.uart.tx.clear();  // Remove first connect packet bytes

  // No response (no RX data), no retry yet
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.operation_start_ms_, 0);

  // Still no response after 1999ms, no retry yet
  ctx.sut.set_current_time(1999);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.operation_start_ms_, 0);

  // Stop waiting after 2s and retry connect
  ctx.sut.set_current_time(2000);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::CONNECTING);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8));
  EXPECT_EQ(ctx.sut.operation_start_ms_, 2000);
}

TEST(MitsubishiCN105Tests, RxWatchdogLimitsProcessingPerUpdate) {
  MitsubishiCN105TestsContext ctx;

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
  MitsubishiCN105TestsContext ctx;

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
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_update_interval(2000);
  ctx.sut.set_current_time(80000);

  // Status update completed, schedule next status update
  ctx.sut.state_ = TestableMitsubishiCN105::State::STATUS_UPDATED;
  ctx.sut.set_state(TestableMitsubishiCN105::State::SCHEDULE_NEXT_STATUS_UPDATE);

  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_EQ(ctx.sut.operation_start_ms_, 80000);

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
  EXPECT_EQ(ctx.sut.operation_start_ms_, 82000);
}

TEST(MitsubishiCN105Tests, DecodeStatusSettingsPackageTempEncodedA) {
  MitsubishiCN105TestsContext ctx;

  ctx.uart.push_rx(
      {0xFC, 0x62, 0x01, 0x30, 0x0C, 0x02, 0x00, 0x00, 0x01, 0x03, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55});

  ctx.sut.update();

  EXPECT_TRUE(ctx.sut.status().power_on);
  EXPECT_FALSE(ctx.sut.property_context_.use_temperature_encoding_b);
  EXPECT_EQ(ctx.sut.status().target_temperature, 26.0f);
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::COOL);
  EXPECT_EQ(ctx.sut.status().fan_mode, MitsubishiCN105::FanMode::QUIET);
}

TEST(MitsubishiCN105Tests, DecodeStatusSettingsPackageTempEncodedB) {
  MitsubishiCN105TestsContext ctx;

  ctx.uart.push_rx(
      {0xFC, 0x62, 0x01, 0x30, 0x0C, 0x02, 0x00, 0x00, 0x00, 0x07, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0xA5, 0xAD});

  ctx.sut.update();

  EXPECT_FALSE(ctx.sut.status().power_on);
  EXPECT_TRUE(ctx.sut.property_context_.use_temperature_encoding_b);
  EXPECT_EQ(ctx.sut.status().target_temperature, 18.5f);
  EXPECT_EQ(ctx.sut.status().mode, MitsubishiCN105::Mode::FAN_ONLY);
  EXPECT_EQ(ctx.sut.status().fan_mode, MitsubishiCN105::FanMode::SPEED_4);
}

TEST(MitsubishiCN105Tests, DecodeStatusRoomTempPackageTempEncodedA) {
  MitsubishiCN105TestsContext ctx;

  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x07, 0x03, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x5D});

  ctx.sut.update();

  EXPECT_EQ(ctx.sut.status().room_temperature, 16.0f);
}

TEST(MitsubishiCN105Tests, DecodeStatusRoomTempPackageTempEncodedB) {
  MitsubishiCN105TestsContext ctx;

  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBC, 0xA7});

  ctx.sut.update();

  EXPECT_EQ(ctx.sut.status().room_temperature, 30.0f);
}

TEST(MitsubishiCN105Tests, DecodeStatusOperatingPackage) {
  MitsubishiCN105TestsContext ctx;

  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x05, 0x06, 0x00, 0x00, 0x00, 0x01, 0x61});
  ctx.sut.update();
  EXPECT_TRUE(ctx.sut.status().operating);

  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x62});
  ctx.sut.update();
  EXPECT_FALSE(ctx.sut.status().operating);
}

TEST(MitsubishiCN105Tests, DecodeWideVanePackageHighBitNotSet) {
  MitsubishiCN105TestsContext ctx;

  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58});

  ctx.sut.update();

  EXPECT_EQ(ctx.sut.status().wide_vane_mode, MitsubishiCN105::WideVaneMode::POSITION_3);
  EXPECT_FALSE(ctx.sut.property_context_.set_wide_vane_high_bit);
}

TEST(MitsubishiCN105Tests, DecodeWideVanePackageHighBitSet) {
  MitsubishiCN105TestsContext ctx;

  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD8});

  ctx.sut.update();

  EXPECT_EQ(ctx.sut.status().wide_vane_mode, MitsubishiCN105::WideVaneMode::POSITION_3);
  EXPECT_TRUE(ctx.sut.property_context_.set_wide_vane_high_bit);
}

TEST(MitsubishiCN105Tests, ApplySettingsPowerOn) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_power(true);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B));
}

TEST(MitsubishiCN105Tests, ApplySettingsTemperatureEncodedA) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_target_temperature(23.0f);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x04, 0x00, 0x00, 0x00, 0x08,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x71));
}

TEST(MitsubishiCN105Tests, ApplySettingsTemperatureEncodedB) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.property_context_.use_temperature_encoding_b = true;
  ctx.sut.set_target_temperature(26.0f);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x00, 0xC5));
}

TEST(MitsubishiCN105Tests, ApplySettingsHalfDegreeTemperatureEncodedB) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.property_context_.use_temperature_encoding_b = true;
  ctx.sut.set_target_temperature(26.5f);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB5, 0x00, 0xC4));
}

TEST(MitsubishiCN105Tests, ApplyModeCool) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_mode(MitsubishiCN105::Mode::COOL);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x02, 0x00, 0x00, 0x03, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78));
}

TEST(MitsubishiCN105Tests, ApplyFanModeSpeed1) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_fan_mode(MitsubishiCN105::FanMode::SPEED_1);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00,
                                                  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73));
}

TEST(MitsubishiCN105Tests, ApplyVaneModeSwing) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_vane_mode(MitsubishiCN105::VaneMode::SWING);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66));
}

TEST(MitsubishiCN105Tests, ApplyWideVaneModePosition2AndHighBitNotSet) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_wide_vane_mode(MitsubishiCN105::WideVaneMode::POSITION_2);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x7A));
}

TEST(MitsubishiCN105Tests, ApplyWideVaneModePosition2AndHighBitSet) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.property_context_.set_wide_vane_high_bit = true;
  ctx.sut.set_wide_vane_mode(MitsubishiCN105::WideVaneMode::POSITION_2);
  ctx.sut.apply_settings();

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x00, 0x00, 0xFA));
}

TEST(MitsubishiCN105Tests, WriteInterruptsWaitingForNextStatusUpdate) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_update_interval(2000);
  ctx.sut.set_current_time(5000);

  // Waiting for next scheduled status update
  ctx.sut.state_ = TestableMitsubishiCN105::State::STATUS_UPDATED;
  ctx.sut.set_state(TestableMitsubishiCN105::State::SCHEDULE_NEXT_STATUS_UPDATE);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_EQ(ctx.sut.operation_start_ms_, 5000);
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);

  // Nothing to do in update (rx empty, no timeout)
  ctx.sut.set_current_time(5500);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.operation_start_ms_, 5000);
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);

  // Write new values
  ctx.sut.property_context_.use_temperature_encoding_b = true;
  ctx.sut.set_power(false);
  ctx.sut.set_target_temperature(25.0f);
  ctx.sut.set_mode(MitsubishiCN105::Mode::HEAT);
  ctx.sut.set_fan_mode(MitsubishiCN105::FanMode::AUTO);
  ctx.sut.set_vane_mode(MitsubishiCN105::VaneMode::AUTO);

  // Waiting for next status update must be interrupted and new values send to AC
  ctx.sut.set_current_time(6000);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 1000);
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x1F, 0x00, 0x00, 0x01, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB2, 0x00, 0xAB));
  // Write ACK response
  ctx.uart.push_rx({0xFC, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E});
  ctx.sut.set_current_time(6500);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  EXPECT_EQ(ctx.sut.operation_start_ms_, 6500 - 1000);
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);
}

TEST(MitsubishiCN105Tests, PendingSettingsTakePriorityOverDueTelemetry) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.status_.target_temperature = 24.0f;
  ctx.sut.status_.room_temperature = 21.0f;
  ASSERT_TRUE(ctx.sut.is_status_initialized());

  ctx.sut.state_ = TestableMitsubishiCN105::State::STATUS_UPDATED;
  ctx.sut.set_state(TestableMitsubishiCN105::State::SCHEDULE_NEXT_STATUS_UPDATE);
  ctx.sut.set_current_time(1000);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::UPDATING_STATUS);
  ctx.uart.tx.clear();

  ctx.sut.set_power(true);
  ctx.uart.push_rx({0xFC, 0x62, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00, 0x00, 0x08, 0x07,
                    0x00, 0x04, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C});

  ctx.sut.set_current_time(1001);
  ASSERT_TRUE(ctx.sut.update());
  EXPECT_TRUE(ctx.uart.tx.empty());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);

  ctx.sut.set_current_time(1002);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B));
}

TEST(MitsubishiCN105Tests, SetAndClearRemoteRoomTemp) {
  MitsubishiCN105TestsContext ctx;

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
  MitsubishiCN105TestsContext ctx;

  // Queue normal settings plus remote temperature together.
  ctx.sut.property_context_.use_temperature_encoding_b = true;
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
  EXPECT_TRUE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::REMOTE_TEMPERATURE));
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::POWER));
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::TEMPERATURE));
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::MODE));
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::FAN));

  // ACK the first write. Remote temperature should still be pending afterward.
  ctx.uart.tx.clear();
  ctx.uart.push_rx({0xFC, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E});
  ASSERT_FALSE(ctx.sut.update());

  EXPECT_TRUE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::REMOTE_TEMPERATURE));

  // The next apply sends the remote-temperature packet and clears the last pending flag.
  ctx.uart.tx.clear();
  ctx.sut.set_state(TestableMitsubishiCN105::State::APPLYING_SETTINGS);

  EXPECT_THAT(ctx.uart.tx, ::testing::ElementsAre(0xFC, 0x41, 0x01, 0x30, 0x10, 0x07, 0x01, 0x29, 0xB9, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x94));
  EXPECT_FALSE(ctx.sut.pending_updates_.any());
}

TEST(MitsubishiCN105Tests, WriteTimeoutClearsStatusUpdateWaitCreditOnReconnect) {
  MitsubishiCN105TestsContext ctx;
  ctx.sut.set_update_interval(2000);
  ctx.sut.set_current_time(5000);

  // Start in the scheduled status update wait state.
  ctx.sut.state_ = TestableMitsubishiCN105::State::STATUS_UPDATED;
  ctx.sut.set_state(TestableMitsubishiCN105::State::SCHEDULE_NEXT_STATUS_UPDATE);
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
  ASSERT_EQ(ctx.sut.operation_start_ms_, 5000);
  ASSERT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);

  // Interrupt that wait with a write so credit is accumulated.
  ctx.sut.property_context_.use_temperature_encoding_b = true;
  ctx.sut.set_power(false);
  ctx.sut.set_target_temperature(25.0f);
  ctx.sut.set_mode(MitsubishiCN105::Mode::HEAT);
  ctx.sut.set_fan_mode(MitsubishiCN105::FanMode::AUTO);
  ctx.sut.set_current_time(6000);
  ASSERT_FALSE(ctx.sut.update());
  ASSERT_EQ(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  ASSERT_EQ(ctx.sut.operation_start_ms_, 6000);
  ASSERT_EQ(ctx.sut.status_update_wait_credit_ms_, 1000);

  // Do not ACK the write. Advance time far enough to force timeout/reconnect
  // handling and verify that stale wait credit is cleared during recovery.
  ctx.sut.set_current_time(36000);
  ASSERT_FALSE(ctx.sut.update());
  EXPECT_NE(ctx.sut.state_, TestableMitsubishiCN105::State::APPLYING_SETTINGS);
  ASSERT_EQ(ctx.sut.operation_start_ms_, 36000);
  EXPECT_EQ(ctx.sut.status_update_wait_credit_ms_, 0);
}

TEST(MitsubishiCN105Tests, SetOutOfRangeRemoteRoomTempIsIgnored) {
  MitsubishiCN105TestsContext ctx;

  ctx.sut.set_remote_temperature(7.0f);
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::REMOTE_TEMPERATURE));

  ctx.sut.set_remote_temperature(40.0f);
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::REMOTE_TEMPERATURE));

  ctx.sut.set_remote_temperature(NAN);
  EXPECT_FALSE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::REMOTE_TEMPERATURE));
}

TEST(MitsubishiCN105Tests, SetMinRemoteRoomTemp) {
  MitsubishiCN105TestsContext ctx;
  ctx.sut.set_remote_temperature(8.0f);
  EXPECT_TRUE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::REMOTE_TEMPERATURE));
}

TEST(MitsubishiCN105Tests, SetMaxRemoteRoomTemp) {
  MitsubishiCN105TestsContext ctx;
  ctx.sut.set_remote_temperature(39.5f);
  EXPECT_TRUE(ctx.sut.pending_updates_.contains(TestableMitsubishiCN105::PropertyId::REMOTE_TEMPERATURE));
}

}  // namespace esphome::mitsubishi_cn105::testing
