#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <utility>

#include "esphome/components/alpha3/alpha3_transaction.h"

namespace esphome::alpha3::testing {
namespace {

WorkItem read_object(ObjectKind kind) { return {WorkKind::WORK_KIND_READ_OBJECT, kind, 0, 0, false}; }

WorkItem read_parameter(uint8_t parameter_id) {
  return {WorkKind::WORK_KIND_READ_PARAMETER, ObjectKind::OBJECT_KIND_ELECTRICAL, parameter_id, 0, false};
}

WorkItem command(WorkKind kind, uint8_t value) {
  return {kind, ObjectKind::OBJECT_KIND_LOCAL_OPERATION, 0, value, true};
}

uint32_t float_bits(float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

void ready_transport(TransportState &state) {
  state.readiness.mark_service_found(0x1234, 0x5678);
  state.readiness.mark_authenticated();
  state.readiness.mark_registration_started();
  state.readiness.mark_registration_succeeded();
  state.readiness.mark_cccd_succeeded();
}

void send_discovery(TransportState &state) {
  ASSERT_TRUE(state.queue.pop_next(state.transaction.work));
  ASSERT_EQ(state.transaction.work.kind, WorkKind::WORK_KIND_DISCOVER_ADDRESS);
  state.transaction.active = true;
  ASSERT_TRUE(build_discovery_get(state.transaction.frame));
  state.transaction.sent_offset = state.transaction.frame.size;
}

void expect_identity_reads(TransportState &state, uint8_t destination) {
  ASSERT_EQ(state.queue.size(), 3U);
  for (const uint8_t parameter_id : {148, 149, 150}) {
    WorkItem work;
    ASSERT_TRUE(state.queue.pop_next(work));
    EXPECT_TRUE(work.equivalent(read_parameter(parameter_id)));
    EncodedFrame request;
    ASSERT_TRUE(build_parameter_get(state.unit_address, work.parameter_id, request));
    EXPECT_EQ(request.data[2], destination);
  }
}

}  // namespace

TEST(Alpha3WorkQueue, PreservesFifoOrderAmongUserCommands) {
  WorkQueue queue;
  const WorkItem first = command(WorkKind::WORK_KIND_SET_OPERATION_MODE, 1);
  const WorkItem second = command(WorkKind::WORK_KIND_SET_CONTROL_MODE, 2);
  const WorkItem third = {WorkKind::WORK_KIND_SET_SETPOINT, ObjectKind::OBJECT_KIND_CS_USER_CONFIG, 0,
                          float_bits(1234.5F), true};
  EXPECT_EQ(queue.enqueue(first, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  EXPECT_EQ(queue.enqueue(second, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  EXPECT_EQ(queue.enqueue(third, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);

  WorkItem output;
  ASSERT_TRUE(queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(first));
  ASSERT_TRUE(queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(second));
  ASSERT_TRUE(queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(third));
  EXPECT_FALSE(queue.pop_next(output));
}

TEST(Alpha3WorkQueue, ServesCommandsBeforeQueuedPolls) {
  WorkQueue queue;
  const WorkItem poll = read_object(ObjectKind::OBJECT_KIND_ELECTRICAL);
  const WorkItem first_command = command(WorkKind::WORK_KIND_SET_OPERATION_MODE, 1);
  const WorkItem second_command = command(WorkKind::WORK_KIND_SET_CONTROL_MODE, 2);
  ASSERT_EQ(queue.enqueue(poll, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  ASSERT_EQ(queue.enqueue(first_command, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  ASSERT_EQ(queue.enqueue(second_command, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);

  WorkItem output;
  ASSERT_TRUE(queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(first_command));
  ASSERT_TRUE(queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(second_command));
  ASSERT_TRUE(queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(poll));
}

TEST(Alpha3WorkQueue, CoalescesDuplicatePollsAgainstQueueAndActiveRead) {
  WorkQueue queue;
  const WorkItem poll = read_object(ObjectKind::OBJECT_KIND_ELECTRICAL);
  EXPECT_EQ(queue.enqueue(poll, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  EXPECT_EQ(queue.enqueue(poll, nullptr), EnqueueResult::ENQUEUE_RESULT_COALESCED);
  EXPECT_EQ(queue.enqueue(poll, &poll), EnqueueResult::ENQUEUE_RESULT_COALESCED);
  EXPECT_EQ(queue.size(), 1U);
  EXPECT_TRUE(queue.contains(poll));
}

TEST(Alpha3WorkQueue, CoalescesOnlyByteIdenticalCommandsAndPreservesDistinctCommands) {
  WorkQueue queue;
  const WorkItem active = command(WorkKind::WORK_KIND_SET_OPERATION_MODE, 1);
  const WorkItem matching = command(WorkKind::WORK_KIND_SET_OPERATION_MODE, 1);
  const WorkItem distinct = command(WorkKind::WORK_KIND_SET_OPERATION_MODE, 2);
  EXPECT_EQ(queue.enqueue(matching, &active), EnqueueResult::ENQUEUE_RESULT_COALESCED);
  EXPECT_EQ(queue.enqueue(matching, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  EXPECT_EQ(queue.enqueue(distinct, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  EXPECT_EQ(queue.size(), 2U);

  WorkItem output;
  ASSERT_TRUE(queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(matching));
  ASSERT_TRUE(queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(distinct));
}

TEST(Alpha3WorkQueue, RejectsPollWhenFull) {
  WorkQueue queue;
  for (uint8_t value = 0; value < WORK_QUEUE_CAPACITY; value++)
    ASSERT_EQ(queue.enqueue(command(WorkKind::WORK_KIND_SET_OPERATION_MODE, value), nullptr),
              EnqueueResult::ENQUEUE_RESULT_ACCEPTED);

  EXPECT_EQ(queue.enqueue(read_object(ObjectKind::OBJECT_KIND_ELECTRICAL), nullptr),
            EnqueueResult::ENQUEUE_RESULT_REJECTED_FULL);
  EXPECT_EQ(queue.size(), WORK_QUEUE_CAPACITY);
}

TEST(Alpha3WorkQueue, CommandEvictsNewestPollWhenFull) {
  WorkQueue queue;
  const WorkItem oldest_poll = read_object(ObjectKind::OBJECT_KIND_ELECTRICAL);
  const WorkItem newest_poll = read_object(ObjectKind::OBJECT_KIND_HISTORY);
  ASSERT_EQ(queue.enqueue(oldest_poll, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  for (uint8_t value = 0; value < WORK_QUEUE_CAPACITY - 2; value++)
    ASSERT_EQ(queue.enqueue(command(WorkKind::WORK_KIND_SET_OPERATION_MODE, value), nullptr),
              EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  ASSERT_EQ(queue.enqueue(newest_poll, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);

  const WorkItem replacement = command(WorkKind::WORK_KIND_SET_CONTROL_MODE, 99);
  EXPECT_EQ(queue.enqueue(replacement, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  EXPECT_EQ(queue.size(), WORK_QUEUE_CAPACITY);
  EXPECT_TRUE(queue.contains(oldest_poll));
  EXPECT_FALSE(queue.contains(newest_poll));
  EXPECT_TRUE(queue.contains(replacement));
}

TEST(Alpha3WorkQueue, RejectsCommandWhenFullOfCommands) {
  WorkQueue queue;
  for (uint8_t value = 0; value < WORK_QUEUE_CAPACITY; value++)
    ASSERT_EQ(queue.enqueue(command(WorkKind::WORK_KIND_SET_OPERATION_MODE, value), nullptr),
              EnqueueResult::ENQUEUE_RESULT_ACCEPTED);

  EXPECT_EQ(queue.enqueue(command(WorkKind::WORK_KIND_SET_CONTROL_MODE, 1), nullptr),
            EnqueueResult::ENQUEUE_RESULT_REJECTED_FULL);
  EXPECT_EQ(queue.size(), WORK_QUEUE_CAPACITY);
}

TEST(Alpha3WorkQueue, ClearRemovesAllQueuedItems) {
  WorkQueue queue;
  const WorkItem poll = read_object(ObjectKind::OBJECT_KIND_ELECTRICAL);
  ASSERT_EQ(queue.enqueue(poll, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  ASSERT_EQ(queue.enqueue(command(WorkKind::WORK_KIND_SET_OPERATION_MODE, 1), nullptr),
            EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  queue.clear();

  WorkItem output;
  EXPECT_EQ(queue.size(), 0U);
  EXPECT_FALSE(queue.contains(poll));
  EXPECT_FALSE(queue.pop_next(output));
}

TEST(Alpha3Readiness, RequiresServiceAndAuthenticationBeforeRegistration) {
  ReadinessTracker tracker;
  EXPECT_FALSE(tracker.can_register());
  tracker.mark_registration_started();
  EXPECT_FALSE(tracker.ready());
  tracker.mark_service_found(0x1234, 0x5678);
  EXPECT_FALSE(tracker.can_register());
  EXPECT_FALSE(tracker.ready());
  tracker.mark_authenticated();
  EXPECT_TRUE(tracker.can_register());
}

TEST(Alpha3Readiness, AllowsEitherServiceAuthenticationOrderAndNeedsRegistrationAndCccd) {
  for (bool authenticate_first : {false, true}) {
    ReadinessTracker tracker;
    if (authenticate_first) {
      tracker.mark_authenticated();
      EXPECT_FALSE(tracker.can_register());
      tracker.mark_service_found(0x1234, 0x5678);
    } else {
      tracker.mark_service_found(0x1234, 0x5678);
      EXPECT_FALSE(tracker.can_register());
      tracker.mark_authenticated();
    }
    EXPECT_TRUE(tracker.can_register());
    tracker.mark_registration_started();
    tracker.mark_registration_succeeded();
    EXPECT_FALSE(tracker.ready());
    tracker.mark_cccd_succeeded();
    EXPECT_TRUE(tracker.ready());
  }
}

TEST(Alpha3Readiness, ResetInvalidatesHandlesAndEveryReadinessFlag) {
  ReadinessTracker tracker;
  tracker.mark_service_found(0x1234, 0x5678);
  tracker.mark_authenticated();
  tracker.mark_registration_started();
  tracker.mark_registration_succeeded();
  tracker.mark_cccd_succeeded();
  ASSERT_TRUE(tracker.ready());
  tracker.reset();

  EXPECT_EQ(tracker.characteristic_handle(), 0U);
  EXPECT_EQ(tracker.cccd_handle(), 0U);
  EXPECT_FALSE(tracker.can_register());
  EXPECT_FALSE(tracker.ready());
}

TEST(Alpha3TransportState, DistinguishesTransportReadinessFromWritableControlReadiness) {
  TransportState state;
  ready_transport(state);

  EXPECT_TRUE(state.transport_ready());
  EXPECT_FALSE(state.control_ready());

  state.unit_address = 0xE6;
  state.profile = match_profile({52, 1, 0, 0x07});
  EXPECT_TRUE(state.control_ready());

  state.profile = match_profile({52, 1, 1, 0x07});
  EXPECT_NE(state.profile.profile, nullptr);
  EXPECT_FALSE(state.control_ready());

  state.reset_on_disconnect();
  EXPECT_FALSE(state.transport_ready());
  EXPECT_FALSE(state.control_ready());
}

TEST(Alpha3TransportState, RetriesAReadOnceThenFails) {
  TransportState state;
  state.transaction.active = true;
  state.transaction.is_write = false;
  state.transaction.retries_remaining = 1;
  EXPECT_EQ(state.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_RETRY_READ);
  EXPECT_EQ(state.transaction.retries_remaining, 0);
  EXPECT_EQ(state.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_FAIL);
}

TEST(Alpha3TransportState, DiscoveryWaitsForCccdThenPrecedesIdentityReads) {
  TransportState state;
  EXPECT_FALSE(state.start_discovery());
  EXPECT_EQ(state.queue.size(), 0U);
  state.readiness.mark_service_found(0x1234, 0x5678);
  state.readiness.mark_authenticated();
  state.readiness.mark_registration_started();
  state.readiness.mark_registration_succeeded();
  EXPECT_FALSE(state.start_discovery());
  EXPECT_EQ(state.queue.size(), 0U);
  state.readiness.mark_cccd_succeeded();
  ASSERT_TRUE(state.start_discovery());
  EXPECT_FALSE(state.start_discovery());
  ASSERT_EQ(state.queue.size(), 1U);
  EXPECT_EQ(state.unit_address, GENI_BROADCAST_ADDRESS);
  send_discovery(state);
  EXPECT_EQ(state.queue.size(), 0U);
  EXPECT_FALSE(state.start_discovery());
  ASSERT_TRUE(state.accept_discovered_address(0xE6));
  EXPECT_FALSE(state.transaction.active);
  EXPECT_EQ(state.unit_address, 0xE6);
  EXPECT_FALSE(state.start_discovery());
  EXPECT_FALSE(state.accept_discovered_address(0xE5));
  expect_identity_reads(state, 0xE6);
}

TEST(Alpha3TransportState, DiscoveryTimeoutFallsBackExactlyOnceAndQueuesIdentity) {
  TransportState state;
  ready_transport(state);
  ASSERT_TRUE(state.start_discovery());
  send_discovery(state);
  state.transaction.retries_remaining = 1;
  EXPECT_EQ(state.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_DISCOVERY_FALLBACK);
  EXPECT_EQ(state.unit_address, 0xE7);
  EXPECT_FALSE(state.transaction.active);
  EXPECT_EQ(state.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_FAIL);
  EXPECT_FALSE(state.start_discovery());
  EXPECT_FALSE(state.accept_discovered_address(0xE6));
  expect_identity_reads(state, 0xE7);
  EXPECT_EQ(state.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_FAIL);
}

TEST(Alpha3TransportState, RejectsUnsolicitedBroadcastAndUnsentDiscoveryResults) {
  TransportState state;
  EXPECT_FALSE(state.accept_discovered_address(0xE6));
  ready_transport(state);
  ASSERT_TRUE(state.start_discovery());
  EXPECT_FALSE(state.accept_discovered_address(0xE6));
  send_discovery(state);
  EXPECT_FALSE(state.accept_discovered_address(GENI_BROADCAST_ADDRESS));
  state.transaction.sent_offset = 0;
  EXPECT_FALSE(state.accept_discovered_address(0xE6));
  EXPECT_EQ(state.unit_address, GENI_BROADCAST_ADDRESS);
  EXPECT_EQ(state.queue.size(), 0U);
  state.transaction.sent_offset = state.transaction.frame.size;
  ASSERT_TRUE(state.accept_discovered_address(0xE6));
  EXPECT_EQ(state.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_FAIL);
  expect_identity_reads(state, 0xE6);
}

TEST(Alpha3TransportState, DisconnectClearsDiscoveredOrFallbackAddressAndAllowsFreshDiscovery) {
  for (const bool fallback : {false, true}) {
    TransportState state;
    ready_transport(state);
    ASSERT_TRUE(state.start_discovery());
    send_discovery(state);
    if (fallback)
      ASSERT_EQ(state.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_DISCOVERY_FALLBACK);
    else
      ASSERT_TRUE(state.accept_discovered_address(0xE6));
    state.reset_on_disconnect();
    EXPECT_EQ(state.unit_address, GENI_BROADCAST_ADDRESS);
    EXPECT_EQ(state.queue.size(), 0U);
    EXPECT_FALSE(state.start_discovery());
    ready_transport(state);
    ASSERT_TRUE(state.start_discovery());
    send_discovery(state);
    EXPECT_EQ(state.queue.size(), 0U);
    ASSERT_TRUE(state.accept_discovered_address(0xE5));
    expect_identity_reads(state, 0xE5);
  }
}

TEST(Alpha3TransportState, WritesVerifyWithoutRetransmission) {
  TransportState state;
  state.transaction.active = true;
  state.transaction.is_write = true;
  state.transaction.retries_remaining = 1;
  EXPECT_EQ(state.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_VERIFY_WRITE);
  EXPECT_EQ(state.transaction.retries_remaining, 1);
  EXPECT_EQ(state.handle_timeout(), TimeoutDecision::TIMEOUT_DECISION_VERIFY_WRITE);
  EXPECT_EQ(state.transaction.retries_remaining, 1);
}

TEST(Alpha3TransportState, DisconnectResetsEveryTransportOwnedState) {
  TransportState state;
  const float retained_sensor_value = 12.5F;
  const WorkItem poll = read_object(ObjectKind::OBJECT_KIND_ELECTRICAL);
  ASSERT_EQ(state.queue.enqueue(poll, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  state.transaction.active = true;
  state.transaction.is_write = true;
  state.transaction.work = poll;
  state.transaction.expected_object = ObjectKind::OBJECT_KIND_ELECTRICAL;
  state.transaction.expected_parameter_id = 148;
  ASSERT_TRUE(build_parameter_get(0xE7, 148, state.transaction.frame));
  state.transaction.sent_offset = state.transaction.frame.size;
  state.transaction.retries_remaining = 1;
  const std::array<uint8_t, 1> partial_frame{{GENI_RESPONSE_START}};
  ASSERT_EQ(state.assembler.append(partial_frame.data(), partial_frame.size()), ParseResult::PARSE_RESULT_INCOMPLETE);
  state.readiness.mark_service_found(0x1234, 0x5678);
  state.readiness.mark_authenticated();
  state.readiness.mark_registration_started();
  state.readiness.mark_registration_succeeded();
  state.readiness.mark_cccd_succeeded();
  ASSERT_TRUE(state.readiness.ready());
  state.identity = {52, 1, 0, 0x07};
  state.profile = match_profile(state.identity);
  ASSERT_NE(state.profile.profile, nullptr);

  state.reset_on_disconnect();

  EXPECT_EQ(state.queue.size(), 0U);
  EXPECT_FALSE(state.transaction.active);
  EXPECT_FALSE(state.transaction.is_write);
  EXPECT_TRUE(state.transaction.work.equivalent(WorkItem{}));
  EXPECT_EQ(state.transaction.expected_object, ObjectKind{});
  EXPECT_EQ(state.transaction.expected_parameter_id, 0);
  EXPECT_EQ(state.transaction.frame.size, 0);
  EXPECT_EQ(state.transaction.frame.data, EncodedFrame{}.data);
  EXPECT_EQ(state.transaction.sent_offset, 0);
  EXPECT_EQ(state.transaction.retries_remaining, 0);
  EXPECT_EQ(state.assembler.size(), 0U);
  EXPECT_FALSE(state.assembler.complete());
  EXPECT_FALSE(state.readiness.ready());
  EXPECT_FALSE(state.readiness.can_register());
  EXPECT_EQ(state.readiness.characteristic_handle(), 0U);
  EXPECT_EQ(state.readiness.cccd_handle(), 0U);
  EXPECT_EQ(state.identity.family, 0);
  EXPECT_EQ(state.identity.type, 0);
  EXPECT_EQ(state.identity.version, 0);
  EXPECT_EQ(state.identity.valid_mask, 0);
  EXPECT_EQ(state.profile.profile, nullptr);
  EXPECT_FALSE(state.profile.exact);
  EXPECT_FALSE(state.profile.writable);
  // Transport reset has no entity-state parameter and cannot erase retained values.
  EXPECT_FLOAT_EQ(retained_sensor_value, 12.5F);
}

TEST(Alpha3SetpointRanges, MapsTheThreeUserConfigurationObjectsAndClearsValidity) {
  SetpointRanges ranges;
  for (const auto &[kind, expected] : std::array{
           std::pair{ObjectKind::OBJECT_KIND_CS_USER_CONFIG, &ranges.constant_speed},
           std::pair{ObjectKind::OBJECT_KIND_CP_USER_CONFIG, &ranges.constant_pressure},
           std::pair{ObjectKind::OBJECT_KIND_PP_USER_CONFIG, &ranges.proportional_pressure},
       }) {
    EXPECT_EQ(ranges.find(kind), expected);
    EXPECT_EQ(static_cast<const SetpointRanges &>(ranges).find(kind), expected);
    expected->valid = true;
  }
  for (const ObjectKind kind : {ObjectKind::OBJECT_KIND_ELECTRICAL, ObjectKind::OBJECT_KIND_CS_FACTORY_LIMITS,
                                ObjectKind::OBJECT_KIND_OPERATION_CONFIG})
    EXPECT_EQ(ranges.find(kind), nullptr);

  ranges.clear();
  EXPECT_FALSE(ranges.constant_speed.valid);
  EXPECT_FALSE(ranges.constant_pressure.valid);
  EXPECT_FALSE(ranges.proportional_pressure.valid);
}

TEST(Alpha3PeriodicReads, MapsEveryDemandToItsExactReadAndIgnoresFalseFields) {
  struct Expectation {
    bool PollDemand::*field;
    WorkKind kind;
    ObjectKind object_kind;
    uint8_t parameter_id;
  };
  const std::array expectations{
      Expectation{&PollDemand::hydraulic, WorkKind::WORK_KIND_READ_OBJECT, ObjectKind::OBJECT_KIND_HYDRAULIC_MODEL_B,
                  0},
      Expectation{&PollDemand::electrical, WorkKind::WORK_KIND_READ_OBJECT, ObjectKind::OBJECT_KIND_ELECTRICAL, 0},
      Expectation{&PollDemand::history, WorkKind::WORK_KIND_READ_OBJECT, ObjectKind::OBJECT_KIND_HISTORY, 0},
      Expectation{&PollDemand::energy, WorkKind::WORK_KIND_READ_OBJECT, ObjectKind::OBJECT_KIND_ENERGY, 0},
      Expectation{&PollDemand::alarm, WorkKind::WORK_KIND_READ_PARAMETER, ObjectKind::OBJECT_KIND_ELECTRICAL, 158},
      Expectation{&PollDemand::warning, WorkKind::WORK_KIND_READ_PARAMETER, ObjectKind::OBJECT_KIND_ELECTRICAL, 156},
      Expectation{&PollDemand::local_operation, WorkKind::WORK_KIND_READ_OBJECT,
                  ObjectKind::OBJECT_KIND_LOCAL_OPERATION, 0},
      Expectation{&PollDemand::local_control, WorkKind::WORK_KIND_READ_OBJECT, ObjectKind::OBJECT_KIND_LOCAL_CONTROL,
                  0},
      Expectation{&PollDemand::realized_operation, WorkKind::WORK_KIND_READ_OBJECT,
                  ObjectKind::OBJECT_KIND_REALIZED_OPERATION, 0},
  };

  WorkQueue empty_queue;
  enqueue_periodic_reads({}, empty_queue, nullptr);
  EXPECT_EQ(empty_queue.size(), 0U);

  for (const auto &expected : expectations) {
    PollDemand demand{};
    demand.*(expected.field) = true;
    WorkQueue queue;
    enqueue_periodic_reads(demand, queue, nullptr);
    ASSERT_EQ(queue.size(), 1U);
    WorkItem output;
    ASSERT_TRUE(queue.pop_next(output));
    EXPECT_EQ(output.kind, expected.kind);
    EXPECT_EQ(output.object_kind, expected.object_kind);
    EXPECT_EQ(output.parameter_id, expected.parameter_id);
    EXPECT_FALSE(output.user_initiated);
  }
}

TEST(Alpha3PeriodicReads, CoalescesAgainstQueuedAndActiveReads) {
  PollDemand demand{};
  demand.electrical = true;
  const WorkItem electrical = read_object(ObjectKind::OBJECT_KIND_ELECTRICAL);

  WorkQueue queued;
  ASSERT_EQ(queued.enqueue(electrical, nullptr), EnqueueResult::ENQUEUE_RESULT_ACCEPTED);
  enqueue_periodic_reads(demand, queued, nullptr);
  EXPECT_EQ(queued.size(), 1U);

  WorkQueue active;
  enqueue_periodic_reads(demand, active, &electrical);
  EXPECT_EQ(active.size(), 0U);
}

TEST(Alpha3PeriodicReads, LegacyDemandQueuesOnlyHydraulicAndElectricalOnce) {
  TransportState state;
  PollDemand demand{};
  demand.hydraulic = true;
  demand.electrical = true;

  enqueue_periodic_reads(demand, state.queue, nullptr);
  ASSERT_EQ(state.queue.size(), 2U);
  enqueue_periodic_reads(demand, state.queue, nullptr);
  ASSERT_EQ(state.queue.size(), 2U);

  WorkItem output;
  ASSERT_TRUE(state.queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(read_object(ObjectKind::OBJECT_KIND_HYDRAULIC_MODEL_B)));
  state.transaction.active = true;
  state.transaction.work = output;
  enqueue_periodic_reads(demand, state.queue, &state.transaction.work);
  ASSERT_EQ(state.queue.size(), 1U);
  ASSERT_TRUE(state.queue.pop_next(output));
  EXPECT_TRUE(output.equivalent(read_object(ObjectKind::OBJECT_KIND_ELECTRICAL)));
  EXPECT_FALSE(state.queue.pop_next(output));
}

}  // namespace esphome::alpha3::testing
