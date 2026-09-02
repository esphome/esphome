#include "hub.h"
#include "esphome/core/helpers.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42";

void OpenTherm42Hub::setup() {
  this->datalink_ = make_unique<OpenThermDataLink>(this->in_pin_, this->out_pin_);
  if (!this->datalink_->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize the OpenTherm datalink (%s); see previous log messages for details",
             timer_error_to_string(this->datalink_->get_timer_error()));
    this->mark_failed();
    return;
  }
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
    case DataLinkState::ERROR:
      ESP_LOGW(TAG, "Conversation failed: %s", data_link_error_to_string(this->datalink_->get_error()));
      this->last_conversation_end_ms_ = millis();
      this->datalink_->stop();
      return;
    default:
      return;  // SENDING/LISTENING/RECEIVING: bit-level progress driven by the datalink's timer ISR.
  }
}

Frame OpenTherm42Hub::build_next_request_() {
  Frame frame{};
  if (!this->boiler_config_read_) {
    this->pending_request_kind_ = RequestKind::BOILER_CONFIG;
    frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
    frame.id = 3;
    return frame;
  }
  if (this->next_is_status_) {
    this->pending_request_kind_ = RequestKind::STATUS;
    frame.type = static_cast<uint8_t>(MessageType::READ_DATA);
    frame.id = 0;  // HB (master status) left at 0 until Class 1 (Commit 4) supplies real bits.
  } else {
    this->pending_request_kind_ = RequestKind::CONTROL_SETPOINT;
    frame.type = static_cast<uint8_t>(MessageType::WRITE_DATA);
    frame.id = 1;
    frame.set_value_f88(0.0f);  // no demand until Class 1 (Commit 4) supplies a real setpoint.
  }
  this->next_is_status_ = !this->next_is_status_;
  return frame;
}

void OpenTherm42Hub::handle_response_(const Frame &frame) {
  // §5.2.1: ID 0 and ID 3 are mandatory for the boiler to support, so a compliant boiler must never
  // answer them with DATA_INVALID or UNKNOWN_DATA_ID -- if it does, something is wrong with the
  // boiler and the (default-valued) reply must not be trusted.
  auto const type = static_cast<MessageType>(frame.type);
  switch (this->pending_request_kind_) {
    case RequestKind::BOILER_CONFIG:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Boiler configuration flags (id=3) read was rejected (message type %u)", frame.type);
        return;
      }
      this->boiler_config_flags_ = frame.value_hb;
      this->boiler_member_id_code_ = frame.value_lb;
      this->boiler_config_read_ = true;
      return;
    case RequestKind::STATUS:
      if (type != MessageType::READ_ACK) {
        ESP_LOGW(TAG, "Status exchange (id=0) was rejected (message type %u)", frame.type);
        return;
      }
      this->boiler_status_ = frame.value_lb;
      return;
    case RequestKind::CONTROL_SETPOINT:
      if (type != MessageType::WRITE_ACK) {
        ESP_LOGW(TAG, "Control setpoint (id=1) write was rejected (message type %u)", frame.type);
      }
      return;
  }
}

void OpenTherm42Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm 4.2:");
  LOG_PIN("  In pin: ", this->in_pin_);
  LOG_PIN("  Out pin: ", this->out_pin_);
}

}  // namespace esphome::opentherm42
