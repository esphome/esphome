#include "alpha3.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cmath>
#include <cstring>

#include "alpha3_payload.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::alpha3 {
namespace {

static const char *const TAG = "alpha3";

constexpr uint16_t CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID = 0x2902;
constexpr uint8_t ALARM_PARAMETER_ID = 158;
constexpr uint8_t WARNING_PARAMETER_ID = 156;

#ifdef USE_NUMBER
struct SetpointObjects {
  Alpha3NumberType type;
  ObjectKind factory_limits;
  ObjectKind user_config;
  uint8_t control_mode;
};

constexpr std::array<SetpointObjects, 3> SETPOINT_OBJECTS{{
    {Alpha3NumberType::ALPHA3_NUMBER_TYPE_CONSTANT_SPEED, ObjectKind::OBJECT_KIND_CS_FACTORY_LIMITS,
     ObjectKind::OBJECT_KIND_CS_USER_CONFIG, 2},
    {Alpha3NumberType::ALPHA3_NUMBER_TYPE_CONSTANT_PRESSURE, ObjectKind::OBJECT_KIND_CP_FACTORY_LIMITS,
     ObjectKind::OBJECT_KIND_CP_USER_CONFIG, 0},
    {Alpha3NumberType::ALPHA3_NUMBER_TYPE_PROPORTIONAL_PRESSURE, ObjectKind::OBJECT_KIND_PP_FACTORY_LIMITS,
     ObjectKind::OBJECT_KIND_PP_USER_CONFIG, 1},
}};
#endif

}  // namespace

void Alpha3::setup() {
  // BLEClient owns dispatch of BLEClientNode::loop(). Keep Application from also
  // dispatching this multiply-inherited component in the same application cycle.
  this->disable_loop();
}

void Alpha3::loop() {
  if (!this->is_ready())
    return;
  if (!this->transport_.transaction.active)
    this->start_next_work_();
  this->send_next_fragment_();
}

void Alpha3::update() {
  if (!this->is_ready() || this->transport_.profile.profile == nullptr)
    return;

  PollDemand demand{};
  demand.hydraulic = (this->flow_sensor_ != nullptr || this->head_sensor_ != nullptr) &&
                     has_read_capability(this->transport_.profile, Capability::CAPABILITY_HYDRAULIC);
  demand.electrical = (this->power_sensor_ != nullptr || this->current_sensor_ != nullptr ||
                       this->speed_sensor_ != nullptr || this->voltage_sensor_ != nullptr) &&
                      has_read_capability(this->transport_.profile, Capability::CAPABILITY_ELECTRICAL);
  demand.history = (this->operating_hours_sensor_ != nullptr || this->starts_sensor_ != nullptr) &&
                   has_read_capability(this->transport_.profile, Capability::CAPABILITY_HISTORY);
  demand.energy =
      this->energy_sensor_ != nullptr && has_read_capability(this->transport_.profile, Capability::CAPABILITY_ENERGY);
  demand.alarm = this->alarm_code_sensor_ != nullptr &&
                 has_read_capability(this->transport_.profile, Capability::CAPABILITY_STATUS);
  demand.warning = this->warning_code_sensor_ != nullptr &&
                   has_read_capability(this->transport_.profile, Capability::CAPABILITY_STATUS);
#ifdef USE_SELECT
  demand.local_operation = this->operation_mode_select_ != nullptr &&
                           has_read_capability(this->transport_.profile, Capability::CAPABILITY_STATUS);
  demand.local_control = this->control_mode_select_ != nullptr &&
                         has_read_capability(this->transport_.profile, Capability::CAPABILITY_STATUS);
#endif
#ifdef USE_TEXT_SENSOR
  demand.realized_operation =
      (this->realized_operation_text_sensor_ != nullptr || this->active_control_source_text_sensor_ != nullptr) &&
      has_read_capability(this->transport_.profile, Capability::CAPABILITY_STATUS);
#endif
  const WorkItem *active = this->transport_.transaction.active ? &this->transport_.transaction.work : nullptr;
  enqueue_periodic_reads(demand, this->transport_.queue, active);
#ifdef USE_NUMBER
  if (has_read_capability(this->transport_.profile, Capability::CAPABILITY_SETPOINT_LIMITS)) {
    for (const auto &objects : SETPOINT_OBJECTS) {
      if (this->get_setpoint_number_(objects.user_config) == nullptr)
        continue;
      for (const ObjectKind kind : {objects.factory_limits, objects.user_config})
        this->transport_.queue.enqueue({WorkKind::WORK_KIND_READ_OBJECT, kind, 0, 0, false}, active);
    }
  }
#endif
}

void Alpha3::dump_config() {
  ESP_LOGCONFIG(TAG, "ALPHA3:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Flow", this->flow_sensor_);
  LOG_SENSOR("  ", "Head", this->head_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Speed", this->speed_sensor_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Operating Hours", this->operating_hours_sensor_);
  LOG_SENSOR("  ", "Energy", this->energy_sensor_);
  LOG_SENSOR("  ", "Starts", this->starts_sensor_);
  LOG_SENSOR("  ", "Alarm Code", this->alarm_code_sensor_);
  LOG_SENSOR("  ", "Warning Code", this->warning_code_sensor_);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Ready", this->ready_binary_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Realized Operation", this->realized_operation_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Active Control Source", this->active_control_source_text_sensor_);
#endif
#ifdef USE_SELECT
  LOG_SELECT("  ", "Operating Mode", this->operation_mode_select_);
  LOG_SELECT("  ", "Control Mode", this->control_mode_select_);
#endif
#ifdef USE_NUMBER
  LOG_NUMBER("  ", "Constant Speed Setpoint", this->constant_speed_number_);
  LOG_NUMBER("  ", "Constant Pressure Setpoint", this->constant_pressure_number_);
  LOG_NUMBER("  ", "Proportional Pressure Setpoint", this->proportional_pressure_number_);
#endif
}

bool Alpha3::is_ready() const { return this->ready_; }

#if defined(USE_SELECT) || defined(USE_NUMBER)
bool Alpha3::validate_write_(Capability capability, ObjectKind object_kind) {
  if (!this->is_ready() || this->transport_.unit_address == GENI_BROADCAST_ADDRESS) {
    ESP_LOGW(TAG, "[%s] rejected command: protocol is not ready", this->parent()->address_str());
    return false;
  }
  const ProfileMatch &match = this->transport_.profile;
  if (match.profile == nullptr || !match.exact || !match.writable) {
    ESP_LOGW(TAG, "[%s] rejected command: exact writable device profile required", this->parent()->address_str());
    return false;
  }
  if (!has_write_capability(match, capability)) {
    ESP_LOGW(TAG, "[%s] rejected command: profile lacks write capability 0x%08X", this->parent()->address_str(),
             static_cast<unsigned>(capability));
    return false;
  }
  const ObjectSchema *schema = find_schema(*match.profile, object_kind);
  if (schema == nullptr || !schema->readable || !schema->writable) {
    ESP_LOGW(TAG, "[%s] rejected command: object %u is not readable and writable", this->parent()->address_str(),
             static_cast<unsigned>(object_kind));
    return false;
  }
  return true;
}

bool Alpha3::enqueue_command_(const WorkItem &work) {
  const WorkItem *active = this->command_state_.active           ? &this->command_state_.work
                           : this->transport_.transaction.active ? &this->transport_.transaction.work
                                                                 : nullptr;
  if (this->transport_.queue.enqueue(work, active) == EnqueueResult::ENQUEUE_RESULT_REJECTED_FULL) {
    ESP_LOGW(TAG, "[%s] rejected command: work queue is full", this->parent()->address_str());
    return false;
  }
  return true;
}
#endif

#ifdef USE_SELECT
bool Alpha3::request_operation_mode(uint8_t mode) {
  if (!this->validate_write_(Capability::CAPABILITY_WRITE_OPERATION, ObjectKind::OBJECT_KIND_OPERATION_CONFIG))
    return false;
  if (mode > 3) {
    ESP_LOGW(TAG, "[%s] rejected operation mode %u: unsupported mode", this->parent()->address_str(), mode);
    return false;
  }
  return this->enqueue_command_(
      {WorkKind::WORK_KIND_SET_OPERATION_MODE, ObjectKind::OBJECT_KIND_OPERATION_CONFIG, 0, mode, true});
}

bool Alpha3::request_control_mode(uint8_t mode) {
  if (!this->validate_write_(Capability::CAPABILITY_WRITE_CONTROL, ObjectKind::OBJECT_KIND_LOCAL_CONTROL))
    return false;
  const DeviceProfile &profile = *this->transport_.profile.profile;
  const auto modes_end = profile.control_modes.begin() + profile.control_mode_count;
  if (std::find(profile.control_modes.begin(), modes_end, mode) == modes_end) {
    ESP_LOGW(TAG, "[%s] rejected control mode %u: unsupported mode", this->parent()->address_str(), mode);
    return false;
  }
  return this->enqueue_command_(
      {WorkKind::WORK_KIND_SET_CONTROL_MODE, ObjectKind::OBJECT_KIND_LOCAL_CONTROL, 0, mode, true});
}
#endif

#ifdef USE_NUMBER
void Alpha3::set_setpoint_number(Alpha3NumberType type, number::Number *entity) {
  switch (type) {
    case Alpha3NumberType::ALPHA3_NUMBER_TYPE_CONSTANT_SPEED:
      this->constant_speed_number_ = entity;
      break;
    case Alpha3NumberType::ALPHA3_NUMBER_TYPE_CONSTANT_PRESSURE:
      this->constant_pressure_number_ = entity;
      break;
    case Alpha3NumberType::ALPHA3_NUMBER_TYPE_PROPORTIONAL_PRESSURE:
      this->proportional_pressure_number_ = entity;
      break;
  }
}

number::Number *Alpha3::get_setpoint_number_(ObjectKind kind) const {
  switch (kind) {
    case ObjectKind::OBJECT_KIND_CS_USER_CONFIG:
      return this->constant_speed_number_;
    case ObjectKind::OBJECT_KIND_CP_USER_CONFIG:
      return this->constant_pressure_number_;
    case ObjectKind::OBJECT_KIND_PP_USER_CONFIG:
      return this->proportional_pressure_number_;
    default:
      return nullptr;
  }
}

bool Alpha3::request_setpoint(Alpha3NumberType type, float displayed_value) {
  const SetpointObjects *objects = nullptr;
  for (const auto &candidate : SETPOINT_OBJECTS) {
    if (candidate.type == type) {
      objects = &candidate;
      break;
    }
  }
  if (objects == nullptr) {
    ESP_LOGW(TAG, "[%s] rejected setpoint: unsupported number type %u", this->parent()->address_str(),
             static_cast<unsigned>(type));
    return false;
  }
  if (!this->validate_write_(Capability::CAPABILITY_WRITE_SETPOINT, objects->user_config))
    return false;
  const DeviceProfile &profile = *this->transport_.profile.profile;
  const auto modes_end = profile.control_modes.begin() + profile.control_mode_count;
  if (std::find(profile.control_modes.begin(), modes_end, objects->control_mode) == modes_end) {
    ESP_LOGW(TAG, "[%s] rejected setpoint: control mode %u is unsupported", this->parent()->address_str(),
             objects->control_mode);
    return false;
  }
  if (!has_read_capability(this->transport_.profile, Capability::CAPABILITY_SETPOINT_LIMITS)) {
    ESP_LOGW(TAG, "[%s] rejected setpoint: profile lacks factory-limit read capability", this->parent()->address_str());
    return false;
  }
  const SetpointRange &range = *this->setpoint_ranges_.find(objects->user_config);
  if (!range.valid) {
    ESP_LOGW(TAG, "[%s] rejected setpoint: factory limits for object %u have not been read",
             this->parent()->address_str(), static_cast<unsigned>(objects->user_config));
    return false;
  }
  const float raw_value = type == Alpha3NumberType::ALPHA3_NUMBER_TYPE_CONSTANT_SPEED
                              ? displayed_value
                              : displayed_value * profile.pascals_per_meter;
  if (!std::isfinite(displayed_value) || !std::isfinite(raw_value)) {
    ESP_LOGW(TAG, "[%s] rejected setpoint: value must be finite", this->parent()->address_str());
    return false;
  }
  if (raw_value < range.minimum || raw_value > range.maximum) {
    ESP_LOGW(TAG, "[%s] rejected setpoint: raw value %g outside factory limits [%g, %g]", this->parent()->address_str(),
             raw_value, range.minimum, range.maximum);
    return false;
  }
  uint32_t argument_bits;
  std::memcpy(&argument_bits, &raw_value, sizeof(argument_bits));
  return this->enqueue_command_({WorkKind::WORK_KIND_SET_SETPOINT, objects->user_config, 0, argument_bits, true});
}
#endif

void Alpha3::try_subscribe_() {
  if (this->registration_requested_ || !this->transport_.readiness.can_register())
    return;

  this->registration_requested_ = true;
  const esp_err_t status = this->parent()->register_for_notify(this->transport_.readiness.characteristic_handle());
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "[%s] notification registration request failed, status=%d", this->parent()->address_str(), status);
    this->registration_requested_ = false;
    return;
  }
  this->transport_.readiness.mark_registration_started();
  ESP_LOGD(TAG, "[%s] notification registration requested", this->parent()->address_str());
}

void Alpha3::set_ready_(bool ready) {
  const bool new_ready = ready && this->transport_.readiness.ready();
  if (new_ready == this->ready_)
    return;
  this->ready_ = new_ready;
#ifdef USE_BINARY_SENSOR
  if (this->ready_binary_sensor_ != nullptr)
    this->ready_binary_sensor_->publish_state(this->ready_);
#endif
  ESP_LOGI(TAG, "[%s] protocol %s", this->parent()->address_str(), this->ready_ ? "ready" : "not ready");
}

void Alpha3::reset_connection_state_() {
  this->cancel_timeout("alpha3_response");
  this->set_ready_(false);
  this->transport_.reset_on_disconnect();
  this->command_policy_.reset(this->command_state_);
  this->setpoint_ranges_.clear();
  this->registration_requested_ = false;
  this->geni_handle_ = 0;
}

void Alpha3::enqueue_initial_reads_() { this->update(); }

void Alpha3::start_next_work_() {
  if (this->command_state_.active || this->transport_.transaction.active)
    return;
  WorkItem work;
  if (!this->transport_.queue.pop_next(work))
    return;
  if (work.kind != WorkKind::WORK_KIND_DISCOVER_ADDRESS && this->transport_.unit_address == GENI_BROADCAST_ADDRESS) {
    ESP_LOGW(TAG, "[%s] cannot start GENI request before unit address discovery", this->parent()->address_str());
    return;
  }

  ActiveTransaction transaction{};
  transaction.active = true;
  transaction.work = work;
  transaction.retries_remaining = 1;

  bool built = false;
  switch (work.kind) {
    case WorkKind::WORK_KIND_DISCOVER_ADDRESS:
#ifdef USE_NUMBER
      this->setpoint_ranges_.clear();
#endif
      transaction.retries_remaining = 0;
      built = build_discovery_get(transaction.frame);
      break;
    case WorkKind::WORK_KIND_READ_PARAMETER:
      if (work.parameter_id == UNIT_FAMILY_PARAMETER_ID) {
        this->transport_.identity = {};
        this->transport_.profile = {};
#ifdef USE_NUMBER
        this->setpoint_ranges_.clear();
#endif
      }
      transaction.expected_parameter_id = work.parameter_id;
      built = build_parameter_get(this->transport_.unit_address, work.parameter_id, transaction.frame);
      break;
    case WorkKind::WORK_KIND_READ_OBJECT: {
      if (!can_read_object(this->transport_.profile, work.object_kind)) {
        ESP_LOGW(TAG, "[%s] cannot read object without profile permission", this->parent()->address_str());
        break;
      }
      const ObjectSchema *schema = find_schema(*this->transport_.profile.profile, work.object_kind);
      if (schema == nullptr || !schema->readable) {
        ESP_LOGW(TAG, "[%s] object is not readable for the selected profile", this->parent()->address_str());
        break;
      }
      transaction.expected_object = work.object_kind;
      built = build_object_get(this->transport_.unit_address, schema->address, transaction.frame);
      break;
    }
    case WorkKind::WORK_KIND_SET_OPERATION_MODE:
    case WorkKind::WORK_KIND_SET_CONTROL_MODE:
    case WorkKind::WORK_KIND_SET_SETPOINT:
      this->command_state_.unit_address = this->transport_.unit_address;
      this->execute_command_outcome_(this->command_policy_.start(this->is_ready(), this->transport_.profile,
                                                                 this->setpoint_ranges_, work, this->command_state_));
      return;
  }

  if (!built) {
    ESP_LOGW(TAG, "[%s] failed to build GENI request", this->parent()->address_str());
    return;
  }
  this->transport_.assembler.reset();
  this->transport_.transaction = transaction;
}

void Alpha3::execute_command_outcome_(const CommandOutcome &outcome) {
  this->cancel_timeout("alpha3_response");
  this->transport_.transaction = {};
  this->transport_.assembler.reset();

  // Install the next transaction before publication, which can invoke user automations.
  if (outcome.request != CommandRequest::COMMAND_REQUEST_NONE) {
    ActiveTransaction &tx = this->transport_.transaction;
    tx.active = true;
    tx.work = this->command_state_.work;
    tx.expected_object = outcome.object_kind;
    if (!this->is_ready() || this->transport_.unit_address == GENI_BROADCAST_ADDRESS) {
      this->fail_active_transaction_("connection lost during command");
      return;
    }
    if (outcome.request == CommandRequest::COMMAND_REQUEST_WRITE_FRAME) {
      tx.frame = outcome.write_frame;
      tx.is_write = true;
      tx.retries_remaining = 0;
    } else {
      const ObjectSchema *schema = this->transport_.profile.profile == nullptr
                                       ? nullptr
                                       : find_schema(*this->transport_.profile.profile, outcome.object_kind);
      if (schema == nullptr || !schema->readable ||
          !build_object_get(this->transport_.unit_address, schema->address, tx.frame)) {
        this->fail_active_transaction_("cannot build command read");
        return;
      }
      tx.retries_remaining = 1;
    }
  }

  switch (outcome.publication) {
    case CommandPublication::COMMAND_PUBLICATION_OPERATION:
#ifdef USE_SELECT
      if (outcome.confirmed_enum <= 3) {
        if (this->operation_mode_select_ != nullptr)
          this->operation_mode_select_->publish_state(static_cast<size_t>(outcome.confirmed_enum));
      } else {
        ESP_LOGW(TAG, "[%s] unknown confirmed operation %u", this->parent()->address_str(), outcome.confirmed_enum);
      }
#endif
      break;
    case CommandPublication::COMMAND_PUBLICATION_CONTROL:
#ifdef USE_SELECT
    {
      const auto mode = std::find(CONTROL_MODE_VALUES.begin(), CONTROL_MODE_VALUES.end(), outcome.confirmed_enum);
      if (mode != CONTROL_MODE_VALUES.end()) {
        if (this->control_mode_select_ != nullptr)
          this->control_mode_select_->publish_state(static_cast<size_t>(mode - CONTROL_MODE_VALUES.begin()));
      } else {
        ESP_LOGW(TAG, "[%s] unknown confirmed control %u", this->parent()->address_str(), outcome.confirmed_enum);
      }
    }
#endif
    break;
    case CommandPublication::COMMAND_PUBLICATION_SETPOINT:
#ifdef USE_NUMBER
      if (auto *entity = this->get_setpoint_number_(this->command_state_.work.object_kind); entity != nullptr) {
        const float displayed = this->command_state_.work.object_kind == ObjectKind::OBJECT_KIND_CS_USER_CONFIG
                                    ? outcome.confirmed_setpoint
                                    : outcome.confirmed_setpoint / this->transport_.profile.profile->pascals_per_meter;
        entity->publish_state(displayed);
      }
#endif
      break;
    case CommandPublication::COMMAND_PUBLICATION_REALIZED_STATUS:
#ifdef USE_TEXT_SENSOR
      if (this->realized_operation_text_sensor_ != nullptr) {
        const char *operation = operation_mode_to_string(outcome.realized_status.operation);
        if (operation != nullptr)
          this->realized_operation_text_sensor_->publish_state(operation);
        else
          ESP_LOGW(TAG, "[%s] unknown realized operation %u", this->parent()->address_str(),
                   outcome.realized_status.operation);
      }
      if (this->active_control_source_text_sensor_ != nullptr) {
        const char *source = control_source_to_string(outcome.realized_status.source);
        if (source != nullptr)
          this->active_control_source_text_sensor_->publish_state(source);
        else
          ESP_LOGW(TAG, "[%s] unknown active control source %u", this->parent()->address_str(),
                   outcome.realized_status.source);
      }
#endif
      break;
    case CommandPublication::COMMAND_PUBLICATION_NONE:
      break;
  }
  if (outcome.result == CommandResult::COMMAND_RESULT_IN_PROGRESS)
    return;
  if (outcome.result == CommandResult::COMMAND_RESULT_SUCCESS) {
    ESP_LOGD(TAG, "[%s] command verified", this->parent()->address_str());
  } else if (outcome.error == CommandError::COMMAND_ERROR_VERIFICATION_MISMATCH) {
    ESP_LOGW(TAG, "[%s] command verification mismatch; retaining confirmed pump value", this->parent()->address_str());
  } else {
    ESP_LOGW(TAG, "[%s] command failed, error=%u", this->parent()->address_str(), static_cast<unsigned>(outcome.error));
  }
  this->command_policy_.reset(this->command_state_);
}

void Alpha3::complete_command_transaction_(const ParsedFrame &frame) {
  const ActiveTransaction &tx = this->transport_.transaction;
  if (tx.is_write) {
    if (parse_write_ack(frame) == ParseResult::PARSE_RESULT_OK) {
      this->execute_command_outcome_(this->command_policy_.accept_write_ack(this->command_state_));
    } else {
      this->fail_active_transaction_("invalid write acknowledgement");
    }
    return;
  }
  const ObjectSchema *schema = this->transport_.profile.profile == nullptr
                                   ? nullptr
                                   : find_schema(*this->transport_.profile.profile, tx.expected_object);
  if (schema == nullptr) {
    this->fail_active_transaction_("missing command object schema");
    return;
  }
  // The policy copies/decodes the payload before execute resets the response assembler.
  CommandOutcome outcome;
  if (!this->command_policy_.accept_read_response(this->command_state_, *schema, frame, outcome)) {
    ESP_LOGW(TAG, "[%s] discarded mismatched command object response", this->parent()->address_str());
    return;
  }
  this->execute_command_outcome_(outcome);
}

void Alpha3::send_next_fragment_() {
  ActiveTransaction &tx = this->transport_.transaction;
  if (!tx.active || tx.sent_offset >= tx.frame.size)
    return;

  const size_t remaining = tx.frame.size - tx.sent_offset;
  const size_t fragment_size = std::min(remaining, BLE_FRAGMENT_SIZE);
  const esp_err_t status = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->geni_handle_, fragment_size,
      tx.frame.data.data() + tx.sent_offset, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "[%s] GENI fragment write failed, status=%d", this->parent()->address_str(), status);
    this->fail_active_transaction_("fragment submission failed");
    return;
  }

  tx.sent_offset += fragment_size;
  if (tx.sent_offset == tx.frame.size) {
    this->set_timeout("alpha3_response", RESPONSE_TIMEOUT_MS, [this]() { this->handle_response_timeout_(); });
  }
}

void Alpha3::handle_notification_(const uint8_t *data, size_t size) {
  if (!this->transport_.transaction.active) {
    ESP_LOGW(TAG, "[%s] discarded stale GENI notification", this->parent()->address_str());
    this->transport_.assembler.reset();
    return;
  }

  if (this->transport_.transaction.sent_offset != this->transport_.transaction.frame.size) {
    ESP_LOGW(TAG, "[%s] discarded GENI notification before request transmission completed",
             this->parent()->address_str());
    this->transport_.assembler.reset();
    return;
  }

  const ParseResult append_result = this->transport_.assembler.append(data, size);
  if (append_result == ParseResult::PARSE_RESULT_INCOMPLETE)
    return;
  if (append_result != ParseResult::PARSE_RESULT_OK) {
    ESP_LOGW(TAG, "[%s] discarded malformed GENI notification, error=%u", this->parent()->address_str(),
             static_cast<unsigned>(append_result));
    this->transport_.assembler.reset();
    if (this->transport_.transaction.is_write)
      this->fail_active_transaction_("malformed write acknowledgement");
    return;
  }

  if (this->transport_.transaction.work.kind == WorkKind::WORK_KIND_DISCOVER_ADDRESS) {
    uint8_t address;
    const ParseResult result =
        parse_discovery_response(this->transport_.assembler.data(), this->transport_.assembler.size(), address);
    if (result != ParseResult::PARSE_RESULT_OK) {
      ESP_LOGW(TAG, "[%s] discarded invalid GENI discovery response, error=%u", this->parent()->address_str(),
               static_cast<unsigned>(result));
    } else if (this->transport_.accept_discovered_address(address)) {
      this->cancel_timeout("alpha3_response");
      ESP_LOGI(TAG, "[%s] discovered GENI unit address 0x%02X", this->parent()->address_str(), address);
    }
    this->transport_.assembler.reset();
    return;
  }

  ParsedFrame frame;
  const ParseResult parse_result = parse_response_frame(
      this->transport_.assembler.data(), this->transport_.assembler.size(), this->transport_.unit_address, frame);
  if (parse_result != ParseResult::PARSE_RESULT_OK) {
    ESP_LOGW(TAG, "[%s] discarded invalid GENI response, error=%u", this->parent()->address_str(),
             static_cast<unsigned>(parse_result));
    this->transport_.assembler.reset();
    if (this->transport_.transaction.is_write)
      this->fail_active_transaction_("invalid write response");
    return;
  }

  if (this->command_state_.active)
    this->complete_command_transaction_(frame);
  else
    this->complete_read_(frame);
  this->transport_.assembler.reset();
}

void Alpha3::complete_read_(const ParsedFrame &frame) {
  ActiveTransaction &tx = this->transport_.transaction;
  if (!tx.active || tx.is_write) {
    ESP_LOGW(TAG, "[%s] discarded response without a matching read", this->parent()->address_str());
    return;
  }

  bool identity_completed = false;
  bool identity_sequence_finished = false;
  switch (tx.work.kind) {
    case WorkKind::WORK_KIND_READ_PARAMETER: {
      if (frame.data_class != GENI_PARAMETER_CLASS || frame.operation != GeniOperation::GENI_OPERATION_GET ||
          frame.data == nullptr || frame.data_size != 1) {
        ESP_LOGW(TAG, "[%s] discarded mismatched parameter response", this->parent()->address_str());
        return;
      }

      switch (tx.expected_parameter_id) {
        case UNIT_FAMILY_PARAMETER_ID:
          this->transport_.identity.family = frame.data[0];
          this->transport_.identity.valid_mask |= 0x01;
          break;
        case UNIT_TYPE_PARAMETER_ID:
          this->transport_.identity.type = frame.data[0];
          this->transport_.identity.valid_mask |= 0x02;
          break;
        case UNIT_VERSION_PARAMETER_ID:
          this->transport_.identity.version = frame.data[0];
          this->transport_.identity.valid_mask |= 0x04;
          identity_sequence_finished = true;
          break;
        case WARNING_PARAMETER_ID:
          if (this->warning_code_sensor_ != nullptr)
            this->warning_code_sensor_->publish_state(frame.data[0]);
          break;
        case ALARM_PARAMETER_ID:
          if (this->alarm_code_sensor_ != nullptr)
            this->alarm_code_sensor_->publish_state(frame.data[0]);
          break;
        default:
          ESP_LOGW(TAG, "[%s] discarded response for unexpected parameter %u", this->parent()->address_str(),
                   tx.expected_parameter_id);
          return;
      }
      identity_completed = identity_sequence_finished && this->transport_.identity.complete();
      break;
    }
    case WorkKind::WORK_KIND_READ_OBJECT: {
      if (frame.operation != GeniOperation::GENI_OPERATION_GET ||
          !can_read_object(this->transport_.profile, tx.expected_object)) {
        ESP_LOGW(TAG, "[%s] discarded mismatched object response", this->parent()->address_str());
        return;
      }
      const ObjectSchema *schema = find_schema(*this->transport_.profile.profile, tx.expected_object);
      if (schema == nullptr || !schema->readable) {
        ESP_LOGW(TAG, "[%s] discarded response for an unsupported object", this->parent()->address_str());
        return;
      }

      ParsedObject object;
      const ParseResult result =
          parse_object_response(frame, schema->object_type, schema->versions.data(), schema->version_count, object);
      if (result != ParseResult::PARSE_RESULT_OK) {
        ESP_LOGW(TAG, "[%s] discarded mismatched object response, error=%u", this->parent()->address_str(),
                 static_cast<unsigned>(result));
        return;
      }

      if (tx.expected_object == ObjectKind::OBJECT_KIND_HYDRAULIC_MODEL_B) {
        HydraulicTelemetry telemetry;
        if (!decode_model_b_hydraulic(object.payload, object.payload_size, telemetry)) {
          ESP_LOGW(TAG, "[%s] discarded invalid hydraulic payload", this->parent()->address_str());
          return;
        }
        if (this->flow_sensor_ != nullptr)
          this->flow_sensor_->publish_state(telemetry.flow_m3_h);
        if (this->head_sensor_ != nullptr)
          this->head_sensor_->publish_state(telemetry.head_m);
      } else if (tx.expected_object == ObjectKind::OBJECT_KIND_ELECTRICAL) {
        ElectricalTelemetry telemetry;
        if (!decode_electrical(object.payload, object.payload_size, object.version, telemetry)) {
          ESP_LOGW(TAG, "[%s] discarded invalid electrical payload", this->parent()->address_str());
          return;
        }
        if (this->voltage_sensor_ != nullptr)
          this->voltage_sensor_->publish_state(telemetry.voltage);
        if (this->current_sensor_ != nullptr)
          this->current_sensor_->publish_state(telemetry.current);
        if (this->power_sensor_ != nullptr)
          this->power_sensor_->publish_state(telemetry.power);
        if (this->speed_sensor_ != nullptr)
          this->speed_sensor_->publish_state(telemetry.speed_rpm);
      } else if (tx.expected_object == ObjectKind::OBJECT_KIND_HISTORY) {
        OperationHistory history;
        if (!decode_operation_history(object.payload, object.payload_size, history)) {
          ESP_LOGW(TAG, "[%s] discarded invalid history payload", this->parent()->address_str());
          return;
        }
        if (this->starts_sensor_ != nullptr)
          this->starts_sensor_->publish_state(history.starts);
        if (this->operating_hours_sensor_ != nullptr)
          this->operating_hours_sensor_->publish_state(history.operating_hours);
      } else if (tx.expected_object == ObjectKind::OBJECT_KIND_ENERGY) {
        double energy;
        if (!decode_energy_kwh(object.payload, object.payload_size, energy)) {
          ESP_LOGW(TAG, "[%s] discarded invalid energy payload", this->parent()->address_str());
          return;
        }
        if (this->energy_sensor_ != nullptr)
          this->energy_sensor_->publish_state(energy);
#ifdef USE_SELECT
      } else if (tx.expected_object == ObjectKind::OBJECT_KIND_LOCAL_OPERATION ||
                 tx.expected_object == ObjectKind::OBJECT_KIND_LOCAL_CONTROL) {
        OperationStatus status;
        if (!decode_operation_status(object.payload, object.payload_size, status)) {
          ESP_LOGW(TAG, "[%s] discarded invalid local operation/control payload", this->parent()->address_str());
          return;
        }
        if (tx.expected_object == ObjectKind::OBJECT_KIND_LOCAL_OPERATION) {
          if (status.operation <= 3) {
            if (this->operation_mode_select_ != nullptr)
              this->operation_mode_select_->publish_state(static_cast<size_t>(status.operation));
          } else {
            ESP_LOGW(TAG, "[%s] unknown local operation mode %u", this->parent()->address_str(), status.operation);
          }
        } else {
          const auto mode = std::find(CONTROL_MODE_VALUES.begin(), CONTROL_MODE_VALUES.end(), status.control);
          if (mode != CONTROL_MODE_VALUES.end()) {
            if (this->control_mode_select_ != nullptr)
              this->control_mode_select_->publish_state(static_cast<size_t>(mode - CONTROL_MODE_VALUES.begin()));
          } else {
            ESP_LOGW(TAG, "[%s] unknown local control mode %u", this->parent()->address_str(), status.control);
          }
        }
#endif
#ifdef USE_NUMBER
      } else if (tx.expected_object == ObjectKind::OBJECT_KIND_CS_FACTORY_LIMITS ||
                 tx.expected_object == ObjectKind::OBJECT_KIND_CP_FACTORY_LIMITS ||
                 tx.expected_object == ObjectKind::OBJECT_KIND_PP_FACTORY_LIMITS) {
        for (const auto &objects : SETPOINT_OBJECTS) {
          if (tx.expected_object != objects.factory_limits)
            continue;
          SetpointRange &range = *this->setpoint_ranges_.find(objects.user_config);
          SetpointLimits limits;
          if (!decode_setpoint_limits(object.payload, object.payload_size, limits) || !std::isfinite(limits.minimum) ||
              !std::isfinite(limits.maximum) || limits.minimum > limits.maximum) {
            range = {};
            ESP_LOGW(TAG, "[%s] discarded invalid factory limits", this->parent()->address_str());
            return;
          }
          range = {true, limits.minimum, limits.maximum};
          break;
        }
      } else if (tx.expected_object == ObjectKind::OBJECT_KIND_CS_USER_CONFIG ||
                 tx.expected_object == ObjectKind::OBJECT_KIND_CP_USER_CONFIG ||
                 tx.expected_object == ObjectKind::OBJECT_KIND_PP_USER_CONFIG) {
        float setpoint;
        if (!decode_setpoint(object.payload, object.payload_size, setpoint) || !std::isfinite(setpoint)) {
          ESP_LOGW(TAG, "[%s] discarded invalid setpoint payload", this->parent()->address_str());
          return;
        }
        if (auto *entity = this->get_setpoint_number_(tx.expected_object); entity != nullptr) {
          if (tx.expected_object != ObjectKind::OBJECT_KIND_CS_USER_CONFIG)
            setpoint /= this->transport_.profile.profile->pascals_per_meter;
          entity->publish_state(setpoint);
        }
#endif
#ifdef USE_TEXT_SENSOR
      } else if (tx.expected_object == ObjectKind::OBJECT_KIND_REALIZED_OPERATION) {
        OperationStatus status;
        if (!decode_operation_status(object.payload, object.payload_size, status)) {
          ESP_LOGW(TAG, "[%s] discarded invalid realized operation payload", this->parent()->address_str());
          return;
        }
        if (this->realized_operation_text_sensor_ != nullptr) {
          const char *operation = operation_mode_to_string(status.operation);
          if (operation != nullptr)
            this->realized_operation_text_sensor_->publish_state(operation);
          else
            ESP_LOGW(TAG, "[%s] unknown realized operation %u", this->parent()->address_str(), status.operation);
        }
        if (this->active_control_source_text_sensor_ != nullptr) {
          const char *source = control_source_to_string(status.source);
          if (source != nullptr)
            this->active_control_source_text_sensor_->publish_state(source);
          else
            ESP_LOGW(TAG, "[%s] unknown active control source %u", this->parent()->address_str(), status.source);
        }
#endif
      } else {
        ESP_LOGW(TAG, "[%s] discarded unhandled object response", this->parent()->address_str());
        return;
      }
      break;
    }
    case WorkKind::WORK_KIND_DISCOVER_ADDRESS:
    case WorkKind::WORK_KIND_SET_OPERATION_MODE:
    case WorkKind::WORK_KIND_SET_CONTROL_MODE:
    case WorkKind::WORK_KIND_SET_SETPOINT:
      ESP_LOGW(TAG, "[%s] discarded response for non-read work", this->parent()->address_str());
      return;
  }

  this->cancel_timeout("alpha3_response");
  this->transport_.transaction = {};

  if (!identity_completed) {
    if (identity_sequence_finished) {
      ESP_LOGW(TAG, "[%s] incomplete identity family=%u type=%u version=%u mask=0x%02X", this->parent()->address_str(),
               this->transport_.identity.family, this->transport_.identity.type, this->transport_.identity.version,
               this->transport_.identity.valid_mask);
    }
    return;
  }

#ifdef USE_NUMBER
  this->setpoint_ranges_.clear();
#endif
  this->transport_.profile = match_profile(this->transport_.identity);
  ESP_LOGI(TAG, "[%s] identity family=%u type=%u version=%u mask=0x%02X", this->parent()->address_str(),
           this->transport_.identity.family, this->transport_.identity.type, this->transport_.identity.version,
           this->transport_.identity.valid_mask);
  if (this->transport_.profile.profile == nullptr) {
    ESP_LOGW(TAG, "[%s] unsupported identity; transport remains read-only", this->parent()->address_str());
    return;
  }
  ESP_LOGI(TAG, "[%s] selected profile %s", this->parent()->address_str(), this->transport_.profile.profile->name);
  this->enqueue_initial_reads_();
}

void Alpha3::fail_active_transaction_(const char *reason) {
  if (!this->transport_.transaction.active)
    return;
  ESP_LOGW(TAG, "[%s] GENI transaction failed: %s", this->parent()->address_str(), reason);
  if (this->transport_.transaction.is_write && this->command_state_.active && this->is_ready()) {
    ESP_LOGW(TAG, "[%s] write result uncertain; verifying without resending SET", this->parent()->address_str());
    this->execute_command_outcome_(this->command_policy_.handle_write_uncertain(this->command_state_));
    return;
  }
  if (this->command_state_.active) {
    ESP_LOGW(TAG, "[%s] command ended without confirmation at stage %u", this->parent()->address_str(),
             static_cast<unsigned>(this->command_state_.stage));
    this->command_policy_.reset(this->command_state_);
  }
  const bool identity_read_failed = this->transport_.transaction.work.kind == WorkKind::WORK_KIND_READ_PARAMETER &&
                                    this->transport_.transaction.expected_parameter_id >= UNIT_FAMILY_PARAMETER_ID &&
                                    this->transport_.transaction.expected_parameter_id <= UNIT_VERSION_PARAMETER_ID;
  this->cancel_timeout("alpha3_response");
  this->transport_.transaction = {};
  this->transport_.assembler.reset();
  if (identity_read_failed && this->transport_.queue.size() == 0) {
    ESP_LOGW(TAG, "[%s] incomplete identity family=%u type=%u version=%u mask=0x%02X", this->parent()->address_str(),
             this->transport_.identity.family, this->transport_.identity.type, this->transport_.identity.version,
             this->transport_.identity.valid_mask);
  }
}

void Alpha3::handle_response_timeout_() {
  switch (this->transport_.handle_timeout()) {
    case TimeoutDecision::TIMEOUT_DECISION_DISCOVERY_FALLBACK:
      ESP_LOGW(TAG, "[%s] GENI discovery timed out; using legacy unit address 0x%02X", this->parent()->address_str(),
               this->transport_.unit_address);
      break;
    case TimeoutDecision::TIMEOUT_DECISION_RETRY_READ: {
      ESP_LOGW(TAG, "[%s] GENI read timed out; retrying once", this->parent()->address_str());
      this->cancel_timeout("alpha3_response");
      this->transport_.assembler.reset();
      ActiveTransaction &tx = this->transport_.transaction;
      tx.sent_offset = 0;
      bool built;
      if (tx.work.kind == WorkKind::WORK_KIND_READ_PARAMETER) {
        built = build_parameter_get(this->transport_.unit_address, tx.expected_parameter_id, tx.frame);
      } else {
        const ObjectSchema *schema = this->transport_.profile.profile == nullptr
                                         ? nullptr
                                         : find_schema(*this->transport_.profile.profile, tx.expected_object);
        built = schema != nullptr && build_object_get(this->transport_.unit_address, schema->address, tx.frame);
      }
      if (!built)
        this->fail_active_transaction_("cannot rebuild read for retry");
      break;
    }
    case TimeoutDecision::TIMEOUT_DECISION_VERIFY_WRITE:
      this->fail_active_transaction_("write response timed out");
      break;
    case TimeoutDecision::TIMEOUT_DECISION_FAIL:
      this->fail_active_transaction_("response timeout");
      break;
  }
}

void Alpha3::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t, esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_CONNECT_EVT: {
      if (std::memcmp(param->connect.remote_bda, this->parent()->get_remote_bda(), sizeof(esp_bd_addr_t)) != 0)
        return;
      this->reset_connection_state_();
      const esp_err_t status = esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT);
      if (status != ESP_OK)
        ESP_LOGW(TAG, "[%s] encryption request failed, status=%d", this->parent()->address_str(), status);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] service discovery failed, status=%d", this->parent()->address_str(),
                 param->search_cmpl.status);
        break;
      }
      auto *characteristic =
          this->parent()->get_characteristic(ALPHA3_GENI_SERVICE_UUID, ALPHA3_GENI_CHARACTERISTIC_UUID);
      if (characteristic == nullptr) {
        ESP_LOGW(TAG, "[%s] GENI service/characteristic discovery failed", this->parent()->address_str());
        break;
      }
      auto *descriptor = characteristic->get_descriptor(CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID);
      if (descriptor == nullptr) {
        ESP_LOGW(TAG, "[%s] GENI CCCD discovery failed", this->parent()->address_str());
        break;
      }
      this->geni_handle_ = characteristic->handle;
      this->transport_.readiness.mark_service_found(characteristic->handle, descriptor->handle);
      ESP_LOGD(TAG, "[%s] GENI characteristic=0x%04X cccd=0x%04X", this->parent()->address_str(),
               characteristic->handle, descriptor->handle);
      this->try_subscribe_();
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.handle != this->transport_.readiness.characteristic_handle())
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] notification registration failed, status=%d", this->parent()->address_str(),
                 param->reg_for_notify.status);
        break;
      }
      this->transport_.readiness.mark_registration_succeeded();
      ESP_LOGD(TAG, "[%s] notification registration succeeded", this->parent()->address_str());
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.handle != this->transport_.readiness.cccd_handle())
        break;
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] notification CCCD write failed, status=%d", this->parent()->address_str(),
                 param->write.status);
        break;
      }
      const bool was_ready = this->is_ready();
      this->transport_.readiness.mark_cccd_succeeded();
      if (!this->transport_.readiness.ready()) {
        ESP_LOGW(TAG, "[%s] notification CCCD completed before registration", this->parent()->address_str());
        break;
      }
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->set_ready_(true);
      if (!was_ready)
        this->transport_.start_discovery();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.handle == this->geni_handle_)
        this->handle_notification_(param->notify.value, param->notify.value_len);
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      if (!this->parent()->check_addr(param->disconnect.remote_bda))
        return;
      this->reset_connection_state_();
      this->node_state = espbt::ClientState::IDLE;
      break;
    default:
      break;
  }
}

void Alpha3::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT || !this->parent()->check_addr(param->ble_security.auth_cmpl.bd_addr))
    return;

  if (!param->ble_security.auth_cmpl.success) {
    ESP_LOGW(TAG, "[%s] authentication failed, status=%d", this->parent()->address_str(),
             param->ble_security.auth_cmpl.fail_reason);
    return;
  }
  this->transport_.readiness.mark_authenticated();
  ESP_LOGD(TAG, "[%s] authentication succeeded", this->parent()->address_str());
  this->try_subscribe_();
}

}  // namespace esphome::alpha3

#endif
