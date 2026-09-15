#pragma once

#include "alpha3_payload.h"
#include "alpha3_transaction.h"

namespace esphome::alpha3 {

struct CommandPayload {
  std::array<uint8_t, 18> current{};
  std::array<uint8_t, 18> requested{};
  uint8_t size{0};
};

enum class CommandRequest : uint8_t {
  COMMAND_REQUEST_NONE,
  COMMAND_REQUEST_READ_OBJECT,
  COMMAND_REQUEST_WRITE_FRAME,
};

enum class CommandPublication : uint8_t {
  COMMAND_PUBLICATION_NONE,
  COMMAND_PUBLICATION_OPERATION,
  COMMAND_PUBLICATION_CONTROL,
  COMMAND_PUBLICATION_SETPOINT,
  COMMAND_PUBLICATION_REALIZED_STATUS,
};

enum class CommandResult : uint8_t {
  COMMAND_RESULT_IN_PROGRESS,
  COMMAND_RESULT_SUCCESS,
  COMMAND_RESULT_FAILURE,
};

enum class CommandError : uint8_t {
  COMMAND_ERROR_NONE,
  COMMAND_ERROR_NOT_READY,
  COMMAND_ERROR_PROFILE_NOT_WRITABLE,
  COMMAND_ERROR_CAPABILITY_MISSING,
  COMMAND_ERROR_UNSUPPORTED_VALUE,
  COMMAND_ERROR_RANGE_UNAVAILABLE,
  COMMAND_ERROR_OUT_OF_RANGE,
  COMMAND_ERROR_UNEXPECTED_STATE,
  COMMAND_ERROR_UNEXPECTED_OBJECT,
  COMMAND_ERROR_INVALID_PAYLOAD,
  COMMAND_ERROR_VERIFICATION_MISMATCH,
};

struct CommandState {
  // Set from this connection's discovery before start; never default to the legacy unit.
  uint8_t unit_address{GENI_BROADCAST_ADDRESS};
  bool active{false};
  WorkItem work{};
  WorkStage stage{WorkStage::WORK_STAGE_READ_CURRENT};
  CommandPayload payload{};
  float requested_raw{0.0F};
  bool local_value_matched{false};
  // Copy the validated write schema; no response-buffer or profile lifetime dependency.
  ObjectAddress write_address{};
  uint16_t write_type{0};
};

struct CommandOutcome {
  CommandRequest request{CommandRequest::COMMAND_REQUEST_NONE};
  ObjectKind object_kind{ObjectKind::OBJECT_KIND_OPERATION_CONFIG};
  EncodedFrame write_frame{};
  CommandPublication publication{CommandPublication::COMMAND_PUBLICATION_NONE};
  uint8_t confirmed_enum{0};
  float confirmed_setpoint{0.0F};
  OperationStatus realized_status{};
  CommandResult result{CommandResult::COMMAND_RESULT_IN_PROGRESS};
  CommandError error{CommandError::COMMAND_ERROR_NONE};
};

class CommandPolicy {
 public:
  CommandOutcome start(bool protocol_ready, const ProfileMatch &profile, const SetpointRanges &ranges,
                       const WorkItem &work, CommandState &state) const;
  CommandOutcome accept_object(CommandState &state, ObjectKind received_kind, const uint8_t *payload,
                               size_t payload_size) const;
  // False discards a mismatched frame without changing state/outcome or the caller's read timeout/retry budget.
  bool accept_read_response(CommandState &state, const ObjectSchema &schema, const ParsedFrame &frame,
                            CommandOutcome &outcome) const;
  CommandOutcome accept_write_ack(CommandState &state) const;
  CommandOutcome handle_write_uncertain(CommandState &state) const;
  void reset(CommandState &state) const;
};

}  // namespace esphome::alpha3
