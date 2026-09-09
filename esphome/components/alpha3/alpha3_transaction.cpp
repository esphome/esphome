#include "alpha3_transaction.h"

namespace esphome::alpha3 {
namespace {

constexpr uint8_t ALARM_PARAMETER_ID = 158;
constexpr uint8_t WARNING_PARAMETER_ID = 156;

WorkItem object_read_(ObjectKind object_kind) { return {WorkKind::WORK_KIND_READ_OBJECT, object_kind, 0, 0, false}; }

WorkItem parameter_read_(uint8_t parameter_id) {
  return {WorkKind::WORK_KIND_READ_PARAMETER, ObjectKind::OBJECT_KIND_ELECTRICAL, parameter_id, 0, false};
}

}  // namespace

bool WorkItem::equivalent(const WorkItem &other) const {
  return this->kind == other.kind && this->object_kind == other.object_kind &&
         this->parameter_id == other.parameter_id && this->argument_bits == other.argument_bits &&
         this->user_initiated == other.user_initiated;
}

EnqueueResult WorkQueue::enqueue(const WorkItem &item, const WorkItem *active) {
  if ((active != nullptr && item.equivalent(*active)) || this->contains(item))
    return EnqueueResult::ENQUEUE_RESULT_COALESCED;

  if (this->size_ == WORK_QUEUE_CAPACITY) {
    if (!item.user_initiated)
      return EnqueueResult::ENQUEUE_RESULT_REJECTED_FULL;

    uint8_t newest_poll = this->size_;
    for (uint8_t index = this->size_; index > 0; index--) {
      if (!this->items_[index - 1].user_initiated) {
        newest_poll = index - 1;
        break;
      }
    }
    if (newest_poll == this->size_)
      return EnqueueResult::ENQUEUE_RESULT_REJECTED_FULL;

    for (uint8_t index = newest_poll; index + 1 < this->size_; index++)
      this->items_[index] = this->items_[index + 1];
    this->size_--;
  }

  this->items_[this->size_++] = item;
  return EnqueueResult::ENQUEUE_RESULT_ACCEPTED;
}

bool WorkQueue::pop_next(WorkItem &output) {
  if (this->size_ == 0)
    return false;

  uint8_t index = 0;
  for (; index < this->size_; index++) {
    if (this->items_[index].user_initiated)
      break;
  }
  if (index == this->size_)
    index = 0;

  output = this->items_[index];
  for (; index + 1 < this->size_; index++)
    this->items_[index] = this->items_[index + 1];
  this->items_[--this->size_] = {};
  return true;
}

bool WorkQueue::contains(const WorkItem &item) const {
  for (uint8_t index = 0; index < this->size_; index++) {
    if (this->items_[index].equivalent(item))
      return true;
  }
  return false;
}

void WorkQueue::clear() {
  this->items_ = {};
  this->size_ = 0;
}

size_t WorkQueue::size() const { return this->size_; }

void ReadinessTracker::reset() {
  this->characteristic_handle_ = 0;
  this->cccd_handle_ = 0;
  this->service_found_ = false;
  this->authenticated_ = false;
  this->registration_started_ = false;
  this->registration_succeeded_ = false;
  this->cccd_succeeded_ = false;
}

void ReadinessTracker::mark_service_found(uint16_t characteristic_handle, uint16_t cccd_handle) {
  this->characteristic_handle_ = characteristic_handle;
  this->cccd_handle_ = cccd_handle;
  this->service_found_ = true;
}

void ReadinessTracker::mark_authenticated() { this->authenticated_ = true; }

void ReadinessTracker::mark_registration_started() {
  if (this->can_register())
    this->registration_started_ = true;
}

void ReadinessTracker::mark_registration_succeeded() {
  if (this->registration_started_)
    this->registration_succeeded_ = true;
}

void ReadinessTracker::mark_cccd_succeeded() {
  if (this->registration_succeeded_)
    this->cccd_succeeded_ = true;
}

bool ReadinessTracker::can_register() const {
  return this->service_found_ && this->authenticated_ && !this->registration_started_;
}

bool ReadinessTracker::ready() const {
  return this->service_found_ && this->authenticated_ && this->registration_started_ && this->registration_succeeded_ &&
         this->cccd_succeeded_;
}

uint16_t ReadinessTracker::characteristic_handle() const { return this->characteristic_handle_; }

uint16_t ReadinessTracker::cccd_handle() const { return this->cccd_handle_; }

const SetpointRange *SetpointRanges::find(ObjectKind user_config_kind) const {
  switch (user_config_kind) {
    case ObjectKind::OBJECT_KIND_CS_USER_CONFIG:
      return &this->constant_speed;
    case ObjectKind::OBJECT_KIND_CP_USER_CONFIG:
      return &this->constant_pressure;
    case ObjectKind::OBJECT_KIND_PP_USER_CONFIG:
      return &this->proportional_pressure;
    default:
      return nullptr;
  }
}

SetpointRange *SetpointRanges::find(ObjectKind user_config_kind) {
  return const_cast<SetpointRange *>(static_cast<const SetpointRanges *>(this)->find(user_config_kind));
}

void SetpointRanges::clear() {
  this->constant_speed = {};
  this->constant_pressure = {};
  this->proportional_pressure = {};
}

bool TransportState::start_discovery() {
  if (!this->readiness.ready() || this->discovery_started_ || this->unit_address != GENI_BROADCAST_ADDRESS)
    return false;
  const WorkItem discovery{WorkKind::WORK_KIND_DISCOVER_ADDRESS, ObjectKind{}, 0, 0, false};
  if (this->queue.enqueue(discovery, nullptr) != EnqueueResult::ENQUEUE_RESULT_ACCEPTED)
    return false;
  this->discovery_started_ = true;
  return true;
}

bool TransportState::accept_discovered_address(uint8_t address) {
  if (!this->transaction.active || this->transaction.work.kind != WorkKind::WORK_KIND_DISCOVER_ADDRESS ||
      this->unit_address != GENI_BROADCAST_ADDRESS || address == GENI_BROADCAST_ADDRESS ||
      this->transaction.frame.size == 0 || this->transaction.sent_offset != this->transaction.frame.size)
    return false;
  this->unit_address = address;
  this->transaction = {};
  this->assembler.reset();
  for (const uint8_t parameter_id : {UNIT_FAMILY_PARAMETER_ID, UNIT_TYPE_PARAMETER_ID, UNIT_VERSION_PARAMETER_ID})
    this->queue.enqueue(parameter_read_(parameter_id), nullptr);
  return true;
}

TimeoutDecision TransportState::handle_timeout() {
  if (!this->transaction.active)
    return TimeoutDecision::TIMEOUT_DECISION_FAIL;
  if (this->transaction.work.kind == WorkKind::WORK_KIND_DISCOVER_ADDRESS) {
    if (this->accept_discovered_address(GENI_LEGACY_UNIT_ADDRESS))
      return TimeoutDecision::TIMEOUT_DECISION_DISCOVERY_FALLBACK;
    return TimeoutDecision::TIMEOUT_DECISION_FAIL;
  }
  if (this->transaction.is_write)
    return TimeoutDecision::TIMEOUT_DECISION_VERIFY_WRITE;
  if (this->transaction.retries_remaining > 0) {
    this->transaction.retries_remaining--;
    return TimeoutDecision::TIMEOUT_DECISION_RETRY_READ;
  }
  return TimeoutDecision::TIMEOUT_DECISION_FAIL;
}

void TransportState::reset_on_disconnect() {
  this->readiness.reset();
  this->queue.clear();
  this->transaction = {};
  this->assembler.reset();
  this->identity = {};
  this->profile = {};
  this->unit_address = GENI_BROADCAST_ADDRESS;
  this->discovery_started_ = false;
}

void enqueue_periodic_reads(const PollDemand &demand, WorkQueue &queue, const WorkItem *active) {
  if (demand.hydraulic)
    queue.enqueue(object_read_(ObjectKind::OBJECT_KIND_HYDRAULIC_MODEL_B), active);
  if (demand.electrical)
    queue.enqueue(object_read_(ObjectKind::OBJECT_KIND_ELECTRICAL), active);
  if (demand.history)
    queue.enqueue(object_read_(ObjectKind::OBJECT_KIND_HISTORY), active);
  if (demand.energy)
    queue.enqueue(object_read_(ObjectKind::OBJECT_KIND_ENERGY), active);
  if (demand.alarm)
    queue.enqueue(parameter_read_(ALARM_PARAMETER_ID), active);
  if (demand.warning)
    queue.enqueue(parameter_read_(WARNING_PARAMETER_ID), active);
  if (demand.local_operation)
    queue.enqueue(object_read_(ObjectKind::OBJECT_KIND_LOCAL_OPERATION), active);
  if (demand.local_control)
    queue.enqueue(object_read_(ObjectKind::OBJECT_KIND_LOCAL_CONTROL), active);
  if (demand.realized_operation)
    queue.enqueue(object_read_(ObjectKind::OBJECT_KIND_REALIZED_OPERATION), active);
}

}  // namespace esphome::alpha3
