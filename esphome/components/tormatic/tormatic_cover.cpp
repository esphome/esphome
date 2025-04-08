#include <vector>

#include "tormatic_cover.h"

using namespace std;

namespace esphome {
namespace tormatic {

static const char *const TAG = "tormatic.cover";

using namespace esphome::cover;

void Tormatic::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
    return;
  }

  // Assume gate is closed without preexisting state.
  this->position = 0.0f;
}

cover::CoverTraits Tormatic::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_is_assumed_state(false);
  return traits;
}

void Tormatic::dump_config() {
  LOG_COVER("", "Tormatic Cover", this);
  this->check_uart_settings(9600, 1, uart::UART_CONFIG_PARITY_NONE, 8);

  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    ESP_LOGCONFIG(TAG, "  Saved position %d%%", (int) (restore->position * 100.f));
  }
}

void Tormatic::update() { this->request_gate_status_(); }

void Tormatic::loop() {
  auto o_status = this->read_gate_status_();
  if (o_status) {
    auto status = o_status.value();

    this->recalibrate_duration_(status);
    this->handle_gate_status_(status);
  }

  this->recompute_position_();
  this->stop_at_target_();
}

void Tormatic::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->send_gate_command_(PAUSED);
    return;
  }

  if (call.get_position().has_value()) {
    auto pos = call.get_position().value();
    this->control_position_(pos);
    return;
  }
}

// Wrap the Cover's publish_state with a rate limiter. Publishes if the last
// publish was longer than ratelimit milliseconds ago. 0 to disable.
void Tormatic::publish_state(bool save, uint32_t ratelimit) {
  auto now = millis();
  if ((now - this->last_publish_time_) < ratelimit) {
    return;
  }
  this->last_publish_time_ = now;

  Cover::publish_state(save);
};

// Recalibrate the gate's estimated open or close duration based on the
// actual time the operation took.
void Tormatic::recalibrate_duration_(GateStatus s) {
  if (this->current_status_ == s) {
    return;
  }

  auto now = millis();
  auto old = this->current_status_;

  // Gate paused halfway through opening or closing, invalidate the start time
  // of the current operation. Close/open durations can only be accurately
  // calibrated on full open or close cycle due to motor acceleration.
  if (s == PAUSED) {
    ESP_LOGD(TAG, "Gate paused, clearing direction start time");
    this->direction_start_time_ = 0;
    return;
  }

  // Record the start time of a state transition if the gate was in the fully
  // open or closed position before the command.
  if ((old == CLOSED && s == OPENING) || (old == OPENED && s == CLOSING)) {
    ESP_LOGD(TAG, "Gate started moving from fully open or closed state");
    this->direction_start_time_ = now;
    return;
  }

  // The gate was resumed from a paused state, don't attempt recalibration.
  if (this->direction_start_time_ == 0) {
    return;
  }

  if (s == OPENED) {
    this->open_duration_ = now - this->direction_start_time_;
    ESP_LOGI(TAG, "Recalibrated the gate's open duration to %dms", this->open_duration_);
  }
  if (s == CLOSED) {
    this->close_duration_ = now - this->direction_start_time_;
    ESP_LOGI(TAG, "Recalibrated the gate's close duration to %dms", this->close_duration_);
  }

  this->direction_start_time_ = 0;
}

// Set the Cover's internal state based on a status message
// received from the unit.
void Tormatic::handle_gate_status_(GateStatus s) {
  if (this->current_status_ == s) {
    return;
  }

  ESP_LOGI(TAG, "Status changed from %s to %s", gate_status_to_str(this->current_status_), gate_status_to_str(s));

  switch (s) {
    case OPENED:
      // The Novoferm 423 doesn't respond to the first 'Close' command after
      // being opened completely. Sending a pause command after opening fixes
      // that.
      this->send_gate_command_(PAUSED);

      this->position = COVER_OPEN;
      break;
    case CLOSED:
      this->position = COVER_CLOSED;
      break;
    default:
      break;
  }

  this->current_status_ = s;
  this->current_operation = gate_status_to_cover_operation(s);

  this->publish_state(true);

  // This timestamp is used to generate position deltas on every loop() while
  // the gate is moving. Bump it on each state transition so the first tick
  // doesn't generate a huge delta.
  this->last_recompute_time_ = millis();
}

// Recompute the gate's position and publish the results while
// the gate is moving. No-op when the gate is idle.
void Tormatic::recompute_position_() {
  if (this->current_operation == COVER_OPERATION_IDLE) {
    return;
  }

  const uint32_t now = millis();
  uint32_t diff = now - this->last_recompute_time_;

  auto direction = +1.0f;
  uint32_t duration = this->open_duration_;
  if (this->current_operation == COVER_OPERATION_CLOSING) {
    direction = -1.0f;
    duration = this->close_duration_;
  }

  auto delta = direction * diff / duration;

  this->position = clamp(this->position + delta, COVER_CLOSED, COVER_OPEN);

  this->last_recompute_time_ = now;

  this->publish_state(true, 250);
}

// Start moving the gate in the direction of the target position.
void Tormatic::control_position_(float target) {
  if (target == this->position) {
    return;
  }

  if (target == COVER_OPEN) {
    ESP_LOGI(TAG, "Fully opening gate");
    this->send_gate_command_(OPENED);
    return;
  }
  if (target == COVER_CLOSED) {
    ESP_LOGI(TAG, "Fully closing gate");
    this->send_gate_command_(CLOSED);
    return;
  }

  // Don't set target position when fully opening or closing the gate, the gate
  // stops automatically when it reaches the configured open/closed positions.
  this->target_position_ = target;

  if (target > this->position) {
    ESP_LOGI(TAG, "Opening gate towards %.1f", target);
    this->send_gate_command_(OPENED);
    return;
  }

  if (target < this->position) {
    ESP_LOGI(TAG, "Closing gate towards %.1f", target);
    this->send_gate_command_(CLOSED);
    return;
  }
}

// Stop the gate if it is moving at or beyond its target position. Target
// position is only set when the gate is requested to move to a halfway
// position.
void Tormatic::stop_at_target_() {
  if (this->current_operation == COVER_OPERATION_IDLE) {
    return;
  }
  if (!this->target_position_) {
    return;
  }
  auto target = this->target_position_.value();

  if (this->current_operation == COVER_OPERATION_OPENING && this->position < target) {
    return;
  }
  if (this->current_operation == COVER_OPERATION_CLOSING && this->position > target) {
    return;
  }

  this->send_gate_command_(PAUSED);
  this->target_position_.reset();
}

// Read a GateStatus from the unit. The unit only sends messages in response to
// status requests or commands, so a message needs to be sent first.
optional<GateStatus> Tormatic::read_gate_status_() {
  if (this->available() < sizeof(MessageHeader)) {
    return {};
  }

  auto o_hdr = this->read_data_<MessageHeader>();
  if (!o_hdr) {
    ESP_LOGE(TAG, "Timeout reading message header");
    return {};
  }
  auto hdr = o_hdr.value();

  switch (hdr.type) {
    case STATUS: {
      if (hdr.payload_size() != sizeof(StatusReply)) {
        ESP_LOGE(TAG, "Header specifies payload size %d but size of StatusReply is %d", hdr.payload_size(),
                 sizeof(StatusReply));
      }

      // Read a StatusReply requested by update().
      auto o_status = this->read_data_<StatusReply>();
      if (!o_status) {
        return {};
      }
      auto status = o_status.value();

      return status.state;
    }

    case COMMAND:
      // Commands initiated by control() are simply echoed back by the unit, but
      // don't guarantee that the unit's internal state has been transitioned,
      // nor that the motor started moving. A subsequent status request may
      // still return the previous state. Discard these messages, don't use them
      // to drive the Cover state machine.
      break;

    default:
      // Unknown message type, drain the remaining amount of bytes specified in
      // the header.
      ESP_LOGE(TAG, "Reading remaining %d payload bytes of unknown type 0x%x", hdr.payload_size(), hdr.type);
      break;
  }

  // Drain any unhandled payload bytes described by the message header, if any.
  this->drain_rx_(hdr.payload_size());

  return {};
}

// Send a message to the unit requesting the gate's status.
void Tormatic::request_gate_status_() {
  ESP_LOGV(TAG, "Requesting gate status");
  StatusRequest req(GATE);
  this->send_message_(STATUS, req);
}

// Send a message to the unit issuing a command.
void Tormatic::send_gate_command_(GateStatus s) {
  ESP_LOGI(TAG, "Sending gate command %s", gate_status_to_str(s));
  CommandRequestReply req(s);
  this->send_message_(COMMAND, req);
}

template<typename T> void Tormatic::send_message_(MessageType t, T req) {
  MessageHeader hdr(t, ++this->seq_tx_, sizeof(req));

  auto out = serialize(hdr);
  auto reqv = serialize(req);
  out.insert(out.end(), reqv.begin(), reqv.end());

  this->write_array(out);
}

template<typename T> optional<T> Tormatic::read_data_() {
  T obj;
  uint32_t start = millis();

  auto ok = this->read_array((uint8_t *) &obj, sizeof(obj));
  if (!ok) {
    // Couldn't read object successfully, timeout?
    return {};
  }
  obj.byteswap();

  ESP_LOGV(TAG, "Read %s in %d ms", obj.print().c_str(), millis() - start);
  return obj;
}

// Drain up to n amount of bytes from the uart rx buffer.
void Tormatic::drain_rx_(uint16_t n) {
  uint8_t data;
  uint16_t count = 0;
  while (this->available()) {
    this->read_byte(&data);
    count++;

    if (n > 0 && count >= n) {
      return;
    }
  }
}

}  // namespace tormatic
}  // namespace esphome
