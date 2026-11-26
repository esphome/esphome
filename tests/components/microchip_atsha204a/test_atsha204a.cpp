#include <gtest/gtest.h>

#include "esphome/components/microchip_atsha204a/microchip_atsha204a.h"
#include "esphome/core/hal.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace esphome::microchip_atsha204a::testing {

class TestTransport : public Transport {
 public:
  bool setup() override { return true; }
  void dump_config() override {}
  bool is_single_wire() const override { return this->single_wire; }
  bool wake() override {
    this->wakes++;
    return this->wake_ok;
  }
  bool command(const uint8_t *packet, size_t size) override {
    this->commands.emplace_back(packet, packet + size);
    return this->command_ok;
  }
  bool request_response() override {
    this->requests++;
    return true;
  }
  Receive receive(uint8_t *packet) override {
    if (this->reply.empty())
      return Receive::RECEIVE_PENDING;
    std::copy(this->reply.begin(), this->reply.end(), packet);
    this->reply.clear();
    return Receive::RECEIVE_COMPLETE;
  }
  bool power_down(bool idle, bool allow_nack) override {
    this->low_power_modes.push_back(idle);
    this->sleep_allows_nack = allow_nack;
    if (this->on_power_down)
      this->on_power_down();
    return this->power_down_ok;
  }

  bool single_wire{false};
  bool wake_ok{true};
  bool command_ok{true};
  bool power_down_ok{true};
  bool sleep_allows_nack{false};
  std::function<void()> on_power_down;
  int wakes{0};
  int requests{0};
  std::vector<uint8_t> reply;
  std::vector<std::vector<uint8_t>> commands;
  std::vector<bool> low_power_modes;
};

class TestChip : public ATSHA204A {
 public:
  explicit TestChip(TestTransport &transport) : ATSHA204A(&transport), transport_(transport) {}

  // Advance one transaction phase without waiting for the hardware timer.
  void advance(std::vector<uint8_t> reply = {}) {
    this->transport_.reply = std::move(reply);
    this->wait_us_ = 0;
    this->loop();
  }
  void set_awake_age(uint32_t age_ms) { this->woke_at_ = millis() - age_ms; }

 protected:
  TestTransport &transport_;
};

TEST(ATSHA204A, ValidatesWakeAndChainsCommandWithoutWakingAgain) {
  TestTransport transport;
  TestChip chip(transport);
  uint32_t held_session = 0;
  int last_status = BUSY;
  std::vector<uint8_t> result;
  const auto finished = [&](int status, const CommandResponse &data, uint32_t session) {
    last_status = status;
    result.assign(data.begin(), data.end());
    EXPECT_EQ(session, 0U);
  };
  chip.command(Opcode::OPCODE_DEV_REV, 0, 0, {}, EndMode::END_MODE_KEEP_AWAKE, 0, true,
               [&](int status, const CommandResponse &data, uint32_t session) {
                 EXPECT_EQ(status, 0);
                 EXPECT_EQ(data.size(), 4U);
                 held_session = session;
                 EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
                 chip.command(Opcode::OPCODE_DEV_REV, 0, 0, {}, EndMode::END_MODE_SLEEP, session, true, finished);
               });
  EXPECT_TRUE(HighFrequencyLoopRequester::is_high_frequency());
  chip.advance({0x04, 0x11, 0x33, 0x43});
  ASSERT_EQ(transport.commands.size(), 1U);
  EXPECT_EQ(transport.commands.front(), (std::vector<uint8_t>{0x07, 0x30, 0, 0, 0, 0x03, 0x5D}));

  chip.advance({0x07, 0, 0, 0, 0, 0x03, 0xAD});
  ASSERT_NE(held_session, 0U);
  ASSERT_EQ(transport.commands.size(), 2U);
  EXPECT_EQ(transport.wakes, 1);
  EXPECT_TRUE(HighFrequencyLoopRequester::is_high_frequency());
  chip.advance({0x07, 0, 0, 0, 0, 0x03, 0xAD});
  EXPECT_FALSE(HighFrequencyLoopRequester::is_high_frequency());
  EXPECT_EQ(last_status, 0);
  EXPECT_EQ(result, (std::vector<uint8_t>{0, 0, 0, 0}));
  EXPECT_EQ(transport.low_power_modes, (std::vector<bool>{false}));
}

TEST(ATSHA204A, RejectsCommandWithoutEnoughWatchdogTime) {
  TestTransport transport;
  TestChip chip(transport);
  uint32_t token = 0;
  chip.command(Opcode::OPCODE_DEV_REV, 0, 0, {}, EndMode::END_MODE_KEEP_AWAKE, 0, true,
               [&](int, const CommandResponse &, uint32_t session) { token = session; });
  chip.advance({0x04, 0x11, 0x33, 0x43});
  chip.advance({0x07, 0, 0, 0, 0, 0x03, 0xAD});
  ASSERT_NE(token, 0U);

  chip.set_awake_age(690);
  int result = 0;
  chip.command(Opcode::OPCODE_HMAC, 0, 0, {}, EndMode::END_MODE_SLEEP, token, true,
               [&](int status, const CommandResponse &, uint32_t) { result = status; });
  EXPECT_EQ(result, SESSION_LOST);
  EXPECT_EQ(transport.commands.size(), 1U);
  EXPECT_EQ(transport.low_power_modes, (std::vector<bool>{false}));
}

TEST(ATSHA204A, RejectsClosingSessionTokenDuringTransportCallback) {
  TestTransport transport;
  TestChip chip(transport);
  uint32_t token = 0;
  chip.command(Opcode::OPCODE_DEV_REV, 0, 0, {}, EndMode::END_MODE_KEEP_AWAKE, 0, true,
               [&](int, const CommandResponse &, uint32_t session) { token = session; });
  chip.advance({0x04, 0x11, 0x33, 0x43});
  chip.advance({0x07, 0, 0, 0, 0, 0x03, 0xAD});
  ASSERT_NE(token, 0U);

  int result = 0;
  transport.on_power_down = [&] {
    chip.command(Opcode::OPCODE_DEV_REV, 0, 0, {}, EndMode::END_MODE_SLEEP, token, true,
                 [&](int status, const CommandResponse &, uint32_t) { result = status; });
  };
  EXPECT_TRUE(chip.end_session(token));
  EXPECT_EQ(result, SESSION_LOST);
  EXPECT_EQ(transport.commands.size(), 1U);
}

TEST(ATSHA204A, RejectsBadResponseCrcWithoutRetryingCommand) {
  TestTransport transport;
  TestChip chip(transport);
  int last_status = BUSY;
  chip.command(Opcode::OPCODE_RANDOM, 0, 0, {}, EndMode::END_MODE_IDLE, 0, true,
               [&](int status, const CommandResponse &data, uint32_t session) {
                 last_status = status;
                 EXPECT_TRUE(data.empty());
                 EXPECT_EQ(session, 0U);
               });
  chip.advance({0x04, 0x11, 0x33, 0x43});
  chip.advance({0x04, 0, 0, 0});
  EXPECT_EQ(last_status, BAD_CRC);
  EXPECT_EQ(transport.commands.size(), 1U);
  EXPECT_EQ(transport.low_power_modes, (std::vector<bool>{false}));
}

TEST(ATSHA204A, PauseWithoutResponseRequiresSleep) {
  TestTransport transport;
  transport.single_wire = true;
  TestChip chip(transport);
  int last_status = BUSY;
  const auto finished = [&](int status, const CommandResponse &data, uint32_t session) {
    last_status = status;
    EXPECT_TRUE(data.empty());
    EXPECT_EQ(session, 0U);
  };
  chip.command(Opcode::OPCODE_PAUSE, 0, 0, {}, EndMode::END_MODE_KEEP_AWAKE, 0, false, finished);
  EXPECT_EQ(last_status, INVALID_ARGUMENT);
  EXPECT_EQ(transport.wakes, 0);

  for (bool power_down_ok : {true, false}) {
    transport.power_down_ok = power_down_ok;
    chip.command(Opcode::OPCODE_PAUSE, 0, 0, {}, EndMode::END_MODE_SLEEP, 0, false, finished);
    chip.advance();
    chip.advance();
    EXPECT_EQ(last_status, power_down_ok ? UNCONFIRMED : IO_ERROR);
    EXPECT_TRUE(transport.sleep_allows_nack);
  }
  EXPECT_EQ(transport.requests, 0);
  EXPECT_EQ(transport.commands.size(), 2U);
  EXPECT_EQ(transport.low_power_modes, (std::vector<bool>{false, false}));
}

TEST(ATSHA204A, PresencePollCompletesAfterDeviceRevision) {
  TestTransport transport;
  TestChip chip(transport);

  chip.update();
  chip.advance({0x04, 0x11, 0x33, 0x43});
  ASSERT_EQ(transport.commands.size(), 1U);
  EXPECT_EQ(transport.commands.front(), (std::vector<uint8_t>{0x07, 0x30, 0, 0, 0, 0x03, 0x5D}));

  chip.advance({0x07, 0, 0, 0, 0, 0x03, 0xAD});
  EXPECT_EQ(transport.commands.size(), 1U);
  EXPECT_EQ(transport.low_power_modes, (std::vector<bool>{false}));
}

TEST(ATSHA204A, EndSessionReportsImmediateFailure) {
  for (auto mode : {EndMode::END_MODE_KEEP_AWAKE, EndMode::END_MODE_IDLE}) {
    TestTransport transport;
    TestChip chip(transport);
    uint32_t token = 0;
    chip.command(Opcode::OPCODE_DEV_REV, 0, 0, {}, mode, 0, true,
                 [&](int, const CommandResponse &, uint32_t session) { token = session; });
    chip.advance({0x04, 0x11, 0x33, 0x43});
    chip.advance({0x07, 0, 0, 0, 0, 0x03, 0xAD});
    ASSERT_NE(token, 0U);

    transport.power_down_ok = false;
    transport.wake_ok = false;
    EXPECT_FALSE(chip.end_session(token));
    EXPECT_FALSE(chip.end_session(token));
    EXPECT_EQ(transport.low_power_modes, (std::vector<bool>{mode == EndMode::END_MODE_IDLE}));
  }
}

TEST(ATSHA204A, EndingIdleSessionSleepsAfterConfirmedWake) {
  for (bool confirmed : {true, false}) {
    TestTransport transport;
    TestChip chip(transport);
    uint32_t token = 0;
    chip.command(Opcode::OPCODE_DEV_REV, 0, 0, {}, EndMode::END_MODE_IDLE, 0, true,
                 [&](int, const CommandResponse &, uint32_t session) { token = session; });
    chip.advance({0x04, 0x11, 0x33, 0x43});
    chip.advance({0x07, 0, 0, 0, 0, 0x03, 0xAD});

    ASSERT_TRUE(chip.end_session(token));
    chip.advance({0x04, 0x11, 0x33, static_cast<uint8_t>(confirmed ? 0x43 : 0)});
    EXPECT_EQ(transport.low_power_modes, (confirmed ? std::vector<bool>{true, false} : std::vector<bool>{true}));
    EXPECT_FALSE(chip.end_session(token));
  }
}

}  // namespace esphome::microchip_atsha204a::testing
