#include "alpha3_command.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace esphome::alpha3 {
namespace {

CommandOutcome failure(CommandError error) {
  CommandOutcome outcome;
  outcome.result = CommandResult::COMMAND_RESULT_FAILURE;
  outcome.error = error;
  return outcome;
}

CommandOutcome fail_command(CommandState &state, CommandError error) {
  if (state.active)
    state.stage = WorkStage::WORK_STAGE_COMPLETE;
  return failure(error);
}

CommandOutcome read_object(ObjectKind kind) {
  CommandOutcome outcome;
  outcome.request = CommandRequest::COMMAND_REQUEST_READ_OBJECT;
  outcome.object_kind = kind;
  return outcome;
}

bool supports_control(const DeviceProfile &profile, uint32_t mode) {
  const auto end = profile.control_modes.begin() + profile.control_mode_count;
  return std::find(profile.control_modes.begin(), end, mode) != end;
}

ObjectKind local_object(const WorkItem &work) {
  return work.kind == WorkKind::WORK_KIND_SET_OPERATION_MODE ? ObjectKind::OBJECT_KIND_LOCAL_OPERATION
                                                             : work.object_kind;
}

void complete(CommandState &state, CommandOutcome &outcome, bool matched) {
  state.stage = WorkStage::WORK_STAGE_COMPLETE;
  outcome.result = matched ? CommandResult::COMMAND_RESULT_SUCCESS : CommandResult::COMMAND_RESULT_FAILURE;
  outcome.error = matched ? CommandError::COMMAND_ERROR_NONE : CommandError::COMMAND_ERROR_VERIFICATION_MISMATCH;
}

}  // namespace

CommandOutcome CommandPolicy::start(bool protocol_ready, const ProfileMatch &profile, const SetpointRanges &ranges,
                                    const WorkItem &work, CommandState &state) const {
  if (state.active)
    return failure(CommandError::COMMAND_ERROR_UNEXPECTED_STATE);
  if (!protocol_ready || state.unit_address == GENI_BROADCAST_ADDRESS)
    return failure(CommandError::COMMAND_ERROR_NOT_READY);
  if (profile.profile == nullptr || !profile.exact || !profile.writable)
    return failure(CommandError::COMMAND_ERROR_PROFILE_NOT_WRITABLE);

  Capability capability;
  uint8_t payload_size;
  float requested_raw = 0;
  switch (work.kind) {
    case WorkKind::WORK_KIND_SET_OPERATION_MODE:
      if (work.object_kind != ObjectKind::OBJECT_KIND_OPERATION_CONFIG || work.argument_bits > 3)
        return failure(CommandError::COMMAND_ERROR_UNSUPPORTED_VALUE);
      capability = Capability::CAPABILITY_WRITE_OPERATION;
      payload_size = 4;
      break;
    case WorkKind::WORK_KIND_SET_CONTROL_MODE:
      if (work.object_kind != ObjectKind::OBJECT_KIND_LOCAL_CONTROL ||
          !supports_control(*profile.profile, work.argument_bits))
        return failure(CommandError::COMMAND_ERROR_UNSUPPORTED_VALUE);
      capability = Capability::CAPABILITY_WRITE_CONTROL;
      payload_size = 7;
      break;
    case WorkKind::WORK_KIND_SET_SETPOINT: {
      const SetpointRange *range = ranges.find(work.object_kind);
      if (range == nullptr)
        return failure(CommandError::COMMAND_ERROR_UNSUPPORTED_VALUE);
      const uint8_t control = work.object_kind == ObjectKind::OBJECT_KIND_CS_USER_CONFIG   ? 2
                              : work.object_kind == ObjectKind::OBJECT_KIND_CP_USER_CONFIG ? 0
                                                                                           : 1;
      if (!supports_control(*profile.profile, control))
        return failure(CommandError::COMMAND_ERROR_UNSUPPORTED_VALUE);
      if (!has_read_capability(profile, Capability::CAPABILITY_SETPOINT_LIMITS))
        return failure(CommandError::COMMAND_ERROR_CAPABILITY_MISSING);
      if (!range->valid || !std::isfinite(range->minimum) || !std::isfinite(range->maximum) ||
          range->minimum > range->maximum)
        return failure(CommandError::COMMAND_ERROR_RANGE_UNAVAILABLE);
      std::memcpy(&requested_raw, &work.argument_bits, sizeof(requested_raw));
      if (!std::isfinite(requested_raw) || requested_raw < range->minimum || requested_raw > range->maximum)
        return failure(CommandError::COMMAND_ERROR_OUT_OF_RANGE);
      capability = Capability::CAPABILITY_WRITE_SETPOINT;
      payload_size = 18;
      break;
    }
    default:
      return failure(CommandError::COMMAND_ERROR_UNSUPPORTED_VALUE);
  }
  if (!has_write_capability(profile, capability))
    return failure(CommandError::COMMAND_ERROR_CAPABILITY_MISSING);
  const ObjectSchema *schema = find_schema(*profile.profile, work.object_kind);
  if (schema == nullptr || !schema->readable || !schema->writable || schema->version_count == 0 ||
      schema->versions[0].version != 1 || schema->versions[0].payload_size != payload_size)
    return failure(CommandError::COMMAND_ERROR_CAPABILITY_MISSING);

  const uint8_t unit_address = state.unit_address;
  state = {};
  state.unit_address = unit_address;
  state.active = true;
  state.work = work;
  state.payload.size = payload_size;
  state.requested_raw = requested_raw;
  state.write_address = schema->address;
  state.write_type = schema->object_type;
  return read_object(work.object_kind);
}

CommandOutcome CommandPolicy::accept_object(CommandState &state, ObjectKind received_kind, const uint8_t *payload,
                                            size_t payload_size) const {
  if (!state.active ||
      (state.stage != WorkStage::WORK_STAGE_READ_CURRENT && state.stage != WorkStage::WORK_STAGE_VERIFY_LOCAL &&
       state.stage != WorkStage::WORK_STAGE_VERIFY_REALIZED))
    return fail_command(state, CommandError::COMMAND_ERROR_UNEXPECTED_STATE);
  const ObjectKind expected = state.stage == WorkStage::WORK_STAGE_READ_CURRENT ? state.work.object_kind
                              : state.stage == WorkStage::WORK_STAGE_VERIFY_LOCAL
                                  ? local_object(state.work)
                                  : ObjectKind::OBJECT_KIND_REALIZED_OPERATION;
  if (received_kind != expected)
    return fail_command(state, CommandError::COMMAND_ERROR_UNEXPECTED_OBJECT);
  const size_t expected_size =
      state.stage == WorkStage::WORK_STAGE_READ_CURRENT || state.work.kind == WorkKind::WORK_KIND_SET_SETPOINT
          ? state.payload.size
          : 7;
  if (payload == nullptr || payload_size != expected_size)
    return fail_command(state, CommandError::COMMAND_ERROR_INVALID_PAYLOAD);

  if (state.stage == WorkStage::WORK_STAGE_READ_CURRENT) {
    std::copy_n(payload, payload_size, state.payload.current.begin());
    state.payload.requested = state.payload.current;
    switch (state.work.kind) {
      case WorkKind::WORK_KIND_SET_OPERATION_MODE:
        state.payload.requested[1] = static_cast<uint8_t>(state.work.argument_bits);
        break;
      case WorkKind::WORK_KIND_SET_CONTROL_MODE:
        state.payload.requested[2] = static_cast<uint8_t>(state.work.argument_bits);
        break;
      case WorkKind::WORK_KIND_SET_SETPOINT:
        if (!replace_setpoint(payload, payload_size, state.requested_raw, state.payload.requested))
          return fail_command(state, CommandError::COMMAND_ERROR_INVALID_PAYLOAD);
        break;
      default:
        return fail_command(state, CommandError::COMMAND_ERROR_UNEXPECTED_STATE);
    }
    CommandOutcome outcome;
    if (!build_object_set(state.unit_address, state.write_address, state.write_type, 1, state.payload.requested.data(),
                          state.payload.size, outcome.write_frame))
      return fail_command(state, CommandError::COMMAND_ERROR_INVALID_PAYLOAD);
    state.stage = WorkStage::WORK_STAGE_WRITE_CURRENT;
    outcome.request = CommandRequest::COMMAND_REQUEST_WRITE_FRAME;
    outcome.object_kind = state.work.object_kind;
    return outcome;
  }

  CommandOutcome outcome;
  if (state.work.kind == WorkKind::WORK_KIND_SET_SETPOINT) {
    if (!decode_setpoint(payload, payload_size, outcome.confirmed_setpoint) ||
        !std::isfinite(outcome.confirmed_setpoint))
      return fail_command(state, CommandError::COMMAND_ERROR_INVALID_PAYLOAD);
    outcome.publication = CommandPublication::COMMAND_PUBLICATION_SETPOINT;
    const float tolerance = std::max(0.001F, std::abs(state.requested_raw) * 0.00001F);
    complete(state, outcome, std::abs(outcome.confirmed_setpoint - state.requested_raw) <= tolerance);
    return outcome;
  }

  OperationStatus status;
  if (!decode_operation_status(payload, payload_size, status))
    return fail_command(state, CommandError::COMMAND_ERROR_INVALID_PAYLOAD);
  if (state.stage == WorkStage::WORK_STAGE_VERIFY_LOCAL) {
    outcome = read_object(ObjectKind::OBJECT_KIND_REALIZED_OPERATION);
    const bool operation = state.work.kind == WorkKind::WORK_KIND_SET_OPERATION_MODE;
    outcome.confirmed_enum = operation ? status.operation : status.control;
    outcome.publication =
        operation ? CommandPublication::COMMAND_PUBLICATION_OPERATION : CommandPublication::COMMAND_PUBLICATION_CONTROL;
    state.local_value_matched = outcome.confirmed_enum == state.work.argument_bits;
    state.stage = WorkStage::WORK_STAGE_VERIFY_REALIZED;
  } else {
    outcome.publication = CommandPublication::COMMAND_PUBLICATION_REALIZED_STATUS;
    outcome.realized_status = status;
    complete(state, outcome, state.local_value_matched);
  }
  return outcome;
}

bool CommandPolicy::accept_read_response(CommandState &state, const ObjectSchema &schema, const ParsedFrame &frame,
                                         CommandOutcome &outcome) const {
  ParsedObject object;
  if (frame.operation != GeniOperation::GENI_OPERATION_GET ||
      parse_object_response(frame, schema.object_type, schema.versions.data(), schema.version_count, object) !=
          ParseResult::PARSE_RESULT_OK) {
    // A delayed SET ACK or unrelated object must not retire the verification GET.
    return false;
  }
  outcome = this->accept_object(state, schema.kind, object.payload, object.payload_size);
  return true;
}

CommandOutcome CommandPolicy::accept_write_ack(CommandState &state) const {
  return this->handle_write_uncertain(state);
}

CommandOutcome CommandPolicy::handle_write_uncertain(CommandState &state) const {
  if (!state.active || state.stage != WorkStage::WORK_STAGE_WRITE_CURRENT)
    return fail_command(state, CommandError::COMMAND_ERROR_UNEXPECTED_STATE);
  state.stage = WorkStage::WORK_STAGE_VERIFY_LOCAL;
  return read_object(local_object(state.work));
}

void CommandPolicy::reset(CommandState &state) const { state = {}; }

}  // namespace esphome::alpha3
