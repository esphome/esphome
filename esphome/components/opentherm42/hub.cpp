#include "hub.h"
#include <algorithm>
#include <cstdio>
#include "esphome/core/controller_registry.h"
#include "esphome/core/helpers.h"

namespace esphome::opentherm42 {

ESPHOME_LOG_TAG(TAG, "opentherm42");

// Logs a rejected conversation at a severity matching what's about to happen to its entity: ERROR if
// this rejection is actually invalidating it now, WARN if should_invalidate_now_() decided to mask it
// (a DATA_INVALID within max_data_invalid's grace period -- see its declaration comment) so the
// severity reflects that nothing user-visible happened. Expands to the normal ESP_LOGE/ESP_LOGW
// macros, so compile-time log-level stripping still applies to whichever branch is actually reachable.
// Only marks the current sweep as having had an error (see sweep_had_error_'s declaration comment) in
// the ERROR/invalidating case -- a DATA_INVALID masked by max_data_invalid's grace period is, by
// definition, tolerated, so it must not flip this any more than it flips the entity itself.
#define OT42_LOG_REJECTION(invalidate_now, ...) \
  do { \
    if (invalidate_now) { \
      ESP_LOGE(TAG, __VA_ARGS__); \
      this->sweep_had_error_ = true; \
    } else { \
      ESP_LOGW(TAG, __VA_ARGS__); \
    } \
  } while (0)

// Same as OT42_LOG_REJECTION, for kinds with no should_invalidate_now_() grace period (every
// rejection invalidates immediately).
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
static void invalidate_entity(number::Number *entity) {
  if (entity == nullptr) {
    return;
  }
  entity->set_has_state(false);
#ifdef USE_NUMBER
  ControllerRegistry::notify_number_update(entity);
#endif
}

void OpenTherm42Hub::setup() {
  this->datalink_ = make_unique<OpenThermDataLink>(this->in_pin_, this->out_pin_);
  if (!this->datalink_->initialize()) {
    auto const error = this->datalink_->get_error();
    const char *reason = error == DataLinkError::RMT_ERROR ? rmt_error_to_string(this->datalink_->get_rmt_error())
                                                           : timer_error_to_string(this->datalink_->get_timer_error());
    ESP_LOGE(TAG, "Failed to initialize the OpenTherm datalink (%s); see previous log messages for details", reason);
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
      Frame const request = this->build_next_request_();
      this->pending_request_id_ = request.id;
      this->datalink_->send(request);
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
      return;  // SENDING/LISTENING/RECEIVING: bit-level progress driven by the datalink's hardware.
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
    default:
      break;
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
  if (frame.id != this->pending_request_id_) {
    // Parity-valid but for another data-id, e.g. a late or mis-decoded frame: publishing it would show
    // another reading's value as this one's.
    OT42_LOG_REJECTION_ALWAYS("Response id=%u does not match the request id=%u", frame.id, this->pending_request_id_);
    this->invalidate_response_(this->pending_request_kind_);
    return;
  }
  // See should_invalidate_now_(): every kind's consecutive-DATA_INVALID counter is reset
  // unconditionally on any success, whether or not the dispatched handler below actually wanted
  // this particular ack type.
  if (type == MessageType::READ_ACK || type == MessageType::WRITE_ACK) {
    if (Entry *entry = this->find_entry_(this->pending_request_kind_); entry != nullptr) {
      entry->consecutive_data_invalid = 0;
    }
  }
  this->handle_response_status_and_identity_(frame, type);
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

bool OpenTherm42Hub::should_invalidate_now_(RequestKind kind, MessageType type) {
  if (type != MessageType::DATA_INVALID) {
    return true;
  }
  Entry *entry = this->find_entry_(kind);
  if (entry == nullptr || this->max_data_invalid_ == 0) {
    return true;
  }
  entry->consecutive_data_invalid++;
  if (entry->consecutive_data_invalid <= this->max_data_invalid_) {
    return false;
  }
  // Logged only on the exact call that exhausts the grace period (not every subsequent
  // still-invalid poll, which would just repeat this every pass) -- see set_max_data_invalid()'s
  // declaration comment for what this limit is tolerating.
  if (entry->consecutive_data_invalid == this->max_data_invalid_ + 1) {
    char kind_desc[80];
    this->describe_request_kind_(kind, kind_desc, sizeof(kind_desc));
    ESP_LOGE(TAG, "%s: %u consecutive DATA_INVALID responses reached max_data_invalid (%u); marking Unknown", kind_desc,
             entry->consecutive_data_invalid, static_cast<unsigned>(this->max_data_invalid_));
  }
  return true;
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

    default:
      return;
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
#ifdef OPENTHERM42_DATALINK_RMT
  ESP_LOGCONFIG(TAG, "  Datalink backend: RMT");
#else
  ESP_LOGCONFIG(TAG, "  Datalink backend: ISR");
#endif
}

}  // namespace esphome::opentherm42
