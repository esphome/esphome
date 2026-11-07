#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/alpha3/alpha3_profile.h"
#include "esphome/components/alpha3/alpha3_protocol.h"

namespace esphome::alpha3 {

constexpr size_t WORK_QUEUE_CAPACITY = 16;
constexpr uint8_t UNIT_FAMILY_PARAMETER_ID = 148;
constexpr uint8_t UNIT_TYPE_PARAMETER_ID = 149;
constexpr uint8_t UNIT_VERSION_PARAMETER_ID = 150;

enum class WorkKind : uint8_t {
  WORK_KIND_READ_PARAMETER,
  WORK_KIND_READ_OBJECT,
  WORK_KIND_SET_OPERATION_MODE,
  WORK_KIND_SET_CONTROL_MODE,
  WORK_KIND_SET_SETPOINT,
  WORK_KIND_DISCOVER_ADDRESS,
};

enum class WorkStage : uint8_t {
  WORK_STAGE_READ_CURRENT,
  WORK_STAGE_WRITE_CURRENT,
  WORK_STAGE_VERIFY_LOCAL,
  WORK_STAGE_VERIFY_REALIZED,
  WORK_STAGE_COMPLETE,
};

struct WorkItem {
  WorkKind kind;
  ObjectKind object_kind;
  uint8_t parameter_id;
  uint32_t argument_bits;
  bool user_initiated;

  bool equivalent(const WorkItem &other) const;
};

enum class EnqueueResult : uint8_t {
  ENQUEUE_RESULT_ACCEPTED,
  ENQUEUE_RESULT_COALESCED,
  ENQUEUE_RESULT_REJECTED_FULL,
};

class WorkQueue {
 public:
  EnqueueResult enqueue(const WorkItem &item, const WorkItem *active);
  bool pop_next(WorkItem &output);
  bool contains(const WorkItem &item) const;
  void clear();
  size_t size() const;

 protected:
  std::array<WorkItem, WORK_QUEUE_CAPACITY> items_{};
  uint8_t size_{0};
};

class ReadinessTracker {
 public:
  void reset();
  void mark_service_found(uint16_t characteristic_handle, uint16_t cccd_handle);
  void mark_authenticated();
  void mark_registration_started();
  void mark_registration_succeeded();
  void mark_cccd_succeeded();
  bool can_register() const;
  bool ready() const;
  uint16_t characteristic_handle() const;
  uint16_t cccd_handle() const;

 protected:
  uint16_t characteristic_handle_{0};
  uint16_t cccd_handle_{0};
  bool service_found_{false};
  bool authenticated_{false};
  bool registration_started_{false};
  bool registration_succeeded_{false};
  bool cccd_succeeded_{false};
};

struct ActiveTransaction {
  bool active{false};
  bool is_write{false};
  WorkItem work{};
  ObjectKind expected_object{};
  uint8_t expected_parameter_id{0};
  EncodedFrame frame{};
  uint8_t sent_offset{0};
  uint8_t retries_remaining{0};
};

struct SetpointRange {
  bool valid{false};
  float minimum{0.0F};
  float maximum{0.0F};
};

struct SetpointRanges {
  SetpointRange constant_speed{};
  SetpointRange constant_pressure{};
  SetpointRange proportional_pressure{};

  const SetpointRange *find(ObjectKind user_config_kind) const;
  SetpointRange *find(ObjectKind user_config_kind);
  void clear();
};

enum class TimeoutDecision : uint8_t {
  TIMEOUT_DECISION_RETRY_READ,
  TIMEOUT_DECISION_VERIFY_WRITE,
  TIMEOUT_DECISION_FAIL,
  TIMEOUT_DECISION_DISCOVERY_FALLBACK,
};

struct TransportState {
  ReadinessTracker readiness{};
  WorkQueue queue{};
  ActiveTransaction transaction{};
  FrameAssembler assembler{};
  DeviceIdentity identity{};
  ProfileMatch profile{};
  // Broadcast is never a valid response source, so it denotes an unresolved unit.
  uint8_t unit_address{GENI_BROADCAST_ADDRESS};

  bool transport_ready() const;
  bool control_ready() const;
  bool start_discovery();
  bool accept_discovered_address(uint8_t address);
  TimeoutDecision handle_timeout();
  void reset_on_disconnect();

 protected:
  bool discovery_started_{false};
};

struct PollDemand {
  bool hydraulic;
  bool electrical;
  bool history;
  bool energy;
  bool alarm;
  bool warning;
  bool local_operation;
  bool local_control;
  bool realized_operation;
};

void enqueue_periodic_reads(const PollDemand &demand, WorkQueue &queue, const WorkItem *active);

}  // namespace esphome::alpha3
