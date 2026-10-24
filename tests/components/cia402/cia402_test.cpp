#include <gtest/gtest.h>
#include "esphome/core/application.h"
#include "esphome/components/cia402/cia402.h"
#include "../canopen/test_helpers.h"

namespace esphome::cia402::testing {
using canopen::testing::MockCANbus;
using canopen::testing::ProbeClient;

class ProbeDrive : public CiA402Drive {
 public:
  using CiA402Drive::CiA402Drive;
  void ready() {
    this->heartbeat_seen_ = true;
    this->last_heartbeat_ms_ = millis();
    this->nmt_state_ = 5;
  }
  void tick() {
    LoopBlockingGuard guard(nullptr, nullptr, millis());
    this->last_request_ms_ -= 50;
    this->loop();
  }
  void expire() { this->started_ms_ -= this->operation_timeout_ms_; }
};

class DriveTest : public ::testing::Test {
 public:
  MockCANbus bus;
  ProbeClient client{&bus};
  ProbeDrive drive{&client, 32};
  std::vector<DriveResult> results;
  void SetUp() override {
    drive.ready();
    drive.add_on_result_callback([this](DriveResult result) { results.push_back(result); });
  }
  canbus::CanFrame next() {
    drive.tick();
    client.loop();
    return bus.sent.back();
  }
  void reply(uint32_t value = 0, uint8_t size = 2) {
    auto frame = bus.sent.back();
    const uint8_t command = frame.data[0] == 0x40 ? (0x43 | ((4 - size) << 2)) : 0x60;
    client.on_frame_(
        0x5A0, false, false,
        {command, frame.data[1], frame.data[2], frame.data[3], static_cast<uint8_t>(value),
         static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)});
  }
  static uint16_t index(const canbus::CanFrame &frame) { return frame.data[1] | (frame.data[2] << 8); }
};

TEST_F(DriveTest, EnableWaitsForAcknowledgedStateTransitions) {
  ASSERT_TRUE(drive.enable());
  EXPECT_EQ(index(next()), 0x6041);
  reply(0x40);
  auto frame = next();
  EXPECT_EQ(index(frame), 0x6040);
  EXPECT_EQ(frame.data[4], 6);
  reply();
  EXPECT_EQ(index(next()), 0x6041);
  reply(0x40);
  EXPECT_EQ(index(next()), 0x6041);
  reply(0x21);
  EXPECT_EQ(next().data[4], 7);
  reply();
  EXPECT_EQ(index(next()), 0x6041);
  reply(0x23);
  EXPECT_EQ(next().data[4], 15);
  reply();
  EXPECT_EQ(index(next()), 0x6041);
  reply(0x27);
  drive.tick();
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], DriveResult::DRIVE_RESULT_SUCCESS);
}

TEST_F(DriveTest, StopDrainsOutstandingReplyWithoutContinuingEnable) {
  ASSERT_TRUE(drive.enable());
  next();
  reply(0x40);
  next();
  drive.quick_stop();
  reply();
  const auto frame = next();
  EXPECT_EQ(index(frame), 0x6040);
  EXPECT_EQ(frame.data[4], 2);
  reply();
  drive.tick();
  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0], DriveResult::DRIVE_RESULT_CANCELLED);
  EXPECT_FALSE(drive.busy());
}

TEST_F(DriveTest, WrongWidthStopsSequence) {
  ASSERT_TRUE(drive.enable());
  next();
  reply(0x40, 1);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], DriveResult::DRIVE_RESULT_INVALID_RESPONSE);
  EXPECT_EQ(next().data[4], 2);
  reply();
  EXPECT_EQ(results.size(), 1);  // Automatic recovery is not command success.
}

TEST_F(DriveTest, TimeoutDoesNotRetryMotion) {
  ASSERT_TRUE(drive.move_to(100));
  next();
  reply(0x27);
  drive.expire();
  EXPECT_EQ(next().data[4], 2);
  reply();
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], DriveResult::DRIVE_RESULT_TIMEOUT);
}

TEST_F(DriveTest, FaultNeverAutomaticallyResets) {
  ASSERT_TRUE(drive.enable());
  next();
  reply(8);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], DriveResult::DRIVE_RESULT_FAULT);
  EXPECT_EQ(next().data[4], 2);
}

TEST_F(DriveTest, FaultResetLeavesDriveDisabled) {
  ASSERT_TRUE(drive.fault_reset());
  EXPECT_EQ(next().data[4], 0);
  reply();
  EXPECT_EQ(next().data[4], 0x80);
  reply();
  EXPECT_EQ(index(next()), 0x6041);
  reply(0x40);
  EXPECT_EQ(next().data[4], 0);
  reply();
  drive.tick();
  EXPECT_FALSE(drive.busy());
}

TEST_F(DriveTest, RejectsOverlappingCommands) {
  ASSERT_TRUE(drive.enable());
  EXPECT_FALSE(drive.move_by(100));
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], DriveResult::DRIVE_RESULT_REJECTED);
}

TEST_F(DriveTest, RequiresOperationalHeartbeat) {
  ProbeDrive offline(&client, 33);
  EXPECT_FALSE(offline.enable());
  EXPECT_FALSE(offline.move_to(100));
  EXPECT_TRUE(bus.sent.empty());
}

TEST_F(DriveTest, SdoAbortCancelsSequence) {
  ASSERT_TRUE(drive.enable());
  auto frame = next();
  client.on_frame_(0x5A0, false, false, {0x80, frame.data[1], frame.data[2], 0, 0, 0, 2, 6});
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], DriveResult::DRIVE_RESULT_COMMUNICATION_ERROR);
  EXPECT_EQ(next().data[4], 2);
}

TEST_F(DriveTest, NewDisableIsNotLostWhileQuickStopAcknowledgementIsPending) {
  drive.quick_stop();
  next();
  drive.disable();
  reply();
  EXPECT_TRUE(drive.busy());
  const auto frame = next();
  EXPECT_EQ(index(frame), 0x6040);
  EXPECT_EQ(frame.data[4], 0);
  reply();
  EXPECT_FALSE(drive.busy());
}

TEST_F(DriveTest, RelativeOverflowNeverWritesTarget) {
  ASSERT_TRUE(drive.move_by(1));
  uint16_t status = 0x27;
  for (unsigned i = 0; i < 30 && results.empty(); i++) {
    const auto frame = next();
    const auto object = index(frame);
    EXPECT_NE(object, 0x607A);
    if (frame.data[0] != 0x40) {
      reply();
      continue;
    }
    if (object == 0x6041)
      reply(status, 2);
    else if (object == 0x6061)
      reply(1, 1);
    else if (object == 0x6064)
      reply(INT32_MAX, 4);
    else
      reply(0, 4);
  }
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], DriveResult::DRIVE_RESULT_REJECTED);
}

TEST_F(DriveTest, ProfilePositionCompletesAcknowledgementHandshake) {
  ASSERT_TRUE(drive.move_to(-100));
  uint16_t status = 0x27;
  bool started = false;
  bool wrote_target = false;
  for (unsigned i = 0; i < 40 && results.empty(); i++) {
    const auto frame = next();
    const auto object = index(frame);
    if (frame.data[0] != 0x40) {
      if (object == 0x607A) {
        wrote_target = true;
        EXPECT_EQ(encode_uint32(frame.data[7], frame.data[6], frame.data[5], frame.data[4]),
                  static_cast<uint32_t>(-100));
      }
      if (object == 0x6040 && frame.data[4] == 0x3F) {
        started = true;
        status = 0x1027;
      }
      if (object == 0x6040 && frame.data[4] == 15)
        status = started ? 0x427 : 0x27;
      reply();
    } else if (object == 0x6041)
      reply(status, 2);
    else if (object == 0x6061)
      reply(1, 1);
    else if (object == 0x6064)
      reply(static_cast<uint32_t>(-100), 4);
    else
      reply(0, 4);
  }
  EXPECT_TRUE(wrote_target);
  EXPECT_TRUE(started);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], DriveResult::DRIVE_RESULT_SUCCESS);
}

}  // namespace esphome::cia402::testing
