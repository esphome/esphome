#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "esphome/components/alpha3/alpha3_command.h"

namespace esphome::alpha3::testing {
namespace {

constexpr WorkItem MAX_COMMAND{WorkKind::WORK_KIND_SET_OPERATION_MODE, ObjectKind::OBJECT_KIND_OPERATION_CONFIG, 0, 3,
                               true};
constexpr WorkItem CP_COMMAND{WorkKind::WORK_KIND_SET_CONTROL_MODE, ObjectKind::OBJECT_KIND_LOCAL_CONTROL, 0, 0, true};
constexpr std::array<uint8_t, 4> OPERATION_CURRENT{{0, 0, 0, 0}};
constexpr std::array<uint8_t, 7> CONTROL_CURRENT{{0, 6, 2, 0x7F, 0xFF, 0xFF, 0xFF}};
constexpr std::array<uint8_t, 7> MAX_LOCAL{{0, 3, 2, 0x45, 0xA4, 0x60, 0}};
constexpr std::array<uint8_t, 7> CP_LOCAL{{0, 6, 0, 0x7F, 0xFF, 0xFF, 0xFF}};
// A higher-priority source can realize Stop even after local Max was confirmed.
constexpr std::array<uint8_t, 7> REALIZED{{5, 1, 2, 0x45, 0xA4, 0x60, 0}};
constexpr std::array<uint8_t, 18> USER_CURRENT{
    {0, 0xFF, 0x45, 0xA4, 0x60, 0, 0x3F, 0x80, 0, 0, 0x3F, 0x80, 0, 0, 0x3F, 0x80, 0, 0}};
constexpr std::array<uint8_t, 18> USER_1650{
    {0, 0xFF, 0x44, 0xCE, 0x40, 0, 0x3F, 0x80, 0, 0, 0x3F, 0x80, 0, 0, 0x3F, 0x80, 0, 0}};
constexpr std::array<uint8_t, 21> MAX_FRAME{
    {0x27, 0x11, 0xE7, 0xF8, 0x0A, 0x8D, 0x56, 0, 0x64, 1, 0x54, 1, 0, 0, 4, 0, 3, 0, 0, 0xCA, 0x5A}};
constexpr std::array<uint8_t, 24> CP_FRAME{{0x27, 0x14, 0xE7, 0xF8, 0x0A, 0x90, 0x56, 0,    0x0A, 1,    0x2F, 1,
                                            0,    0,    7,    0,    6,    0,    0x7F, 0xFF, 0xFF, 0xFF, 0x48, 0x6F}};
constexpr std::array<uint8_t, 35> SPEED_FRAME{{0x27, 0x1F, 0xE7, 0xF8, 0x0A, 0x9B, 0x56, 0,    0x0E, 1,    0x2E, 1,
                                               0,    0,    0x12, 0,    0xFF, 0x44, 0xCE, 0x40, 0,    0x3F, 0x80, 0,
                                               0,    0x3F, 0x80, 0,    0,    0x3F, 0x80, 0,    0,    0xDE, 0x7C}};

WorkItem setpoint_command(float raw, ObjectKind kind = ObjectKind::OBJECT_KIND_CS_USER_CONFIG) {
  uint32_t bits;
  std::memcpy(&bits, &raw, sizeof(bits));
  return {WorkKind::WORK_KIND_SET_SETPOINT, kind, 0, bits, true};
}

void expect_no_effect(const CommandOutcome &outcome) {
  EXPECT_EQ(outcome.request, CommandRequest::COMMAND_REQUEST_NONE);
  EXPECT_EQ(outcome.publication, CommandPublication::COMMAND_PUBLICATION_NONE);
  EXPECT_EQ(outcome.write_frame.size, 0);
}

void expect_read(const CommandOutcome &outcome, ObjectKind kind) {
  EXPECT_EQ(outcome.request, CommandRequest::COMMAND_REQUEST_READ_OBJECT);
  EXPECT_EQ(outcome.object_kind, kind);
  EXPECT_EQ(outcome.write_frame.size, 0);
  EXPECT_EQ(outcome.result, CommandResult::COMMAND_RESULT_IN_PROGRESS);
}

template<size_t N> void expect_write(const CommandOutcome &outcome, const std::array<uint8_t, N> &fixture) {
  ASSERT_EQ(outcome.request, CommandRequest::COMMAND_REQUEST_WRITE_FRAME);
  EXPECT_EQ(outcome.publication, CommandPublication::COMMAND_PUBLICATION_NONE);
  EXPECT_EQ(outcome.result, CommandResult::COMMAND_RESULT_IN_PROGRESS);
  ASSERT_EQ(outcome.write_frame.size, fixture.size());
  EXPECT_TRUE(std::equal(fixture.begin(), fixture.end(), outcome.write_frame.data.begin()));
}

class Alpha3Commands : public ::testing::Test {
 protected:
  void SetUp() override {
    this->state_.unit_address = 0xE7;
    // Synthetic pump-reported type301 range; default is deliberately not a limit.
    const std::array<uint8_t, 28> payload{{0x46, 0x6A, 0x60, 0, 0, 0, 0, 0, 0x46, 0x1C, 0x40, 0}};
    SetpointLimits limits;
    ASSERT_TRUE(decode_setpoint_limits(payload.data(), payload.size(), limits));
    this->ranges_.constant_speed = {true, limits.minimum, limits.maximum};
  }

  CommandOutcome start_(const WorkItem &work) {
    return this->policy_.start(true, this->profile_, this->ranges_, work, this->state_);
  }

  template<size_t N> CommandOutcome accept_(ObjectKind kind, const std::array<uint8_t, N> &payload) {
    return this->policy_.accept_object(this->state_, kind, payload.data(), payload.size());
  }

  CommandPolicy policy_{};
  CommandState state_{};
  ProfileMatch profile_{match_profile({52, 1, 0, 7})};
  SetpointRanges ranges_{};
};

}  // namespace

TEST_F(Alpha3Commands, MaxPreservesConfigAndPublishesOnlyLocalThenRealizedReadback) {
  const auto initial = this->start_(MAX_COMMAND);
  expect_read(initial, ObjectKind::OBJECT_KIND_OPERATION_CONFIG);
  EXPECT_EQ(initial.publication, CommandPublication::COMMAND_PUBLICATION_NONE);
  ASSERT_TRUE(this->state_.active);
  EXPECT_EQ(this->state_.stage, WorkStage::WORK_STAGE_READ_CURRENT);
  auto current = OPERATION_CURRENT;
  expect_write(this->accept_(ObjectKind::OBJECT_KIND_OPERATION_CONFIG, current), MAX_FRAME);
  current.fill(0xA5);  // The command must own a copy after the assembler is reused.
  EXPECT_TRUE(std::equal(OPERATION_CURRENT.begin(), OPERATION_CURRENT.end(), this->state_.payload.current.begin()));
  EXPECT_EQ(this->state_.stage, WorkStage::WORK_STAGE_WRITE_CURRENT);
  const auto ack = this->policy_.accept_write_ack(this->state_);
  expect_read(ack, ObjectKind::OBJECT_KIND_LOCAL_OPERATION);
  EXPECT_EQ(ack.publication, CommandPublication::COMMAND_PUBLICATION_NONE);
  EXPECT_EQ(this->state_.stage, WorkStage::WORK_STAGE_VERIFY_LOCAL);
  const auto local = this->accept_(ObjectKind::OBJECT_KIND_LOCAL_OPERATION, MAX_LOCAL);
  expect_read(local, ObjectKind::OBJECT_KIND_REALIZED_OPERATION);
  EXPECT_EQ(local.publication, CommandPublication::COMMAND_PUBLICATION_OPERATION);
  EXPECT_EQ(local.confirmed_enum, 3);
  EXPECT_TRUE(this->state_.active);
  EXPECT_EQ(this->state_.stage, WorkStage::WORK_STAGE_VERIFY_REALIZED);
  const auto realized = this->accept_(ObjectKind::OBJECT_KIND_REALIZED_OPERATION, REALIZED);
  EXPECT_EQ(realized.request, CommandRequest::COMMAND_REQUEST_NONE);
  EXPECT_EQ(realized.publication, CommandPublication::COMMAND_PUBLICATION_REALIZED_STATUS);
  EXPECT_EQ(realized.realized_status.source, 5);
  EXPECT_EQ(realized.realized_status.operation, 1);
  EXPECT_EQ(realized.result, CommandResult::COMMAND_RESULT_SUCCESS);
  EXPECT_EQ(this->state_.stage, WorkStage::WORK_STAGE_COMPLETE);
}

TEST_F(Alpha3Commands, ConstantPressurePreservesSourceOperationAndSetpointIncludingNan) {
  expect_read(this->start_(CP_COMMAND), ObjectKind::OBJECT_KIND_LOCAL_CONTROL);
  expect_write(this->accept_(ObjectKind::OBJECT_KIND_LOCAL_CONTROL, CONTROL_CURRENT), CP_FRAME);
  for (size_t index = 0; index < CONTROL_CURRENT.size(); index++)
    EXPECT_EQ(this->state_.payload.requested[index], index == 2 ? 0 : CONTROL_CURRENT[index]);
  const auto ack = this->policy_.accept_write_ack(this->state_);
  expect_read(ack, ObjectKind::OBJECT_KIND_LOCAL_CONTROL);
  EXPECT_EQ(ack.publication, CommandPublication::COMMAND_PUBLICATION_NONE);
  const auto local = this->accept_(ObjectKind::OBJECT_KIND_LOCAL_CONTROL, CP_LOCAL);
  expect_read(local, ObjectKind::OBJECT_KIND_REALIZED_OPERATION);
  EXPECT_EQ(local.publication, CommandPublication::COMMAND_PUBLICATION_CONTROL);
  EXPECT_EQ(local.confirmed_enum, 0);
  EXPECT_EQ(this->accept_(ObjectKind::OBJECT_KIND_REALIZED_OPERATION, REALIZED).result,
            CommandResult::COMMAND_RESULT_SUCCESS);
}

TEST_F(Alpha3Commands, Speed1650PreservesFeedbackAndPidAndWaitsForUserObjectReadback) {
  expect_read(this->start_(setpoint_command(1650)), ObjectKind::OBJECT_KIND_CS_USER_CONFIG);
  expect_write(this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, USER_CURRENT), SPEED_FRAME);
  EXPECT_EQ(this->state_.payload.requested, USER_1650);
  const auto ack = this->policy_.accept_write_ack(this->state_);
  expect_read(ack, ObjectKind::OBJECT_KIND_CS_USER_CONFIG);
  EXPECT_EQ(ack.publication, CommandPublication::COMMAND_PUBLICATION_NONE);
  const auto local = this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, USER_1650);
  EXPECT_EQ(local.request, CommandRequest::COMMAND_REQUEST_NONE);
  EXPECT_EQ(local.publication, CommandPublication::COMMAND_PUBLICATION_SETPOINT);
  EXPECT_FLOAT_EQ(local.confirmed_setpoint, 1650);
  EXPECT_EQ(local.result, CommandResult::COMMAND_RESULT_SUCCESS);
}

TEST_F(Alpha3Commands, UsesDiscoveredAddressAndPreservesEveryNonOperationByte) {
  this->state_.unit_address = 0xE6;
  this->start_(MAX_COMMAND);
  const auto write = this->accept_(ObjectKind::OBJECT_KIND_OPERATION_CONFIG, OPERATION_CURRENT);
  constexpr std::array<uint8_t, 21> fixture{
      {0x27, 0x11, 0xE6, 0xF8, 0x0A, 0x8D, 0x56, 0, 0x64, 1, 0x54, 1, 0, 0, 4, 0, 3, 0, 0, 0xDA, 0xB8}};
  expect_write(write, fixture);
  this->policy_.reset(this->state_);
  this->state_.unit_address = 0xE6;
  this->start_(MAX_COMMAND);
  this->accept_(ObjectKind::OBJECT_KIND_OPERATION_CONFIG, std::array<uint8_t, 4>{{0xA5, 0, 0x0D, 0x7E}});
  EXPECT_EQ(this->state_.payload.requested[0], 0xA5);
  EXPECT_EQ(this->state_.payload.requested[1], 3);
  EXPECT_EQ(this->state_.payload.requested[2], 0x0D);
  EXPECT_EQ(this->state_.payload.requested[3], 0x7E);
}

TEST_F(Alpha3Commands, RejectsNotReadyUnresolvedAndNonWritableProfilesWithoutEffects) {
  auto rejected = this->policy_.start(false, this->profile_, this->ranges_, MAX_COMMAND, this->state_);
  expect_no_effect(rejected);
  EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_NOT_READY);
  EXPECT_FALSE(this->state_.active);
  this->state_.unit_address = GENI_BROADCAST_ADDRESS;
  rejected = this->start_(MAX_COMMAND);
  expect_no_effect(rejected);
  EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_NOT_READY);
  this->state_.unit_address = 0xE7;
  for (const ProfileMatch profile : {ProfileMatch{}, match_profile({51, 1, 0, 7}), match_profile({52, 1, 1, 7}),
                                     ProfileMatch{this->profile_.profile, true, false}}) {
    rejected = this->policy_.start(true, profile, this->ranges_, MAX_COMMAND, this->state_);
    expect_no_effect(rejected);
    EXPECT_EQ(rejected.result, CommandResult::COMMAND_RESULT_FAILURE);
    EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_PROFILE_NOT_WRITABLE);
    EXPECT_FALSE(this->state_.active);
  }
}

TEST_F(Alpha3Commands, RejectsMissingCapabilitiesAndUnsupportedCommands) {
  DeviceProfile restricted = *this->profile_.profile;
  restricted.write_capabilities = 0;
  auto rejected = this->policy_.start(true, {&restricted, true, true}, this->ranges_, MAX_COMMAND, this->state_);
  expect_no_effect(rejected);
  EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_CAPABILITY_MISSING);
  for (WorkItem work : {MAX_COMMAND, CP_COMMAND}) {
    for (uint32_t value : {4U, 255U, 256U}) {
      work.argument_bits = value;
      rejected = this->start_(work);
      expect_no_effect(rejected);
      EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_UNSUPPORTED_VALUE);
      EXPECT_FALSE(this->state_.active);
    }
  }
  rejected = this->start_(setpoint_command(100, ObjectKind::OBJECT_KIND_LOCAL_OPERATION));
  expect_no_effect(rejected);
  EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_UNSUPPORTED_VALUE);
}

TEST_F(Alpha3Commands, RequiresFiniteReportedLimitsAndInclusiveRawRange) {
  for (const SetpointRange range : {SetpointRange{}, SetpointRange{true, 2000, 1000},
                                    SetpointRange{true, 0, std::numeric_limits<float>::infinity()},
                                    SetpointRange{true, std::numeric_limits<float>::quiet_NaN(), 10000}}) {
    this->ranges_.constant_speed = range;
    const auto rejected = this->start_(setpoint_command(1650));
    expect_no_effect(rejected);
    EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_RANGE_UNAVAILABLE);
    EXPECT_FALSE(this->state_.active);
  }
  this->ranges_.constant_speed = {true, 1000, 2000};
  for (float value : {std::nextafter(1000.0F, 0.0F), std::nextafter(2000.0F, 3000.0F),
                      std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    const auto rejected = this->start_(setpoint_command(value));
    expect_no_effect(rejected);
    EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_OUT_OF_RANGE);
    EXPECT_FALSE(this->state_.active);
  }
  for (float value : {1000.0F, 2000.0F}) {
    expect_read(this->start_(setpoint_command(value)), ObjectKind::OBJECT_KIND_CS_USER_CONFIG);
    this->policy_.reset(this->state_);
    this->state_.unit_address = 0xE7;
  }
}

TEST_F(Alpha3Commands, MapsEachSetpointToItsOwnRangeAndUserObject) {
  for (const auto kind : {ObjectKind::OBJECT_KIND_CS_USER_CONFIG, ObjectKind::OBJECT_KIND_CP_USER_CONFIG,
                          ObjectKind::OBJECT_KIND_PP_USER_CONFIG}) {
    this->ranges_.clear();
    *this->ranges_.find(kind) = {true, 1000, 2000};
    expect_read(this->start_(setpoint_command(1650, kind)), kind);
    const auto write = this->accept_(kind, USER_CURRENT);
    ASSERT_EQ(write.request, CommandRequest::COMMAND_REQUEST_WRITE_FRAME);
    const uint8_t sub_id = kind == ObjectKind::OBJECT_KIND_CS_USER_CONFIG   ? 14
                           : kind == ObjectKind::OBJECT_KIND_CP_USER_CONFIG ? 16
                                                                            : 18;
    EXPECT_EQ(write.write_frame.data[8], sub_id);
    expect_read(this->policy_.accept_write_ack(this->state_), kind);
    const auto local = this->accept_(kind, USER_1650);
    EXPECT_FLOAT_EQ(local.confirmed_setpoint, 1650);
    EXPECT_EQ(local.result, CommandResult::COMMAND_RESULT_SUCCESS);
    this->policy_.reset(this->state_);
    this->state_.unit_address = 0xE7;
  }
}

TEST_F(Alpha3Commands, RejectsWrongObjectsAndMalformedPrerequisitesWithoutWrites) {
  for (const auto work : {MAX_COMMAND, CP_COMMAND, setpoint_command(1650)}) {
    for (const size_t size : {0U, 3U, 5U, 6U, 8U, 17U, 19U}) {
      this->start_(work);
      const std::array<uint8_t, 19> payload{};
      const auto rejected = this->policy_.accept_object(this->state_, work.object_kind, payload.data(), size);
      expect_no_effect(rejected);
      EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_INVALID_PAYLOAD);
      EXPECT_EQ(rejected.result, CommandResult::COMMAND_RESULT_FAILURE);
      this->policy_.reset(this->state_);
      this->state_.unit_address = 0xE7;
    }
    this->start_(work);
    const auto wrong = this->accept_(ObjectKind::OBJECT_KIND_REALIZED_OPERATION, REALIZED);
    expect_no_effect(wrong);
    EXPECT_EQ(wrong.error, CommandError::COMMAND_ERROR_UNEXPECTED_OBJECT);
    this->policy_.reset(this->state_);
    this->state_.unit_address = 0xE7;
  }
}

TEST_F(Alpha3Commands, LocalMismatchStillReadsRealizedAndNeverRetriesWrite) {
  for (const auto work : {MAX_COMMAND, CP_COMMAND}) {
    this->start_(work);
    if (work.kind == WorkKind::WORK_KIND_SET_OPERATION_MODE) {
      this->accept_(work.object_kind, OPERATION_CURRENT);
    } else {
      this->accept_(work.object_kind, CONTROL_CURRENT);
    }
    const auto ack = this->policy_.accept_write_ack(this->state_);
    const auto local = this->accept_(ack.object_kind, CONTROL_CURRENT);
    expect_read(local, ObjectKind::OBJECT_KIND_REALIZED_OPERATION);
    EXPECT_FALSE(this->state_.local_value_matched);
    EXPECT_NE(local.publication, CommandPublication::COMMAND_PUBLICATION_NONE);
    const auto complete = this->accept_(ObjectKind::OBJECT_KIND_REALIZED_OPERATION, REALIZED);
    EXPECT_EQ(complete.result, CommandResult::COMMAND_RESULT_FAILURE);
    EXPECT_EQ(complete.error, CommandError::COMMAND_ERROR_VERIFICATION_MISMATCH);
    EXPECT_EQ(complete.request, CommandRequest::COMMAND_REQUEST_NONE);
    EXPECT_EQ(complete.write_frame.size, 0);
    expect_no_effect(this->policy_.accept_write_ack(this->state_));
    this->policy_.reset(this->state_);
    this->state_.unit_address = 0xE7;
  }
}

TEST_F(Alpha3Commands, SetpointReadbackToleranceAndMismatchNeverCauseAnotherWrite) {
  for (const auto entry : {std::pair{1650.01F, true}, std::pair{1650.02F, false}, std::pair{5260.0F, false}}) {
    this->start_(setpoint_command(1650));
    this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, USER_CURRENT);
    this->policy_.accept_write_ack(this->state_);
    std::array<uint8_t, 18> readback;
    ASSERT_TRUE(replace_setpoint(USER_CURRENT.data(), USER_CURRENT.size(), entry.first, readback));
    const auto complete = this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, readback);
    EXPECT_EQ(complete.result,
              entry.second ? CommandResult::COMMAND_RESULT_SUCCESS : CommandResult::COMMAND_RESULT_FAILURE);
    EXPECT_EQ(complete.publication, CommandPublication::COMMAND_PUBLICATION_SETPOINT);
    EXPECT_FLOAT_EQ(complete.confirmed_setpoint, entry.first);
    EXPECT_EQ(complete.request, CommandRequest::COMMAND_REQUEST_NONE);
    EXPECT_EQ(complete.write_frame.size, 0);
    this->policy_.reset(this->state_);
    this->state_.unit_address = 0xE7;
  }
}

TEST_F(Alpha3Commands, NegativeAckAndTimeoutOnlyAdvanceToVerification) {
  for (bool timeout : {false, true}) {
    for (const auto work : {MAX_COMMAND, CP_COMMAND, setpoint_command(1650)}) {
      this->start_(work);
      if (work.kind == WorkKind::WORK_KIND_SET_OPERATION_MODE) {
        this->accept_(work.object_kind, OPERATION_CURRENT);
      } else if (work.kind == WorkKind::WORK_KIND_SET_CONTROL_MODE) {
        this->accept_(work.object_kind, CONTROL_CURRENT);
      } else {
        this->accept_(work.object_kind, USER_CURRENT);
      }
      if (timeout) {
        TransportState transport;
        transport.transaction.active = true;
        transport.transaction.is_write = true;
        EXPECT_EQ(transport.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_VERIFY_WRITE);
      } else {
        const uint8_t negative_ack = 1;
        const ParsedFrame ack{GENI_OBJECT_CLASS, GeniOperation::GENI_OPERATION_SET, &negative_ack, 1};
        EXPECT_EQ(parse_write_ack(ack), ParseResult::PARSE_RESULT_INVALID_ACK);
      }
      const auto uncertain = this->policy_.handle_write_uncertain(this->state_);
      expect_read(uncertain, work.kind == WorkKind::WORK_KIND_SET_OPERATION_MODE
                                 ? ObjectKind::OBJECT_KIND_LOCAL_OPERATION
                                 : work.object_kind);
      EXPECT_EQ(uncertain.publication, CommandPublication::COMMAND_PUBLICATION_NONE);
      EXPECT_TRUE(this->state_.active);
      expect_no_effect(this->policy_.handle_write_uncertain(this->state_));
      this->policy_.reset(this->state_);
      this->state_.unit_address = 0xE7;
    }
  }
}

TEST_F(Alpha3Commands, ResetBetweenWriteAndVerificationCannotResumeOrPublish) {
  this->start_(MAX_COMMAND);
  this->accept_(ObjectKind::OBJECT_KIND_OPERATION_CONFIG, OPERATION_CURRENT);
  this->policy_.reset(this->state_);
  EXPECT_FALSE(this->state_.active);
  EXPECT_EQ(this->state_.unit_address, GENI_BROADCAST_ADDRESS);
  EXPECT_EQ(this->state_.payload.size, 0);
  expect_no_effect(this->policy_.accept_write_ack(this->state_));
  expect_no_effect(this->policy_.handle_write_uncertain(this->state_));
  expect_no_effect(this->accept_(ObjectKind::OBJECT_KIND_LOCAL_OPERATION, MAX_LOCAL));
  EXPECT_EQ(this->start_(MAX_COMMAND).error, CommandError::COMMAND_ERROR_NOT_READY);
}

TEST_F(Alpha3Commands, KeepsMaxActiveThroughRealizedVerificationBeforeNextCommand) {
  WorkQueue queue;
  ASSERT_EQ(queue.enqueue(MAX_COMMAND, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  ASSERT_EQ(queue.enqueue(CP_COMMAND, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  EXPECT_EQ(queue.enqueue(CP_COMMAND, nullptr), EnqueueResult::ENQUEUE_RESULT_COALESCED);
  WorkItem work;
  ASSERT_TRUE(queue.pop_next(work));
  expect_read(this->start_(work), ObjectKind::OBJECT_KIND_OPERATION_CONFIG);
  EXPECT_EQ(queue.enqueue(MAX_COMMAND, &this->state_.work), EnqueueResult::ENQUEUE_RESULT_COALESCED);
  this->accept_(ObjectKind::OBJECT_KIND_OPERATION_CONFIG, OPERATION_CURRENT);
  this->policy_.accept_write_ack(this->state_);
  this->accept_(ObjectKind::OBJECT_KIND_LOCAL_OPERATION, MAX_LOCAL);
  EXPECT_TRUE(this->state_.active);
  EXPECT_EQ(queue.size(), 1);
  const auto premature = this->start_(CP_COMMAND);
  expect_no_effect(premature);
  EXPECT_EQ(premature.error, CommandError::COMMAND_ERROR_UNEXPECTED_STATE);
  EXPECT_EQ(this->state_.work.kind, WorkKind::WORK_KIND_SET_OPERATION_MODE);
  EXPECT_EQ(this->accept_(ObjectKind::OBJECT_KIND_REALIZED_OPERATION, REALIZED).result,
            CommandResult::COMMAND_RESULT_SUCCESS);
  this->policy_.reset(this->state_);
  this->state_.unit_address = 0xE7;
  ASSERT_TRUE(queue.pop_next(work));
  expect_read(this->start_(work), ObjectKind::OBJECT_KIND_LOCAL_CONTROL);
}

TEST_F(Alpha3Commands, KeepsDistinctSetpointsInOrderAndCoalescesIdenticalPendingValues) {
  WorkQueue queue;
  EXPECT_EQ(queue.enqueue(setpoint_command(1650), nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  EXPECT_EQ(queue.enqueue(setpoint_command(1660), nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  EXPECT_EQ(queue.enqueue(setpoint_command(1660), nullptr), EnqueueResult::ENQUEUE_RESULT_COALESCED);
  for (float raw : {1650.0F, 1660.0F}) {
    WorkItem work;
    ASSERT_TRUE(queue.pop_next(work));
    EXPECT_TRUE(work.equivalent(setpoint_command(raw)));
    this->start_(work);
    EXPECT_FLOAT_EQ(this->state_.requested_raw, raw);
    this->accept_(work.object_kind, USER_CURRENT);
    this->policy_.accept_write_ack(this->state_);
    std::array<uint8_t, 18> readback;
    ASSERT_TRUE(replace_setpoint(USER_CURRENT.data(), USER_CURRENT.size(), raw, readback));
    EXPECT_EQ(this->accept_(work.object_kind, readback).result, CommandResult::COMMAND_RESULT_SUCCESS);
    this->policy_.reset(this->state_);
    this->state_.unit_address = 0xE7;
  }
  EXPECT_EQ(queue.size(), 0);
}

TEST_F(Alpha3Commands, NoChangeOperationWritesTheExactPrerequisiteBytesOnce) {
  this->start_(MAX_COMMAND);
  const std::array<uint8_t, 4> already_max{{0xA5, 3, 0x0D, 0x7E}};
  const auto write = this->accept_(ObjectKind::OBJECT_KIND_OPERATION_CONFIG, already_max);
  ASSERT_EQ(write.request, CommandRequest::COMMAND_REQUEST_WRITE_FRAME);
  EXPECT_TRUE(std::equal(already_max.begin(), already_max.end(), write.write_frame.data.begin() + 15));
  expect_read(this->policy_.accept_write_ack(this->state_), ObjectKind::OBJECT_KIND_LOCAL_OPERATION);
  expect_read(this->accept_(ObjectKind::OBJECT_KIND_LOCAL_OPERATION, MAX_LOCAL),
              ObjectKind::OBJECT_KIND_REALIZED_OPERATION);
  const auto complete = this->accept_(ObjectKind::OBJECT_KIND_REALIZED_OPERATION, REALIZED);
  EXPECT_EQ(complete.result, CommandResult::COMMAND_RESULT_SUCCESS);
  EXPECT_EQ(complete.write_frame.size, 0);
  expect_no_effect(this->accept_(ObjectKind::OBJECT_KIND_OPERATION_CONFIG, already_max));
}

TEST_F(Alpha3Commands, UncertainWriteCanSucceedOrFailBasedOnlyOnReadback) {
  for (bool matched : {true, false}) {
    this->start_(setpoint_command(1650));
    expect_write(this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, USER_CURRENT), SPEED_FRAME);
    const auto uncertain = this->policy_.handle_write_uncertain(this->state_);
    expect_read(uncertain, ObjectKind::OBJECT_KIND_CS_USER_CONFIG);
    EXPECT_EQ(uncertain.publication, CommandPublication::COMMAND_PUBLICATION_NONE);
    const auto confirmed = this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, matched ? USER_1650 : USER_CURRENT);
    EXPECT_EQ(confirmed.result,
              matched ? CommandResult::COMMAND_RESULT_SUCCESS : CommandResult::COMMAND_RESULT_FAILURE);
    EXPECT_EQ(confirmed.publication, CommandPublication::COMMAND_PUBLICATION_SETPOINT);
    EXPECT_FLOAT_EQ(confirmed.confirmed_setpoint, matched ? 1650 : 5260);
    EXPECT_EQ(confirmed.write_frame.size, 0);
    expect_no_effect(this->policy_.handle_write_uncertain(this->state_));
    this->policy_.reset(this->state_);
    this->state_.unit_address = 0xE7;
  }
}

TEST_F(Alpha3Commands, RejectsInvalidReadbackWithoutPublicationOrDeferredWrite) {
  for (bool setpoint : {false, true}) {
    for (bool wrong_size : {false, true}) {
      const auto work = setpoint ? setpoint_command(1650) : MAX_COMMAND;
      this->start_(work);
      if (setpoint) {
        this->accept_(work.object_kind, USER_CURRENT);
      } else {
        this->accept_(work.object_kind, OPERATION_CURRENT);
      }
      const auto ack = this->policy_.accept_write_ack(this->state_);
      const auto rejected =
          this->policy_.accept_object(this->state_, ack.object_kind, wrong_size ? USER_CURRENT.data() : nullptr,
                                      wrong_size ? 17 : (setpoint ? 18 : 7));
      expect_no_effect(rejected);
      EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_INVALID_PAYLOAD);
      EXPECT_EQ(rejected.result, CommandResult::COMMAND_RESULT_FAILURE);
      expect_no_effect(this->accept_(work.object_kind, USER_CURRENT));
      this->policy_.reset(this->state_);
      this->state_.unit_address = 0xE7;
    }
  }
  this->start_(setpoint_command(1650));
  this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, USER_CURRENT);
  this->policy_.accept_write_ack(this->state_);
  auto nonfinite = USER_CURRENT;
  nonfinite[2] = 0x7F;
  nonfinite[3] = 0x80;
  nonfinite[4] = 0;
  nonfinite[5] = 0;
  const auto rejected = this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, nonfinite);
  expect_no_effect(rejected);
  EXPECT_EQ(rejected.error, CommandError::COMMAND_ERROR_INVALID_PAYLOAD);
}

TEST_F(Alpha3Commands, SetpointUsesAbsoluteToleranceNearZero) {
  for (const auto entry : {std::pair{0.001F, true}, std::pair{0.0011F, false}}) {
    this->start_(setpoint_command(0));
    this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, USER_CURRENT);
    this->policy_.accept_write_ack(this->state_);
    std::array<uint8_t, 18> readback;
    ASSERT_TRUE(replace_setpoint(USER_CURRENT.data(), USER_CURRENT.size(), entry.first, readback));
    const auto complete = this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, readback);
    EXPECT_EQ(complete.result,
              entry.second ? CommandResult::COMMAND_RESULT_SUCCESS : CommandResult::COMMAND_RESULT_FAILURE);
    EXPECT_FLOAT_EQ(complete.confirmed_setpoint, entry.first);
    this->policy_.reset(this->state_);
    this->state_.unit_address = 0xE7;
  }
}

TEST_F(Alpha3Commands, LateWriteAckLeavesVerificationPendingUntilConfirmedReadback) {
  constexpr std::array<uint8_t, 9> ack_get{{0x24, 0x05, 0xF8, 0xE7, 0x0A, 0x01, 0x00, 0xAE, 0xA2}};
  constexpr std::array<uint8_t, 9> ack_set{{0x24, 0x05, 0xF8, 0xE7, 0x0A, 0x81, 0x00, 0xB5, 0x3A}};
  constexpr std::array<uint8_t, 25> verified_object{
      {0, 1, 0x2E, 1, 0, 0, 18, 0, 0xFF, 0x44, 0xCE, 0x40, 0, 0x3F, 0x80, 0, 0, 0x3F, 0x80, 0, 0, 0x3F, 0x80, 0, 0}};
  const ParsedFrame readback{GENI_OBJECT_CLASS, GeniOperation::GENI_OPERATION_GET, verified_object.data(),
                             verified_object.size()};
  const ObjectSchema &schema = *find_schema(*this->profile_.profile, ObjectKind::OBJECT_KIND_CS_USER_CONFIG);
  for (const auto &late_ack : {ack_get, ack_set}) {
    for (bool timeout_again : {false, true}) {
      this->start_(setpoint_command(1650));
      const auto write = this->accept_(ObjectKind::OBJECT_KIND_CS_USER_CONFIG, USER_CURRENT);
      expect_write(write, SPEED_FRAME);
      size_t writes = write.request == CommandRequest::COMMAND_REQUEST_WRITE_FRAME ? 1 : 0;
      TransportState transport;
      transport.transaction.active = true;
      transport.transaction.is_write = true;
      transport.transaction.work = this->state_.work;
      transport.transaction.frame = write.write_frame;
      transport.transaction.sent_offset = write.write_frame.size;
      ASSERT_EQ(transport.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_VERIFY_WRITE);
      const auto verify = this->policy_.handle_write_uncertain(this->state_);
      expect_read(verify, ObjectKind::OBJECT_KIND_CS_USER_CONFIG);
      transport.transaction.is_write = false;
      transport.transaction.expected_object = verify.object_kind;
      transport.transaction.retries_remaining = 1;
      ASSERT_TRUE(build_object_get(0xE7, schema.address, transport.transaction.frame));
      transport.transaction.sent_offset = transport.transaction.frame.size;
      writes += verify.request == CommandRequest::COMMAND_REQUEST_WRITE_FRAME ? 1 : 0;

      ParsedFrame delayed;
      ASSERT_EQ(parse_response_frame(late_ack.data(), late_ack.size(), 0xE7, delayed), ParseResult::PARSE_RESULT_OK);
      CommandOutcome ignored;
      EXPECT_FALSE(this->policy_.accept_read_response(this->state_, schema, delayed, ignored));
      expect_no_effect(ignored);
      EXPECT_EQ(ignored.result, CommandResult::COMMAND_RESULT_IN_PROGRESS);
      EXPECT_TRUE(this->state_.active);
      EXPECT_EQ(this->state_.stage, WorkStage::WORK_STAGE_VERIFY_LOCAL);
      EXPECT_TRUE(transport.transaction.active);
      EXPECT_EQ(transport.transaction.retries_remaining, 1);
      writes += ignored.request == CommandRequest::COMMAND_REQUEST_WRITE_FRAME ? 1 : 0;
      if (timeout_again) {
        EXPECT_EQ(transport.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_RETRY_READ);
        EXPECT_EQ(transport.transaction.retries_remaining, 0);
      }

      CommandOutcome confirmed;
      ASSERT_TRUE(this->policy_.accept_read_response(this->state_, schema, readback, confirmed));
      EXPECT_EQ(confirmed.result, CommandResult::COMMAND_RESULT_SUCCESS);
      EXPECT_EQ(confirmed.publication, CommandPublication::COMMAND_PUBLICATION_SETPOINT);
      EXPECT_FLOAT_EQ(confirmed.confirmed_setpoint, 1650);
      writes += confirmed.request == CommandRequest::COMMAND_REQUEST_WRITE_FRAME ? 1 : 0;
      EXPECT_EQ(writes, 1);
      this->policy_.reset(this->state_);
      this->state_.unit_address = 0xE7;
    }
  }
}

TEST_F(Alpha3Commands, UnexpectedCommandObjectResponseDoesNotConsumeThePendingRead) {
  const ObjectSchema &schema = *find_schema(*this->profile_.profile, ObjectKind::OBJECT_KIND_LOCAL_OPERATION);
  constexpr std::array<uint8_t, 14> verified_object{{0, 1, 0x2F, 1, 0, 0, 7, 0, 3, 2, 0x45, 0xA4, 0x60, 0}};
  this->start_(MAX_COMMAND);
  this->accept_(ObjectKind::OBJECT_KIND_OPERATION_CONFIG, OPERATION_CURRENT);
  this->policy_.handle_write_uncertain(this->state_);
  for (size_t offset : {1U, 3U, 6U}) {
    auto unexpected = verified_object;
    unexpected[offset] ^= 1;  // Wrong object type, version, or payload length.
    const ParsedFrame response{GENI_OBJECT_CLASS, GeniOperation::GENI_OPERATION_GET, unexpected.data(),
                               unexpected.size()};
    CommandOutcome ignored;
    EXPECT_FALSE(this->policy_.accept_read_response(this->state_, schema, response, ignored));
    expect_no_effect(ignored);
    EXPECT_EQ(this->state_.stage, WorkStage::WORK_STAGE_VERIFY_LOCAL);
  }
  const ParsedFrame response{GENI_OBJECT_CLASS, GeniOperation::GENI_OPERATION_GET, verified_object.data(),
                             verified_object.size()};
  CommandOutcome confirmed;
  ASSERT_TRUE(this->policy_.accept_read_response(this->state_, schema, response, confirmed));
  expect_read(confirmed, ObjectKind::OBJECT_KIND_REALIZED_OPERATION);
  EXPECT_EQ(confirmed.publication, CommandPublication::COMMAND_PUBLICATION_OPERATION);
  EXPECT_EQ(confirmed.confirmed_enum, 3);
  EXPECT_EQ(this->accept_(ObjectKind::OBJECT_KIND_REALIZED_OPERATION, REALIZED).result,
            CommandResult::COMMAND_RESULT_SUCCESS);
}

}  // namespace esphome::alpha3::testing
