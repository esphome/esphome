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
};
// clang-format on

// §5.3.1 Class 1, ID 101 LB bits 3,2,1 (Solar Storage mode and status: Solar mode) -- same 5-state
// enum as the select platform's ID 101 HB (Master Solar Storage status), but this is the boiler's
// own independently-reported value, not a readback of HB.

// §5.3.1 Class 1, ID 101 LB bits 5,4: Solar Storage mode and status: Solar status.

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
    this->pass_start_ms_ = now;
    if (this->pass_counter_ % this->sweep_length_passes_ == 0) {
      this->sweep_start_ms_ = now;
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

    default:
      return false;
  }
}

bool OpenTherm42Hub::handle_response_feeds_and_time_(const Frame &frame, MessageType type) {
  switch (this->pending_request_kind_) {
    default:
      return false;
  }
}

bool OpenTherm42Hub::handle_response_setpoints_and_parameters_(const Frame &frame, MessageType type) {
  switch (this->pending_request_kind_) {
    default:
      return false;
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
    case RequestKind::CONTROL_SETPOINT:
      if (this->control_setpoint_number_ != nullptr) {
        invalidate_entity(this->control_setpoint_number_);
      }
      return;

    case RequestKind::STATUS:
      // Unlike a definitive rejection (not legal for this mandatory id per §5.2.1, so never reached
      // here), a raw datalink error means we don't know whether the boiler ever saw this turn's
      // master-status byte at all -- show these switches as unknown too, rather than keep displaying
      // a commanded state we can no longer vouch for.
      this->master_status_write_.invalidate();
      this->boiler_status_read_.invalidate();
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
    case RequestKind::STATUS:
      return "Status exchange (id=0)";
    case RequestKind::CONTROL_SETPOINT:
      return "Control setpoint (id=1)";
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
