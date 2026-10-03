#include "hub.h"
#include <algorithm>
#include <cstdio>
#include "esphome/core/controller_registry.h"
#include "esphome/core/helpers.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42";

// Logs a rejected conversation at a severity matching what's about to happen to its entity: ERROR if
// this rejection is actually invalidating it now, WARN if should_invalidate_now_() decided to mask it
// (a DATA_INVALID within max_data_invalid's grace period -- see its declaration comment) so the
// severity reflects that nothing user-visible happened. Expands to the normal ESP_LOGE/ESP_LOGW
// macros, so compile-time log-level stripping still applies to whichever branch is actually reachable.
// Also marks the current sweep as having had an error regardless of severity -- see
// sweep_had_error_'s declaration comment -- since a masked DATA_INVALID is exactly the kind of
// otherwise-invisible-outside-the-log condition that sensor is meant to surface.
#define OT42_LOG_REJECTION(invalidate_now, ...) \
  do { \
    if (invalidate_now) { \
      ESP_LOGE(TAG, __VA_ARGS__); \
    } else { \
      ESP_LOGW(TAG, __VA_ARGS__); \
    } \
    this->sweep_had_error_ = true; \
  } while (0)

// Same as OT42_LOG_REJECTION, for kinds with no should_invalidate_now_() grace period (every
// rejection invalidates immediately) -- MASTER_CONFIG/MASTER_OPENTHERM_VERSION/
// MASTER_PRODUCT_VERSION, DAY_TIME/DATE/YEAR, BRAND/BRAND_VERSION/BRAND_SERIAL_NUMBER, TSP, FHB.
#define OT42_LOG_REJECTION_ALWAYS(...) \
  do { \
    ESP_LOGE(TAG, __VA_ARGS__); \
    this->sweep_had_error_ = true; \
  } while (0)

// set_has_state(false) alone doesn't notify already-connected API/web_server clients -- only
// publish_state() does, via each domain's own internal call to ControllerRegistry. An
// already-subscribed client would otherwise keep showing the last known value forever after a
// rejected write or failed conversation, so the notification has to be triggered explicitly here.
// One overload per entity type used below, so every set_has_state(false) call site in this file
// can go through this instead of the bare call.
//
// Every notify_*_update() call below is wrapped in the matching #ifdef USE_*: entity_types.h only
// generates each ControllerRegistry member when at least one entity of that domain exists
// *anywhere* in the device's config, not specifically in opentherm42 -- e.g. a device with no
// sensor: platform of any kind anywhere would fail to compile on notify_sensor_update() otherwise,
// even though the guarded call can only be reached when the corresponding entity pointer is
// non-null, which itself requires that define to be set. This, not an ESPHome version difference,
// is what caused the original real-world "is not a member of ControllerRegistry" build failures on
// switch/number: the local test YAML always configures at least one of every domain, so it never
// exercised a config shaped like the ones that failed. Applied to all five domains here for the
// same reason, since none of them are actually mandatory in an opentherm42 config either.
static void invalidate_entity(sensor::Sensor *entity) {
  if (entity == nullptr) {
    return;
  }
  entity->set_has_state(false);
#ifdef USE_SENSOR
  ControllerRegistry::notify_sensor_update(entity);
#endif
}
static void invalidate_entity(number::Number *entity) {
  if (entity == nullptr) {
    return;
  }
  entity->set_has_state(false);
#ifdef USE_NUMBER
  ControllerRegistry::notify_number_update(entity);
#endif
}
static void invalidate_entity(text_sensor::TextSensor *entity) {
  if (entity == nullptr) {
    return;
  }
  entity->set_has_state(false);
#ifdef USE_TEXT_SENSOR
  ControllerRegistry::notify_text_sensor_update(entity);
#endif
}
static void invalidate_entity(select::Select *entity) {
  if (entity == nullptr) {
    return;
  }
  entity->set_has_state(false);
#ifdef USE_SELECT
  ControllerRegistry::notify_select_update(entity);
#endif
}
static void invalidate_entity(binary_sensor::BinarySensor *entity) {
  if (entity == nullptr) {
    return;
  }
  entity->set_has_state(false);
#ifdef USE_BINARY_SENSOR
  ControllerRegistry::notify_binary_sensor_update(entity);
#endif
}
static void invalidate_entity(switch_::Switch *entity) {
  if (entity == nullptr) {
    return;
  }
  entity->set_has_state(false);
#ifdef USE_SWITCH
  ControllerRegistry::notify_switch_update(entity);
#endif
}

// clang-format off
const SimpleSensorInfo OpenTherm42Hub::SIMPLE_SENSORS[] = {
    {RequestKind::NUMBER_OF_TSPS, 10, SimpleValueKind::U8_HB, &OpenTherm42Hub::number_of_tsps_sensor_, "Number of TSP's (id=10)"},
    {RequestKind::NUMBER_OF_TSPS_VENTILATION, 88, SimpleValueKind::U8_HB, &OpenTherm42Hub::number_of_tsps_ventilation_sensor_, "Number of TSP's ventilation/heat-recovery (id=88)"},
    {RequestKind::NUMBER_OF_TSPS_SOLAR_STORAGE, 105, SimpleValueKind::U8_HB, &OpenTherm42Hub::number_of_tsps_solar_storage_sensor_, "Number of TSP's Solar Storage (id=105)"},
    {RequestKind::FAULT_HISTORY_BUFFER_SIZE, 12, SimpleValueKind::U8_HB, &OpenTherm42Hub::fault_history_buffer_size_sensor_, "Size of Fault Buffer (id=12)"},
    {RequestKind::FAULT_HISTORY_BUFFER_SIZE_VENTILATION, 90, SimpleValueKind::U8_HB, &OpenTherm42Hub::fault_history_buffer_size_ventilation_sensor_, "Size of Fault Buffer ventilation/heat-recovery (id=90)"},
    {RequestKind::FAULT_HISTORY_BUFFER_SIZE_SOLAR_STORAGE, 107, SimpleValueKind::U8_HB, &OpenTherm42Hub::fault_history_buffer_size_solar_storage_sensor_, "Size of Fault Buffer Solar Storage (id=107)"},
    {RequestKind::OPENTHERM_VERSION_BOILER, 125, SimpleValueKind::F88, &OpenTherm42Hub::opentherm_version_boiler_sensor_, "OpenTherm version Boiler (id=125)"},
    {RequestKind::OPENTHERM_VERSION_VENTILATION, 75, SimpleValueKind::F88, &OpenTherm42Hub::opentherm_version_ventilation_sensor_, "OpenTherm version ventilation/heat-recovery (id=75)"},
};
// clang-format on

// §5.3.1 Class 1, ID 101 LB bits 3,2,1 (Solar Storage mode and status: Solar mode) -- same 5-state
// enum as the select platform's ID 101 HB (Master Solar Storage status), but this is the boiler's
// own independently-reported value, not a readback of HB.
static const char *solar_mode_to_string(uint8_t code) {
  switch (code) {
    case 0:
      return "Off";
    case 1:
      return "DHW Eco";
    case 2:
      return "DHW Comfort";
    case 3:
      return "DHW Single Boost";
    case 4:
      return "DHW Continuous Boost";
    default:
      return "Reserved";
  }
}

// §5.3.1 Class 1, ID 101 LB bits 5,4: Solar Storage mode and status: Solar status.
static const char *solar_status_to_string(uint8_t code) {
  switch (code) {
    case 0:
      return "Standby";
    case 1:
      return "Loading By Sun";
    case 2:
      return "Loading By Boiler";
    default:
      return "Anti-Legionella";
  }
}

// §5.3.4 Class 4, ID 20 HB bits 7-5 (read side): 1=Monday..7=Sunday. Returns "?" both when the
// boiler reports 0 ("no day-of-week information available" per the spec's own ID 20 table) and
// when this sub-field hasn't been read yet -- date_time_text_sensor_ doesn't need to distinguish
// those two cases from each other.

const SimpleSensorInfo *OpenTherm42Hub::find_simple_sensor_(RequestKind kind) const {
  for (auto const &info : SIMPLE_SENSORS) {
    if (info.kind == kind) {
      return &info;
    }
  }
  return nullptr;
}

const SimpleSensorInfo *OpenTherm42Hub::find_simple_sensor_by_id_(uint8_t id) const {
  for (auto const &info : SIMPLE_SENSORS) {
    if (info.id == id) {
      return &info;
    }
  }
  return nullptr;
}

void OpenTherm42Hub::setup() {
  this->datalink_ = make_unique<OpenThermDataLink>(this->in_pin_, this->out_pin_);
  if (!this->datalink_->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize the OpenTherm datalink (%s); see previous log messages for details",
             timer_error_to_string(this->datalink_->get_timer_error()));
    this->mark_failed();
    return;
  }
  this->build_schedule_();
}

void OpenTherm42Hub::loop() {
  switch (this->datalink_->get_state()) {
    case DataLinkState::IDLE: {
      if (millis() - this->last_conversation_end_ms_ < MASTER_WAIT_TIME_MS) {
        return;  // §4.3.1 MWT: wait at least 100 ms since the end of the previous conversation.
      }
      this->datalink_->send(this->build_next_request_());
      return;
    }
    case DataLinkState::SENT:
      this->datalink_->listen(RESPONSE_TIMEOUT_MS);
      return;
    case DataLinkState::RECEIVED:
      this->handle_response_(this->datalink_->get_frame());
      this->last_conversation_end_ms_ = millis();
      this->datalink_->stop();
      return;
    case DataLinkState::ERROR: {
      char kind_desc[80];
      this->describe_request_kind_(this->pending_request_kind_, kind_desc, sizeof(kind_desc));
      ESP_LOGE(TAG, "Conversation failed: %s (%s)", data_link_error_to_string(this->datalink_->get_error()), kind_desc);
      this->sweep_had_error_ = true;
      this->invalidate_response_(this->pending_request_kind_);
      this->last_conversation_end_ms_ = millis();
      this->datalink_->stop();
      return;
    }
    default:
      return;  // SENDING/LISTENING/RECEIVING: bit-level progress driven by the datalink's timer ISR.
  }
}

void OpenTherm42Hub::build_schedule_() {
  // §5.2's two mandatory ids: unconditionally scheduled regardless of which, if any, entity is
  // configured for their bits -- see Entry's declaration comment for why there's no more separate
  // "reserved" tier at the C++ level. CONTROL_SETPOINT is a required entity in config, but it's
  // added unconditionally here (rather than gated on control_setpoint_number_ != nullptr) so this
  // line's meaning obviously matches the spec mandate rather than an implementation detail of how
  // config validation happens to guarantee that pointer is always set.
  this->add_entry_(RequestKind::STATUS);
  this->add_entry_(RequestKind::CONTROL_SETPOINT);
  // §5.3.2 Class 2, ID 3: unlike STATUS above, nothing in the spec requires continuously re-reading
  // id 3 regardless of which entities are configured -- see
  // set_configuration_information_boiler_configuration_update_every()'s declaration comment -- so
  // this group is gated like every other one below.
  if (this->configuration_information_boiler_configuration_dhw_present_text_sensor_ != nullptr ||
      this->configuration_information_boiler_configuration_control_type_text_sensor_ != nullptr ||
      this->configuration_information_boiler_configuration_cooling_config_text_sensor_ != nullptr ||
      this->configuration_information_boiler_configuration_dhw_config_text_sensor_ != nullptr ||
      this->configuration_information_boiler_configuration_master_low_off_and_pump_control_function_text_sensor_ !=
          nullptr ||
      this->configuration_information_boiler_configuration_ch2_present_text_sensor_ != nullptr ||
      this->configuration_information_boiler_configuration_remote_water_filling_function_text_sensor_ != nullptr ||
      this->configuration_information_boiler_configuration_heat_cool_mode_control_text_sensor_ != nullptr ||
      this->boiler_member_id_code_sensor_ != nullptr) {
    this->add_entry_(RequestKind::BOILER_CONFIG);
  }
  // §5.3.2 Class 2, IDs 2/124/126: this master's own identity, announced to the boiler -- no entity
  // of its own, so unconditionally scheduled like STATUS above (see their
  // set_configuration_information_master_*_update_every() declaration comments).
  this->add_entry_(RequestKind::MASTER_CONFIG);
  this->add_entry_(RequestKind::MASTER_OPENTHERM_VERSION);
  this->add_entry_(RequestKind::MASTER_PRODUCT_VERSION);
  // §5.3.2 Class 2, IDs 93/94/95: brand identification strings, only scheduled if a text_sensor is
  // configured for that string (mirrors the old startup_item_actionable_()'s BRAND* gating, which
  // no longer exists as a separate mechanism). 1:1, so each uses its own staged cadence -- see
  // brand_update_every_'s declaration comment -- same reasoning as the other bespoke sensors below.
  if (this->brand_.sensor != nullptr) {
    this->entries_.push_back({RequestKind::BRAND, this->brand_update_every_});
  }
  if (this->brand_version_.sensor != nullptr) {
    this->entries_.push_back({RequestKind::BRAND_VERSION, this->brand_version_update_every_});
  }
  if (this->brand_serial_number_.sensor != nullptr) {
    this->entries_.push_back({RequestKind::BRAND_SERIAL_NUMBER, this->brand_serial_number_update_every_});
  }

  if (this->control_setpoint_2_number_ != nullptr) {
    this->add_entry_(RequestKind::CONTROL_SETPOINT_2);
  }
  if (this->ventilation_status_write_.any_configured() || this->ventilation_status_read_.any_configured()) {
    this->add_entry_(RequestKind::VENTILATION_STATUS);
  }
  if (this->control_setpoint_ventilation_number_ != nullptr) {
    this->add_entry_(RequestKind::CONTROL_SETPOINT_VENTILATION);
  }

  if (this->fault_flags_read_.any_configured() || this->oem_fault_code_sensor_ != nullptr) {
    this->add_entry_(RequestKind::FAULT_FLAGS);
  }
  if (this->ventilation_fault_flags_read_.any_configured() || this->oem_fault_code_ventilation_sensor_ != nullptr) {
    this->add_entry_(RequestKind::VENTILATION_FAULT_FLAGS);
  }
  // The select is an active control input (like the Class 1 setpoints below); read-only consumers
  // alone still get the same single Entry either way -- see build_next_request_()/handle_response_()
  // for how the select's HB and the sensors' LB stay independent.
  if (this->master_solar_storage_status_solar_mode_select_ != nullptr ||
      this->solar_storage_fault_indication_binary_sensor_ != nullptr ||
      this->solar_storage_mode_and_status_solar_mode_text_sensor_ != nullptr ||
      this->solar_storage_mode_and_status_solar_status_text_sensor_ != nullptr) {
    this->add_entry_(RequestKind::SOLAR_STORAGE_STATUS);
  }
  // These three are 1:1 but bespoke (not dispatched through SIMPLE_SENSORS -- see
  // handle_response_()/build_next_request_()), so their cadence is stored separately by
  // set_..._update_every() at wiring time and consumed here, same reasoning as
  // date_time_read_update_every_ (below, for DAY_TIME_READ) and set_number_update_every().
  if (this->oem_fault_code_solar_storage_sensor_ != nullptr) {
    this->entries_.push_back(
        {RequestKind::SOLAR_STORAGE_FAULT_FLAGS, this->oem_fault_code_solar_storage_update_every_});
  }
  if (this->oem_diagnostic_code_sensor_ != nullptr) {
    this->entries_.push_back({RequestKind::OEM_DIAGNOSTIC_CODE, this->oem_diagnostic_code_update_every_});
  }
  if (this->oem_diagnostic_code_ventilation_sensor_ != nullptr) {
    this->entries_.push_back(
        {RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION, this->oem_diagnostic_code_ventilation_update_every_});
  }

  if (this->configuration_information_configuration_ventilation_heat_recovery_system_type_text_sensor_ != nullptr ||
      this->configuration_information_configuration_ventilation_heat_recovery_bypass_text_sensor_ != nullptr ||
      this->configuration_information_configuration_ventilation_heat_recovery_speed_control_text_sensor_ != nullptr ||
      this->member_id_code_ventilation_sensor_ != nullptr) {
    this->add_entry_(RequestKind::VENTILATION_CONFIGURATION);
  }
  if (this->configuration_information_solar_storage_configuration_system_type_text_sensor_ != nullptr ||
      this->solar_storage_member_id_sensor_ != nullptr) {
    this->add_entry_(RequestKind::SOLAR_STORAGE_CONFIGURATION);
  }
  // OPENTHERM_VERSION_BOILER/OPENTHERM_VERSION_VENTILATION are deliberately NOT seeded here: both
  // are plain F88 reads listed in SIMPLE_SENSORS below, which already gates their existence on the
  // same sensor pointer and applies the real staged update_every -- adding them a second time here
  // would give each a duplicate Entry (the one added here defaulting to update_every=1 forever,
  // silently overriding the user's configured cadence and permanently doubling their bus traffic).
  if (this->boiler_product_type_sensor_ != nullptr || this->boiler_product_version_sensor_ != nullptr) {
    this->add_entry_(RequestKind::PRODUCT_VERSION_BOILER);
  }
  if (this->ventilation_product_type_sensor_ != nullptr || this->ventilation_product_version_sensor_ != nullptr) {
    this->add_entry_(RequestKind::PRODUCT_VERSION_VENTILATION);
  }
  if (this->solar_storage_product_type_sensor_ != nullptr || this->solar_storage_product_version_sensor_ != nullptr) {
    this->add_entry_(RequestKind::PRODUCT_VERSION_SOLAR_STORAGE);
  }

  // Every plain read-only sensor: scheduled if its entity is configured, at whatever cadence
  // set_simple_sensor_update_every() staged for its id (see pending_simple_sensor_update_every_'s
  // declaration comment), falling back to the default (every pass) if none was staged
  // (config-schema always supplies one via a required field, so this fallback is only reached if
  // some marker were ever added to SIMPLE_SENSORS without a matching update_every schema field in a
  // platform's config).
  for (auto const &info : SIMPLE_SENSORS) {
    if (this->*(info.member) == nullptr) {
      continue;
    }
    uint32_t update_every = 1;
    for (auto const &pending : this->pending_simple_sensor_update_every_) {
      if (pending.first == info.id) {
        update_every = pending.second;
        break;
      }
    }
    this->entries_.push_back({info.kind, update_every});
  }
  this->pending_simple_sensor_update_every_.clear();
  this->pending_simple_sensor_update_every_.shrink_to_fit();

  // §5.3.6 Class 6: one Entry round-robins through every configured TSP for periodic reads;
  // on-demand writes (see write_tsp()) are serviced ahead of this rotation.
  if (!this->tsp_slots_.empty()) {
    this->add_entry_(RequestKind::TSP);
  }

  // §5.3.7 Class 7: same round-robin, for fault-history-buffer entries (purely read-only).
  if (!this->fhb_slots_.empty()) {
    this->add_entry_(RequestKind::FHB);
  }

  // Every hub-level group option staged its cadence at wiring time (see
  // pending_group_update_every_'s declaration comment) since entries_ didn't exist yet back then --
  // apply them now that every add_entry_() call above has run.
  for (auto const &pending : this->pending_group_update_every_) {
    if (Entry *entry = this->find_entry_(pending.first); entry != nullptr) {
      entry->update_every = pending.second;
    }
  }
  this->pending_group_update_every_.clear();
  this->pending_group_update_every_.shrink_to_fit();

  // One sweep = enough passes for every currently-configured entry to be attempted at least once --
  // see sweep_length_passes_'s declaration comment. Computed once here since update_every values
  // are fixed at config time.
  this->sweep_length_passes_ = 1;
  for (auto const &entry : this->entries_) {
    this->sweep_length_passes_ = std::max(this->sweep_length_passes_, entry.update_every);
  }
  this->sweep_start_ms_ = millis();
  this->pass_start_ms_ = this->sweep_start_ms_;
}

Frame OpenTherm42Hub::build_next_request_() {
  if (this->remote_request_pending_) {
    this->remote_request_pending_ = false;
    this->pending_request_kind_ = RequestKind::REMOTE_REQUEST;
    Frame frame{};
    frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
    frame.id = 4;
    frame.value_hb = this->remote_request_code_;
    this->log_outgoing_frame_(frame);
    return frame;
  }
  if (this->tsp_write_pending_) {
    this->tsp_write_pending_ = false;
    this->pending_request_kind_ = RequestKind::TSP;
    this->pending_tsp_slot_index_ = this->tsp_write_slot_index_;
    this->pending_tsp_is_write_ = true;
    Frame frame{};
    auto const &slot = this->tsp_slots_[this->tsp_write_slot_index_];
    frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
    frame.id = slot.data_id;
    frame.value_hb = slot.index;
    frame.value_lb = this->tsp_write_value_;
    this->log_outgoing_frame_(frame);
    return frame;
  }
  // ASAP: a dirty write jumps the queue immediately, ahead of the ordinary pass-pull below -- see
  // Entry's declaration comment. Scans every entry (not just STATUS/CONTROL_SETPOINT -- a prior
  // version of this scheduler only scanned those two, silently leaving every other write id's ASAP
  // path dead code). Does not touch cursor_/pass_counter_ -- purely an out-of-band send.
  for (auto &entry : this->entries_) {
    if (entry.dirty) {
      entry.dirty = false;
      return this->build_entry_request_(entry.kind);
    }
  }
  if (optional<Frame> frame = this->pull_next_due_entry_(); frame.has_value()) {
    return *frame;
  }
  // Nothing due anywhere for this pass, and pull_next_due_entry_() already advanced past it -- see
  // its declaration comment for why STATUS is always a safe, immediate filler here.
  return this->build_entry_request_(RequestKind::STATUS);
}

Frame OpenTherm42Hub::build_entry_request_(RequestKind kind) {
  Frame frame{};
  this->pending_request_kind_ = kind;

  switch (kind) {
    case RequestKind::BOILER_CONFIG:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 3;
      break;
    case RequestKind::STATUS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 0;
      frame.value_hb = this->master_status_write_.pack();
      break;
    case RequestKind::CONTROL_SETPOINT:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 1;
      frame.set_value_f88(this->control_setpoint_write_value_);
      break;
    case RequestKind::CONTROL_SETPOINT_2:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 8;
      frame.set_value_f88(this->control_setpoint_2_write_value_);
      break;
    case RequestKind::VENTILATION_STATUS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 70;
      frame.value_hb = this->ventilation_status_write_.pack();
      break;
    case RequestKind::CONTROL_SETPOINT_VENTILATION:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 71;
      frame.value_lb = static_cast<uint8_t>(this->control_setpoint_ventilation_write_value_);
      break;
    case RequestKind::FAULT_FLAGS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 5;
      break;
    case RequestKind::VENTILATION_FAULT_FLAGS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 72;
      break;
    case RequestKind::SOLAR_STORAGE_STATUS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 101;
      // §5.3.1 ID 101 HB: master-authored (see hub.h's RequestKind comment) -- send whatever was
      // last commanded, exactly like the STATUS/VENTILATION_STATUS master-status bytes. Read from
      // solar_storage_solar_mode_write_value_, not the select's own ->active_index(): the latter
      // returns nullopt once the select is invalidated, which would otherwise silently start
      // sending index 0 on the wire in addition to displaying Unknown (see set_solar_storage_
      // solar_mode_write_value()'s declaration comment).
      frame.value_hb = this->solar_storage_solar_mode_write_value_;
      break;
    case RequestKind::SOLAR_STORAGE_FAULT_FLAGS:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 102;
      break;
    case RequestKind::OEM_DIAGNOSTIC_CODE:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 115;
      break;
    case RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 73;
      break;
    case RequestKind::MASTER_CONFIG:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 2;
      // bit 0 Smart Power: always 0 (not implemented). §3.4.2 defines Smart Power as a physical-layer
      // negotiation where the master signals support by actually switching the bus's idle voltage
      // level between Low/Medium/High (§3.4.2.4/§3.4.2.5) -- not a data value. This datalink is a
      // fixed-idle-level GPIO bit-banger (see OpenThermDataLink) with no concept of variable idle
      // voltage/current, so it cannot perform that switching. Claiming support here would be a false
      // promise: §3.4.2.3 warns a boiler that believes Smart Power is supported may switch to high
      // idle current expecting power this interface was never designed to deliver.
      frame.value_hb = 0;
      frame.value_lb = this->controller_member_id_code_;
      break;
    case RequestKind::MASTER_OPENTHERM_VERSION:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 124;
      frame.set_value_f88(CONTROLLER_OPENTHERM_VERSION);
      break;
    case RequestKind::MASTER_PRODUCT_VERSION:
      frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
      frame.id = 126;
      frame.value_hb = this->controller_product_type_;
      frame.value_lb = this->controller_product_version_;
      break;
    case RequestKind::VENTILATION_CONFIGURATION:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 74;
      break;
    case RequestKind::SOLAR_STORAGE_CONFIGURATION:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 103;
      break;
    case RequestKind::PRODUCT_VERSION_BOILER:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 127;
      break;
    case RequestKind::PRODUCT_VERSION_VENTILATION:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 76;
      break;
    case RequestKind::PRODUCT_VERSION_SOLAR_STORAGE:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 104;
      break;
    case RequestKind::BRAND:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 93;
      frame.value_hb = this->brand_.next_index;
      break;
    case RequestKind::BRAND_VERSION:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 94;
      frame.value_hb = this->brand_version_.next_index;
      break;
    case RequestKind::BRAND_SERIAL_NUMBER:
      frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
      frame.id = 95;
      frame.value_hb = this->brand_serial_number_.next_index;
      break;
    case RequestKind::REMOTE_REQUEST:
      break;  // built directly in build_next_request_() before this switch, unreachable here

    case RequestKind::TSP:
      // Only reached for the periodic-read rotation -- on-demand writes are intercepted by the
      // tsp_write_pending_ check above build_next_request_()'s switch.
      if (!this->tsp_slots_.empty()) {
        this->pending_tsp_slot_index_ = this->tsp_read_index_;
        this->pending_tsp_is_write_ = false;
        auto const &slot = this->tsp_slots_[this->tsp_read_index_];
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
        frame.id = slot.data_id;
        frame.value_hb = slot.index;
        this->tsp_read_index_ = (this->tsp_read_index_ + 1) % this->tsp_slots_.size();
      }
      break;

    case RequestKind::FHB:
      if (!this->fhb_slots_.empty()) {
        this->pending_fhb_slot_index_ = this->fhb_read_index_;
        auto const &slot = this->fhb_slots_[this->fhb_read_index_];
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
        frame.id = slot.data_id;
        frame.value_hb = slot.index;
        this->fhb_read_index_ = (this->fhb_read_index_ + 1) % this->fhb_slots_.size();
      }
      break;

    default: {
      // Every plain read-only sensor (see the SIMPLE_SENSORS table) shares this one case.
      const SimpleSensorInfo *info = this->find_simple_sensor_(kind);
      if (info != nullptr) {
        frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
        frame.id = info->id;
      }
      break;  // info == nullptr only for kinds handled explicitly above, unreachable here
    }
  }
  this->log_outgoing_frame_(frame);
  return frame;
}

void OpenTherm42Hub::set_write_value(uint8_t id, float value) {
  float *write_value;
  RequestKind kind;
  switch (id) {
    case 1:
      write_value = &this->control_setpoint_write_value_;
      kind = RequestKind::CONTROL_SETPOINT;
      break;
    case 8:
      write_value = &this->control_setpoint_2_write_value_;
      kind = RequestKind::CONTROL_SETPOINT_2;
      break;
    case 71:
      write_value = &this->control_setpoint_ventilation_write_value_;
      kind = RequestKind::CONTROL_SETPOINT_VENTILATION;
      break;
    default:
      return;
  }
  *write_value = value;
  // ASAP: jump the queue rather than wait for this id's own due time -- see Entry's declaration
  // comment. nullptr-safe: only reachable once the matching entity's control()/setup() has run,
  // which requires build_schedule_() to already have scheduled this kind.
  if (Entry *entry = this->find_entry_(kind); entry != nullptr) {
    entry->dirty = true;
  }
}

void OpenTherm42Hub::set_number_update_every(uint8_t id, uint32_t update_every) {
  RequestKind kind;
  switch (id) {
    case 1:
      kind = RequestKind::CONTROL_SETPOINT;
      break;
    case 8:
      kind = RequestKind::CONTROL_SETPOINT_2;
      break;
    case 71:
      kind = RequestKind::CONTROL_SETPOINT_VENTILATION;
      break;
    default:
      return;
  }
  if (Entry *entry = this->find_entry_(kind); entry != nullptr) {
    entry->update_every = update_every;
  }
}

void OpenTherm42Hub::add_entry_(RequestKind kind, uint32_t update_every) {
  this->entries_.push_back({kind, update_every});
}

Entry *OpenTherm42Hub::find_entry_(RequestKind kind) {
  for (auto &entry : this->entries_) {
    if (entry.kind == kind) {
      return &entry;
    }
  }
  return nullptr;
}

optional<Frame> OpenTherm42Hub::pull_next_due_entry_() {
  if (this->entries_.empty()) {
    return {};  // defensive only -- STATUS/CONTROL_SETPOINT/BOILER_CONFIG-family ids are always
                // seeded by build_schedule_(), so this can't actually happen once setup() has run
  }
  bool wrapped = false;
  while (true) {
    while (this->cursor_ < this->entries_.size()) {
      Entry &entry = this->entries_[this->cursor_];
      this->cursor_++;
      if (this->pass_counter_ % entry.update_every != 0) {
        continue;
      }
      RequestKind const kind = entry.kind;
      return this->build_entry_request_(kind);
    }
    if (wrapped) {
      return {};  // a full lap at the new pass_counter_ still found nothing due -- see this
                  // method's declaration comment for why the caller's STATUS filler self-corrects
                  // over the next few calls rather than needing this method to search further
    }
    wrapped = true;
    this->cursor_ = 0;
    this->pass_counter_++;
    uint32_t const now = millis();
    if (this->pass_duration_sensor_ != nullptr) {
      this->pass_duration_sensor_->publish_state(now - this->pass_start_ms_);
    }
    this->pass_start_ms_ = now;
    if (this->pass_counter_ % this->sweep_length_passes_ == 0) {
      if (this->sweep_duration_sensor_ != nullptr) {
        this->sweep_duration_sensor_->publish_state(now - this->sweep_start_ms_);
      }
      this->sweep_start_ms_ = now;
      if (this->sweep_had_errors_binary_sensor_ != nullptr) {
        this->sweep_had_errors_binary_sensor_->publish_state(this->sweep_had_error_);
      }
      this->sweep_had_error_ = false;
    }
  }
}

void OpenTherm42Hub::handle_response_(const Frame &frame) {
  auto const type = static_cast<MessageType>(frame.type);
  {
    // See log_outgoing_frame_()'s declaration comment in hub.h -- debug instrumentation.
    char kind_desc[80];
    this->describe_request_kind_(this->pending_request_kind_, kind_desc, sizeof(kind_desc));
    ESP_LOGD(TAG, "RX %s: %s id=%u hb=%u lb=%u", kind_desc, message_type_to_string(type), frame.id, frame.value_hb,
             frame.value_lb);
  }
  // See should_invalidate_now_(): every kind's consecutive-DATA_INVALID counter is reset
  // unconditionally on any success, whether or not the dispatched handler below actually wanted
  // this particular ack type.
  if (type == MessageType::READ_ACK || type == MessageType::WRITE_ACK) {
    if (Entry *entry = this->find_entry_(this->pending_request_kind_); entry != nullptr) {
      entry->consecutive_data_invalid = 0;
    }
  }
  if (this->handle_response_status_and_identity_(frame, type) || this->handle_response_feeds_and_time_(frame, type) ||
      this->handle_response_setpoints_and_parameters_(frame, type)) {
    return;
  }
  // Every plain read-only sensor (see the SIMPLE_SENSORS table) shares this one case. A subset of
  // these (the u16 counter/hour ids) also support an on-demand "reset by writing zero" via
  // reset_counter() -- its WRITE_ACK response is distinguished from the periodic READ_ACK purely
  // by message type, the same technique used for id=99's dual read/write handling.
  const SimpleSensorInfo *info = this->find_simple_sensor_(this->pending_request_kind_);
  if (info == nullptr) {
    return;  // startup-only kinds are handled by the dedicated handlers above, unreachable here
  }
  sensor::Sensor *sensor_ptr = this->*(info->member);
  if (type == MessageType::WRITE_ACK) {
    // Response to an on-demand reset_counter() write -- trust whatever value the boiler echoes
    // back (it may ignore or clamp the reset) rather than assuming it is now zero.
    if (sensor_ptr != nullptr) {
      sensor_ptr->publish_state(frame.value_u16());
    }
    return;
  }
  if (type != MessageType::READ_ACK) {
    bool invalidate_now = this->should_invalidate_now_(info->kind, type);
    OT42_LOG_REJECTION(invalidate_now, "%s read was rejected (message type %s)", info->log_name,
                       message_type_to_string(type));
    if (invalidate_now && sensor_ptr != nullptr) {
      invalidate_entity(sensor_ptr);
    }
    return;
  }
  if (sensor_ptr == nullptr) {
    return;
  }
  switch (info->value_kind) {
    case SimpleValueKind::F88:
      sensor_ptr->publish_state(frame.value_f88());
      return;
    case SimpleValueKind::S16:
      sensor_ptr->publish_state(frame.value_s16());
      return;
    case SimpleValueKind::U16:
      sensor_ptr->publish_state(frame.value_u16());
      return;
    case SimpleValueKind::U8_LB:
      sensor_ptr->publish_state(frame.value_lb);
      return;
    case SimpleValueKind::U8_HB:
      sensor_ptr->publish_state(frame.value_hb);
      return;
  }
}

bool OpenTherm42Hub::handle_response_status_and_identity_(const Frame &frame, MessageType type) {
  switch (this->pending_request_kind_) {
    case RequestKind::BOILER_CONFIG:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::BOILER_CONFIG, type);
        OT42_LOG_REJECTION(invalidate_now, "Boiler configuration flags (id=3) read was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::BOILER_CONFIG);
        }
        return true;
      }
      this->boiler_config_flags_ = frame.value_hb;
      this->boiler_member_id_code_ = frame.value_lb;
      if (this->configuration_information_boiler_configuration_dhw_present_text_sensor_ != nullptr) {
        this->configuration_information_boiler_configuration_dhw_present_text_sensor_->publish_state(
            (frame.value_hb & 0x01) ? "DHW is present" : "DHW not present");
      }
      if (this->configuration_information_boiler_configuration_control_type_text_sensor_ != nullptr) {
        this->configuration_information_boiler_configuration_control_type_text_sensor_->publish_state(
            (frame.value_hb & 0x02) ? "On/off" : "Modulating");
      }
      if (this->configuration_information_boiler_configuration_cooling_config_text_sensor_ != nullptr) {
        this->configuration_information_boiler_configuration_cooling_config_text_sensor_->publish_state(
            (frame.value_hb & 0x04) ? "Cooling supported" : "Cooling not supported");
      }
      if (this->configuration_information_boiler_configuration_dhw_config_text_sensor_ != nullptr) {
        this->configuration_information_boiler_configuration_dhw_config_text_sensor_->publish_state(
            (frame.value_hb & 0x08) ? "Storage tank" : "Instantaneous or not-specified");
      }
      if (this->configuration_information_boiler_configuration_master_low_off_and_pump_control_function_text_sensor_ !=
          nullptr) {
        this->configuration_information_boiler_configuration_master_low_off_and_pump_control_function_text_sensor_
            ->publish_state((frame.value_hb & 0x10) ? "Not allowed" : "Allowed");
      }
      if (this->configuration_information_boiler_configuration_ch2_present_text_sensor_ != nullptr) {
        this->configuration_information_boiler_configuration_ch2_present_text_sensor_->publish_state(
            (frame.value_hb & 0x20) ? "CH2 present" : "CH2 not present");
      }
      if (this->configuration_information_boiler_configuration_remote_water_filling_function_text_sensor_ != nullptr) {
        this->configuration_information_boiler_configuration_remote_water_filling_function_text_sensor_->publish_state(
            (frame.value_hb & 0x40) ? "Not available" : "Available or unknown");
      }
      if (this->configuration_information_boiler_configuration_heat_cool_mode_control_text_sensor_ != nullptr) {
        this->configuration_information_boiler_configuration_heat_cool_mode_control_text_sensor_->publish_state(
            (frame.value_hb & 0x80) ? "Switching done by boiler" : "Switching done by master");
      }
      if (this->boiler_member_id_code_sensor_ != nullptr) {
        this->boiler_member_id_code_sensor_->publish_state(frame.value_lb);
      }
      return true;

    case RequestKind::STATUS:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::STATUS, type);
        OT42_LOG_REJECTION(invalidate_now, "Status exchange (id=0) was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::STATUS);
        }
        return true;
      }
      this->boiler_status_ = frame.value_lb;
      this->boiler_status_read_.publish(frame.value_lb);
      return true;

    case RequestKind::CONTROL_SETPOINT:
      // WRITE-ACK's echoed value is not trusted for display -- real hardware has been observed
      // acking a write while echoing 0 (or some other unrelated value) regardless of what was
      // actually accepted (see hub.h's RequestKind::OUTSIDE_TEMPERATURE comment for the same
      // observation on a paired-READ id). Since this id has no READ counterpart to self-correct from,
      // the display simply stays at whatever was last commanded (see OpenTherm42Number::control())
      // or restored at boot, and only an explicit rejection below ever changes it.
      if (type != MessageType::WRITE_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::CONTROL_SETPOINT, type);
        OT42_LOG_REJECTION(invalidate_now, "Control setpoint (id=1) write was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now && this->control_setpoint_number_ != nullptr) {
          invalidate_entity(this->control_setpoint_number_);
        }
        return true;
      }
      // A later successful WRITE-ACK, after a prior rejection invalidated this entity, must bring it
      // back out of Unknown -- republish the last commanded value (never the untrusted echo above).
      if (this->control_setpoint_number_ != nullptr) {
        this->control_setpoint_number_->publish_state(this->control_setpoint_write_value_);
      }
      return true;

    case RequestKind::CONTROL_SETPOINT_2:
      // See CONTROL_SETPOINT above: WRITE-ACK's echo is not trusted for display.
      if (type != MessageType::WRITE_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::CONTROL_SETPOINT_2, type);
        OT42_LOG_REJECTION(invalidate_now, "Control setpoint 2 (id=8) write was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now && this->control_setpoint_2_number_ != nullptr) {
          invalidate_entity(this->control_setpoint_2_number_);
        }
        return true;
      }
      // See CONTROL_SETPOINT above: a later success must recover a previously-invalidated entity.
      if (this->control_setpoint_2_number_ != nullptr) {
        this->control_setpoint_2_number_->publish_state(this->control_setpoint_2_write_value_);
      }
      return true;

    case RequestKind::VENTILATION_STATUS:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::VENTILATION_STATUS, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "Ventilation/heat-recovery status exchange (id=70) was rejected (message type %s)",
                           message_type_to_string(type));
        // Same max_data_invalid grace period as every other id's DATA_INVALID handling (see
        // should_invalidate_now_()'s declaration comment) -- a genuine "this boiler has no
        // ventilation/heat-recovery system" answer is UNKNOWN_DATA_ID, which should_invalidate_now_()
        // already treats as immediate regardless of the mask; only a possibly-transient DATA_INVALID
        // gets the grace period, same as the read side below.
        if (invalidate_now) {
          this->ventilation_status_write_.invalidate();
          this->invalidate_response_(RequestKind::VENTILATION_STATUS);
        }
        return true;
      }
      this->ventilation_status_read_.publish(frame.value_lb);
      return true;

    case RequestKind::CONTROL_SETPOINT_VENTILATION:
      // See CONTROL_SETPOINT above: WRITE-ACK's echo is not trusted for display.
      if (type != MessageType::WRITE_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::CONTROL_SETPOINT_VENTILATION, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "Control setpoint ventilation/heat-recovery (id=71) write was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now && this->control_setpoint_ventilation_number_ != nullptr) {
          invalidate_entity(this->control_setpoint_ventilation_number_);
        }
        return true;
      }
      // See CONTROL_SETPOINT above: a later success must recover a previously-invalidated entity.
      if (this->control_setpoint_ventilation_number_ != nullptr) {
        this->control_setpoint_ventilation_number_->publish_state(this->control_setpoint_ventilation_write_value_);
      }
      return true;

    case RequestKind::FAULT_FLAGS:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::FAULT_FLAGS, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "Application-specific fault flags (id=5) read was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::FAULT_FLAGS);
        }
        return true;
      }
      this->fault_flags_read_.publish(frame.value_hb);
      if (this->oem_fault_code_sensor_ != nullptr) {
        this->oem_fault_code_sensor_->publish_state(frame.value_lb);
      }
      return true;

    case RequestKind::VENTILATION_FAULT_FLAGS:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::VENTILATION_FAULT_FLAGS, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "Application-specific fault flags ventilation/heat-recovery (id=72) read was rejected "
                           "(message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::VENTILATION_FAULT_FLAGS);
        }
        return true;
      }
      this->ventilation_fault_flags_read_.publish(frame.value_hb);
      if (this->oem_fault_code_ventilation_sensor_ != nullptr) {
        this->oem_fault_code_ventilation_sensor_->publish_state(frame.value_lb);
      }
      return true;

    case RequestKind::SOLAR_STORAGE_STATUS:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::SOLAR_STORAGE_STATUS, type);
        OT42_LOG_REJECTION(invalidate_now, "Solar storage status (id=101) read was rejected (message type %s)",
                           message_type_to_string(type));
        // Reaching handle_response_() at all means a valid frame was received -- unlike a
        // transient datalink error, a non-ACK type here is the boiler's definitive answer that it
        // has no Solar Storage feature, so the select can never have any real effect either.
        if (invalidate_now) {
          if (this->master_solar_storage_status_solar_mode_select_ != nullptr) {
            invalidate_entity(this->master_solar_storage_status_solar_mode_select_);
          }
          this->invalidate_response_(RequestKind::SOLAR_STORAGE_STATUS);
        }
        return true;
      }
      // HB bits 2,1,0 and LB bits 3,2,1 both encode "Solar mode" (same 5-value enum, different byte);
      // LB bit 0 is a fault flag and LB bits 5,4 are "Solar status" -- see the spec's ID 101 table.
      // HB's echoed frame content is never trusted for the select's display -- same precedent as
      // STATUS/VENTILATION_STATUS, whose master-status bytes are one-way (local state, resent every
      // turn, never confirmed). But reaching here at all means this conversation succeeded, so a
      // previously-invalidated select must still recover -- republish the last commanded value
      // (never frame.value_hb) so it doesn't stay stuck at Unknown forever after one rejection.
      if (this->master_solar_storage_status_solar_mode_select_ != nullptr) {
        this->master_solar_storage_status_solar_mode_select_->publish_state(
            this->solar_storage_solar_mode_write_value_);
      }
      if (this->solar_storage_fault_indication_binary_sensor_ != nullptr) {
        this->solar_storage_fault_indication_binary_sensor_->publish_state(frame.value_lb & 0x1);
      }
      if (this->solar_storage_mode_and_status_solar_mode_text_sensor_ != nullptr) {
        this->solar_storage_mode_and_status_solar_mode_text_sensor_->publish_state(
            solar_mode_to_string((frame.value_lb >> 1) & 0x7));
      }
      if (this->solar_storage_mode_and_status_solar_status_text_sensor_ != nullptr) {
        this->solar_storage_mode_and_status_solar_status_text_sensor_->publish_state(
            solar_status_to_string((frame.value_lb >> 4) & 0x3));
      }
      return true;

    case RequestKind::SOLAR_STORAGE_FAULT_FLAGS:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::SOLAR_STORAGE_FAULT_FLAGS, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "Solar storage specific fault flags (id=102) read was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::SOLAR_STORAGE_FAULT_FLAGS);
        }
        return true;
      }
      if (this->oem_fault_code_solar_storage_sensor_ != nullptr) {
        this->oem_fault_code_solar_storage_sensor_->publish_state(frame.value_lb);
      }
      return true;

    case RequestKind::OEM_DIAGNOSTIC_CODE:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::OEM_DIAGNOSTIC_CODE, type);
        OT42_LOG_REJECTION(invalidate_now, "OEM diagnostic code (id=115) read was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::OEM_DIAGNOSTIC_CODE);
        }
        return true;
      }
      if (this->oem_diagnostic_code_sensor_ != nullptr) {
        this->oem_diagnostic_code_sensor_->publish_state(frame.value_u16());
      }
      return true;

    case RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "OEM diagnostic code ventilation/heat-recovery (id=73) read was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION);
        }
        return true;
      }
      if (this->oem_diagnostic_code_ventilation_sensor_ != nullptr) {
        this->oem_diagnostic_code_ventilation_sensor_->publish_state(frame.value_u16());
      }
      return true;

    case RequestKind::MASTER_CONFIG:
      // No entity to invalidate -- this is a pure master-authored write with nothing to display.
      // Whatever the outcome, the next due pass just retries it, same as any other id.
      if (type != MessageType::WRITE_ACK) {
        OT42_LOG_REJECTION_ALWAYS("Master configuration (id=2) write was rejected (message type %s)",
                                  message_type_to_string(type));
      }
      return true;

    case RequestKind::MASTER_OPENTHERM_VERSION:
      if (type != MessageType::WRITE_ACK) {
        OT42_LOG_REJECTION_ALWAYS("OpenTherm version Master (id=124) write was rejected (message type %s)",
                                  message_type_to_string(type));
      }
      return true;

    case RequestKind::MASTER_PRODUCT_VERSION:
      if (type != MessageType::WRITE_ACK) {
        OT42_LOG_REJECTION_ALWAYS(
            "Master product version number and type (id=126) write was rejected (message type %s)",
            message_type_to_string(type));
      }
      return true;

    case RequestKind::VENTILATION_CONFIGURATION:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::VENTILATION_CONFIGURATION, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "Configuration ventilation/heat-recovery (id=74) read was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::VENTILATION_CONFIGURATION);
        }
        return true;
      }
      if (this->configuration_information_configuration_ventilation_heat_recovery_system_type_text_sensor_ != nullptr) {
        this->configuration_information_configuration_ventilation_heat_recovery_system_type_text_sensor_->publish_state(
            (frame.value_hb & 0x01) ? "Heat-recovery ventilation" : "Central exhaust ventilation");
      }
      if (this->configuration_information_configuration_ventilation_heat_recovery_bypass_text_sensor_ != nullptr) {
        this->configuration_information_configuration_ventilation_heat_recovery_bypass_text_sensor_->publish_state(
            (frame.value_hb & 0x02) ? "Present" : "Not present");
      }
      if (this->configuration_information_configuration_ventilation_heat_recovery_speed_control_text_sensor_ !=
          nullptr) {
        this->configuration_information_configuration_ventilation_heat_recovery_speed_control_text_sensor_
            ->publish_state((frame.value_hb & 0x04) ? "Variable" : "3-speed");
      }
      if (this->member_id_code_ventilation_sensor_ != nullptr) {
        this->member_id_code_ventilation_sensor_->publish_state(frame.value_lb);
      }
      return true;

    case RequestKind::SOLAR_STORAGE_CONFIGURATION:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::SOLAR_STORAGE_CONFIGURATION, type);
        OT42_LOG_REJECTION(invalidate_now, "Solar Storage configuration (id=103) read was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::SOLAR_STORAGE_CONFIGURATION);
        }
        return true;
      }
      if (this->configuration_information_solar_storage_configuration_system_type_text_sensor_ != nullptr) {
        this->configuration_information_solar_storage_configuration_system_type_text_sensor_->publish_state(
            (frame.value_hb & 0x01) ? "DHW parallel system" : "DHW preheat system");
      }
      if (this->solar_storage_member_id_sensor_ != nullptr) {
        this->solar_storage_member_id_sensor_->publish_state(frame.value_lb);
      }
      return true;

    case RequestKind::PRODUCT_VERSION_BOILER:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::PRODUCT_VERSION_BOILER, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "Boiler product version number and type (id=127) read was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::PRODUCT_VERSION_BOILER);
        }
        return true;
      }
      if (this->boiler_product_type_sensor_ != nullptr) {
        this->boiler_product_type_sensor_->publish_state(frame.value_hb);
      }
      if (this->boiler_product_version_sensor_ != nullptr) {
        this->boiler_product_version_sensor_->publish_state(frame.value_lb);
      }
      return true;

    case RequestKind::PRODUCT_VERSION_VENTILATION:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::PRODUCT_VERSION_VENTILATION, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "Ventilation/heat-recovery product version number and type (id=76) read was rejected "
                           "(message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::PRODUCT_VERSION_VENTILATION);
        }
        return true;
      }
      if (this->ventilation_product_type_sensor_ != nullptr) {
        this->ventilation_product_type_sensor_->publish_state(frame.value_hb);
      }
      if (this->ventilation_product_version_sensor_ != nullptr) {
        this->ventilation_product_version_sensor_->publish_state(frame.value_lb);
      }
      return true;

    case RequestKind::PRODUCT_VERSION_SOLAR_STORAGE:
      if (type != MessageType::READ_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::PRODUCT_VERSION_SOLAR_STORAGE, type);
        OT42_LOG_REJECTION(invalidate_now,
                           "Solar Storage product version number and type (id=104) read was rejected (message type %s)",
                           message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::PRODUCT_VERSION_SOLAR_STORAGE);
        }
        return true;
      }
      if (this->solar_storage_product_type_sensor_ != nullptr) {
        this->solar_storage_product_type_sensor_->publish_state(frame.value_hb);
      }
      if (this->solar_storage_product_version_sensor_ != nullptr) {
        this->solar_storage_product_version_sensor_->publish_state(frame.value_lb);
      }
      return true;

    default:
      return false;
  }
}

bool OpenTherm42Hub::handle_response_feeds_and_time_(const Frame &frame, MessageType type) {
  switch (this->pending_request_kind_) {
    case RequestKind::BRAND:
      this->handle_brand_response_(frame, this->brand_, RequestKind::BRAND, "Brand (id=93)");
      return true;

    case RequestKind::BRAND_VERSION:
      this->handle_brand_response_(frame, this->brand_version_, RequestKind::BRAND_VERSION, "Brand version (id=94)");
      return true;

    case RequestKind::BRAND_SERIAL_NUMBER:
      this->handle_brand_response_(frame, this->brand_serial_number_, RequestKind::BRAND_SERIAL_NUMBER,
                                   "Brand serial number (id=95)");
      return true;

    case RequestKind::REMOTE_REQUEST:
      if (type != MessageType::WRITE_ACK) {
        bool invalidate_now = this->should_invalidate_now_(RequestKind::REMOTE_REQUEST, type);
        OT42_LOG_REJECTION(invalidate_now, "Remote request (id=4, code=%u) was rejected (message type %s)",
                           this->remote_request_code_, message_type_to_string(type));
        if (invalidate_now) {
          this->invalidate_response_(RequestKind::REMOTE_REQUEST);
        }
        return true;
      }
      if (this->remote_request_last_response_code_sensor_ != nullptr) {
        this->remote_request_last_response_code_sensor_->publish_state(frame.value_lb);
      }
      if (this->remote_request_last_response_text_sensor_ != nullptr) {
        // §5.3.3: 0..127 = request refused, 128..255 = request accepted.
        this->remote_request_last_response_text_sensor_->publish_state((frame.value_lb >= 128) ? "Request accepted"
                                                                                               : "Request refused");
      }
      return true;

    default:
      return false;
  }
}

bool OpenTherm42Hub::handle_response_setpoints_and_parameters_(const Frame &frame, MessageType type) {
  switch (this->pending_request_kind_) {
    case RequestKind::TSP: {
      // Deliberately not gated by should_invalidate_now_(): every TSP slot shares this one
      // RequestKind, so a single per-kind last-success timestamp can't tell which specific slot most
      // recently succeeded -- unlike everything else, a rejected TSP read always invalidates right away.
      auto const &slot = this->tsp_slots_[this->pending_tsp_slot_index_];
      if (this->pending_tsp_is_write_) {
        if (type != MessageType::WRITE_ACK) {
          OT42_LOG_REJECTION_ALWAYS("TSP write (id=%u, index=%u) was rejected (message type %s)", slot.data_id,
                                    slot.index, message_type_to_string(type));
          return true;
        }
      } else if (type != MessageType::READ_ACK) {
        OT42_LOG_REJECTION_ALWAYS("TSP read (id=%u, index=%u) was rejected (message type %s)", slot.data_id, slot.index,
                                  message_type_to_string(type));
        if (slot.number != nullptr) {
          invalidate_entity(slot.number);
        }
        return true;
      }
      // §5.3.6: both a READ-ACK and a WRITE-ACK echo the (possibly boiler-clamped) TSP-value in LB --
      // always trust that over whatever was requested.
      if (slot.number != nullptr) {
        slot.number->publish_state(frame.value_lb);
      }
      return true;
    }

    case RequestKind::FHB: {
      // See TSP above: not gated by should_invalidate_now_(), for the same per-slot-vs-per-kind reason.
      auto const &slot = this->fhb_slots_[this->pending_fhb_slot_index_];
      if (type != MessageType::READ_ACK) {
        OT42_LOG_REJECTION_ALWAYS("FHB read (id=%u, index=%u) was rejected (message type %s)", slot.data_id, slot.index,
                                  message_type_to_string(type));
        if (slot.sensor != nullptr) {
          invalidate_entity(slot.sensor);
        }
        return true;
      }
      if (slot.sensor != nullptr) {
        slot.sensor->publish_state(frame.value_lb);
      }
      return true;
    }

    default:
      return false;
  }
}

void OpenTherm42Hub::handle_brand_response_(const Frame &frame, BrandRead &brand, RequestKind kind,
                                            const char *log_name) {
  if (brand.sensor == nullptr) {
    return;  // only scheduled when configured; defensive in case that invariant is ever broken
  }
  auto const type = static_cast<MessageType>(frame.type);
  if (type != MessageType::READ_ACK) {
    OT42_LOG_REJECTION_ALWAYS("%s read was rejected (message type %s)", log_name, message_type_to_string(type));
    invalidate_entity(brand.sensor);
    // Mid-string, this read fails: restart from the first character next time this id comes due,
    // rather than resuming a partial buffer that may no longer match what the boiler now reports.
    brand.next_index = 0;
    return;
  }
  // §5.3.2: the response's HB is the total character count (not an index) -- e.g. HB=0x06 means "6
  // characters can be read" -- and LB is the character at the index this request's HB asked for.
  uint8_t const total_len = std::min<uint8_t>(frame.value_hb, brand.buffer.size() - 1);
  if (brand.next_index < total_len) {
    brand.buffer[brand.next_index] = static_cast<char>(frame.value_lb);
    brand.next_index++;
  }
  if (brand.next_index >= total_len) {
    brand.buffer[brand.next_index] = '\0';
    brand.sensor->publish_state(brand.buffer.data(), brand.next_index);
    // Reset for next time this id comes due (per its own update_every), so a later re-read starts
    // a fresh character-by-character read rather than immediately re-completing at the old index --
    // this is what lets a later firmware update's (possibly different-length) string actually be
    // picked up, since every id is retried forever now (see kind's declaration comment in hub.h).
    brand.next_index = 0;
    return;
  }
  // Mid-string, more characters left: the pass-pull only revisits this id once pass_counter_ next
  // reaches a multiple of its own update_every, which would stall a multi-character read for many
  // passes -- reuse the ASAP dirty bit (checked ahead of the pass-pull on every call, regardless of
  // update_every) to force this id to be picked again on the very next build_next_request_() call
  // instead, continuing the read one character per call until it completes or fails.
  if (Entry *entry = this->find_entry_(kind); entry != nullptr) {
    entry->dirty = true;
  }
}

bool OpenTherm42Hub::should_invalidate_now_(RequestKind kind, MessageType type) {
  if (type != MessageType::DATA_INVALID) {
    return true;
  }
  Entry *entry = this->find_entry_(kind);
  if (entry == nullptr || this->max_data_invalid_ == 0) {
    return true;
  }
  entry->consecutive_data_invalid++;
  return entry->consecutive_data_invalid > this->max_data_invalid_;
}

void OpenTherm42Hub::invalidate_response_(RequestKind kind) {
  switch (kind) {
    case RequestKind::BOILER_CONFIG:
      if (this->configuration_information_boiler_configuration_dhw_present_text_sensor_ != nullptr) {
        invalidate_entity(this->configuration_information_boiler_configuration_dhw_present_text_sensor_);
      }
      if (this->configuration_information_boiler_configuration_control_type_text_sensor_ != nullptr) {
        invalidate_entity(this->configuration_information_boiler_configuration_control_type_text_sensor_);
      }
      if (this->configuration_information_boiler_configuration_cooling_config_text_sensor_ != nullptr) {
        invalidate_entity(this->configuration_information_boiler_configuration_cooling_config_text_sensor_);
      }
      if (this->configuration_information_boiler_configuration_dhw_config_text_sensor_ != nullptr) {
        invalidate_entity(this->configuration_information_boiler_configuration_dhw_config_text_sensor_);
      }
      if (this->configuration_information_boiler_configuration_master_low_off_and_pump_control_function_text_sensor_ !=
          nullptr) {
        invalidate_entity(
            this->configuration_information_boiler_configuration_master_low_off_and_pump_control_function_text_sensor_);
      }
      if (this->configuration_information_boiler_configuration_ch2_present_text_sensor_ != nullptr) {
        invalidate_entity(this->configuration_information_boiler_configuration_ch2_present_text_sensor_);
      }
      if (this->configuration_information_boiler_configuration_remote_water_filling_function_text_sensor_ != nullptr) {
        invalidate_entity(
            this->configuration_information_boiler_configuration_remote_water_filling_function_text_sensor_);
      }
      if (this->configuration_information_boiler_configuration_heat_cool_mode_control_text_sensor_ != nullptr) {
        invalidate_entity(this->configuration_information_boiler_configuration_heat_cool_mode_control_text_sensor_);
      }
      if (this->boiler_member_id_code_sensor_ != nullptr) {
        invalidate_entity(this->boiler_member_id_code_sensor_);
      }
      return;

    case RequestKind::CONTROL_SETPOINT:
      if (this->control_setpoint_number_ != nullptr) {
        invalidate_entity(this->control_setpoint_number_);
      }
      return;

    case RequestKind::CONTROL_SETPOINT_2:
      if (this->control_setpoint_2_number_ != nullptr) {
        invalidate_entity(this->control_setpoint_2_number_);
      }
      return;

    case RequestKind::CONTROL_SETPOINT_VENTILATION:
      if (this->control_setpoint_ventilation_number_ != nullptr) {
        invalidate_entity(this->control_setpoint_ventilation_number_);
      }
      return;

    case RequestKind::MASTER_CONFIG:
    case RequestKind::MASTER_OPENTHERM_VERSION:
    case RequestKind::MASTER_PRODUCT_VERSION:
      return;  // no entity to invalidate -- see their build_entry_request_() cases' comments

    case RequestKind::BRAND:
      if (this->brand_.sensor != nullptr) {
        invalidate_entity(this->brand_.sensor);
      }
      this->brand_.next_index = 0;  // restart from the first character next time this id comes due
      return;

    case RequestKind::BRAND_VERSION:
      if (this->brand_version_.sensor != nullptr) {
        invalidate_entity(this->brand_version_.sensor);
      }
      this->brand_version_.next_index = 0;
      return;

    case RequestKind::BRAND_SERIAL_NUMBER:
      if (this->brand_serial_number_.sensor != nullptr) {
        invalidate_entity(this->brand_serial_number_.sensor);
      }
      this->brand_serial_number_.next_index = 0;
      return;

    case RequestKind::STATUS:
      // Unlike a definitive rejection (not legal for this mandatory id per §5.2.1, so never reached
      // here), a raw datalink error means we don't know whether the boiler ever saw this turn's
      // master-status byte at all -- show these switches as unknown too, rather than keep displaying
      // a commanded state we can no longer vouch for.
      this->master_status_write_.invalidate();
      this->boiler_status_read_.invalidate();
      return;

    case RequestKind::VENTILATION_STATUS:
      // Same reasoning as STATUS above: a raw datalink error means we can't tell whether the boiler
      // received this turn's write, so the switches go unknown here too, not just on a definitive
      // rejection (see handle_response_()).
      this->ventilation_status_write_.invalidate();
      this->ventilation_status_read_.invalidate();
      return;

    case RequestKind::FAULT_FLAGS:
      this->fault_flags_read_.invalidate();
      if (this->oem_fault_code_sensor_ != nullptr) {
        invalidate_entity(this->oem_fault_code_sensor_);
      }
      return;

    case RequestKind::VENTILATION_FAULT_FLAGS:
      this->ventilation_fault_flags_read_.invalidate();
      if (this->oem_fault_code_ventilation_sensor_ != nullptr) {
        invalidate_entity(this->oem_fault_code_ventilation_sensor_);
      }
      return;

    case RequestKind::SOLAR_STORAGE_STATUS:
      // Same reasoning as STATUS/VENTILATION_STATUS above: a raw datalink error means we can't tell
      // whether the boiler received this turn's HB write, so the select goes unknown here too, not
      // just on a definitive rejection (see handle_response_()).
      if (this->master_solar_storage_status_solar_mode_select_ != nullptr) {
        invalidate_entity(this->master_solar_storage_status_solar_mode_select_);
      }
      if (this->solar_storage_fault_indication_binary_sensor_ != nullptr) {
        invalidate_entity(this->solar_storage_fault_indication_binary_sensor_);
      }
      if (this->solar_storage_mode_and_status_solar_mode_text_sensor_ != nullptr) {
        invalidate_entity(this->solar_storage_mode_and_status_solar_mode_text_sensor_);
      }
      if (this->solar_storage_mode_and_status_solar_status_text_sensor_ != nullptr) {
        invalidate_entity(this->solar_storage_mode_and_status_solar_status_text_sensor_);
      }
      return;

    case RequestKind::SOLAR_STORAGE_FAULT_FLAGS:
      if (this->oem_fault_code_solar_storage_sensor_ != nullptr) {
        invalidate_entity(this->oem_fault_code_solar_storage_sensor_);
      }
      return;

    case RequestKind::OEM_DIAGNOSTIC_CODE:
      if (this->oem_diagnostic_code_sensor_ != nullptr) {
        invalidate_entity(this->oem_diagnostic_code_sensor_);
      }
      return;

    case RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION:
      if (this->oem_diagnostic_code_ventilation_sensor_ != nullptr) {
        invalidate_entity(this->oem_diagnostic_code_ventilation_sensor_);
      }
      return;

    case RequestKind::VENTILATION_CONFIGURATION:
      if (this->configuration_information_configuration_ventilation_heat_recovery_system_type_text_sensor_ != nullptr) {
        invalidate_entity(
            this->configuration_information_configuration_ventilation_heat_recovery_system_type_text_sensor_);
      }
      if (this->configuration_information_configuration_ventilation_heat_recovery_bypass_text_sensor_ != nullptr) {
        invalidate_entity(this->configuration_information_configuration_ventilation_heat_recovery_bypass_text_sensor_);
      }
      if (this->configuration_information_configuration_ventilation_heat_recovery_speed_control_text_sensor_ !=
          nullptr) {
        invalidate_entity(
            this->configuration_information_configuration_ventilation_heat_recovery_speed_control_text_sensor_);
      }
      if (this->member_id_code_ventilation_sensor_ != nullptr) {
        invalidate_entity(this->member_id_code_ventilation_sensor_);
      }
      return;

    case RequestKind::SOLAR_STORAGE_CONFIGURATION:
      if (this->configuration_information_solar_storage_configuration_system_type_text_sensor_ != nullptr) {
        invalidate_entity(this->configuration_information_solar_storage_configuration_system_type_text_sensor_);
      }
      if (this->solar_storage_member_id_sensor_ != nullptr) {
        invalidate_entity(this->solar_storage_member_id_sensor_);
      }
      return;

    case RequestKind::PRODUCT_VERSION_BOILER:
      if (this->boiler_product_type_sensor_ != nullptr) {
        invalidate_entity(this->boiler_product_type_sensor_);
      }
      if (this->boiler_product_version_sensor_ != nullptr) {
        invalidate_entity(this->boiler_product_version_sensor_);
      }
      return;

    case RequestKind::PRODUCT_VERSION_VENTILATION:
      if (this->ventilation_product_type_sensor_ != nullptr) {
        invalidate_entity(this->ventilation_product_type_sensor_);
      }
      if (this->ventilation_product_version_sensor_ != nullptr) {
        invalidate_entity(this->ventilation_product_version_sensor_);
      }
      return;

    case RequestKind::PRODUCT_VERSION_SOLAR_STORAGE:
      if (this->solar_storage_product_type_sensor_ != nullptr) {
        invalidate_entity(this->solar_storage_product_type_sensor_);
      }
      if (this->solar_storage_product_version_sensor_ != nullptr) {
        invalidate_entity(this->solar_storage_product_version_sensor_);
      }
      return;

    case RequestKind::REMOTE_REQUEST:
      if (this->remote_request_last_response_code_sensor_ != nullptr) {
        invalidate_entity(this->remote_request_last_response_code_sensor_);
      }
      if (this->remote_request_last_response_text_sensor_ != nullptr) {
        invalidate_entity(this->remote_request_last_response_text_sensor_);
      }
      return;

    case RequestKind::TSP:
      if (this->pending_tsp_slot_index_ < this->tsp_slots_.size()) {
        number::Number *tsp_number = this->tsp_slots_[this->pending_tsp_slot_index_].number;
        if (tsp_number != nullptr) {
          invalidate_entity(tsp_number);
        }
      }
      return;

    case RequestKind::FHB:
      if (this->pending_fhb_slot_index_ < this->fhb_slots_.size()) {
        sensor::Sensor *fhb_sensor = this->fhb_slots_[this->pending_fhb_slot_index_].sensor;
        if (fhb_sensor != nullptr) {
          invalidate_entity(fhb_sensor);
        }
      }
      return;

    default: {
      const SimpleSensorInfo *info = this->find_simple_sensor_(kind);
      if (info != nullptr) {
        sensor::Sensor *sensor_ptr = this->*(info->member);
        if (sensor_ptr != nullptr) {
          invalidate_entity(sensor_ptr);
        }
      }
      return;
    }
  }
}

// The literal-name half of describe_request_kind_() below -- every kind not covered by
// find_simple_sensor_() or one of the three kinds needing a runtime-formatted value (TSP, FHB,
// REMOTE_REQUEST). Names match the text each case already logs on a definitive rejection in
// handle_response_(), so the two ways this component reports "this conversation failed" agree.
static const char *bespoke_request_kind_name(RequestKind kind) {
  switch (kind) {
    case RequestKind::BOILER_CONFIG:
      return "Boiler configuration flags (id=3)";
    case RequestKind::STATUS:
      return "Status exchange (id=0)";
    case RequestKind::CONTROL_SETPOINT:
      return "Control setpoint (id=1)";
    case RequestKind::CONTROL_SETPOINT_2:
      return "Control setpoint 2 (id=8)";
    case RequestKind::VENTILATION_STATUS:
      return "Ventilation/heat-recovery status exchange (id=70)";
    case RequestKind::CONTROL_SETPOINT_VENTILATION:
      return "Control setpoint ventilation/heat-recovery (id=71)";
    case RequestKind::FAULT_FLAGS:
      return "Application-specific fault flags (id=5)";
    case RequestKind::VENTILATION_FAULT_FLAGS:
      return "Application-specific fault flags ventilation/heat-recovery (id=72)";
    case RequestKind::SOLAR_STORAGE_STATUS:
      return "Solar storage status (id=101)";
    case RequestKind::SOLAR_STORAGE_FAULT_FLAGS:
      return "Solar storage specific fault flags (id=102)";
    case RequestKind::OEM_DIAGNOSTIC_CODE:
      return "OEM diagnostic code (id=115)";
    case RequestKind::OEM_DIAGNOSTIC_CODE_VENTILATION:
      return "OEM diagnostic code ventilation/heat-recovery (id=73)";
    case RequestKind::MASTER_CONFIG:
      return "Master configuration (id=2)";
    case RequestKind::MASTER_OPENTHERM_VERSION:
      return "OpenTherm version Master (id=124)";
    case RequestKind::MASTER_PRODUCT_VERSION:
      return "Master product version number and type (id=126)";
    case RequestKind::VENTILATION_CONFIGURATION:
      return "Configuration ventilation/heat-recovery (id=74)";
    case RequestKind::SOLAR_STORAGE_CONFIGURATION:
      return "Solar Storage configuration (id=103)";
    case RequestKind::PRODUCT_VERSION_BOILER:
      return "Boiler product version number and type (id=127)";
    case RequestKind::PRODUCT_VERSION_VENTILATION:
      return "Ventilation/heat-recovery product version number and type (id=76)";
    case RequestKind::PRODUCT_VERSION_SOLAR_STORAGE:
      return "Solar Storage product version number and type (id=104)";
    case RequestKind::BRAND:
      return "Brand (id=93)";
    case RequestKind::BRAND_VERSION:
      return "Brand version (id=94)";
    case RequestKind::BRAND_SERIAL_NUMBER:
      return "Brand serial number (id=95)";
    default:
      return nullptr;
  }
}

void OpenTherm42Hub::describe_request_kind_(RequestKind kind, char *buf, size_t buf_len) const {
  if (const SimpleSensorInfo *info = this->find_simple_sensor_(kind)) {
    snprintf(buf, buf_len, "%s", info->log_name);
    return;
  }
  switch (kind) {
    case RequestKind::TSP: {
      auto const &slot = this->tsp_slots_[this->pending_tsp_slot_index_];
      snprintf(buf, buf_len, "TSP %s (id=%u, index=%u)", this->pending_tsp_is_write_ ? "write" : "read", slot.data_id,
               slot.index);
      return;
    }
    case RequestKind::FHB: {
      auto const &slot = this->fhb_slots_[this->pending_fhb_slot_index_];
      snprintf(buf, buf_len, "FHB read (id=%u, index=%u)", slot.data_id, slot.index);
      return;
    }
    case RequestKind::REMOTE_REQUEST:
      snprintf(buf, buf_len, "Remote request (id=4, code=%u)", this->remote_request_code_);
      return;
    default:
      break;
  }
  if (const char *name = bespoke_request_kind_name(kind)) {
    snprintf(buf, buf_len, "%s", name);
  } else {
    snprintf(buf, buf_len, "kind=%u", static_cast<unsigned>(kind));
  }
}

void OpenTherm42Hub::log_outgoing_frame_(const Frame &frame) const {
  char kind_desc[80];
  this->describe_request_kind_(this->pending_request_kind_, kind_desc, sizeof(kind_desc));
  ESP_LOGD(TAG, "TX %s: %s id=%u hb=%u lb=%u", kind_desc, message_type_to_string(static_cast<MessageType>(frame.type)),
           frame.id, frame.value_hb, frame.value_lb);
}

void OpenTherm42Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm 4.2:");
  LOG_PIN("  In pin: ", this->in_pin_);
  LOG_PIN("  Out pin: ", this->out_pin_);
}

}  // namespace esphome::opentherm42
