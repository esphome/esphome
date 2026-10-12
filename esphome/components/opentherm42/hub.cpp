#include "hub.h"
#include "esphome/core/helpers.h"

namespace esphome::opentherm42 {

ESPHOME_LOG_TAG(TAG, "opentherm42");

void OpenTherm42Hub::setup() {
  this->datalink_ = make_unique<OpenThermDataLink>(this->in_pin_, this->out_pin_);
  if (!this->datalink_->initialize()) {
    const char *reason = rmt_error_to_string(this->datalink_->get_rmt_error());
    ESP_LOGE(TAG, "Failed to initialize the OpenTherm datalink (%s); see previous log messages for details", reason);
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
      // §5.2: the Status exchange (id=0) is the one conversation every master must run. All master
      // status bits are left clear (CH and DHW disabled) until they get entities of their own.
      Frame request{};
      request.type = static_cast<uint8_t>(MessageType::READ_DATA);
      request.id = 0;
      ESP_LOGD(TAG, "TX: %s id=%u hb=%u lb=%u", message_type_to_string(static_cast<MessageType>(request.type)),
               request.id, request.value_hb, request.value_lb);
      this->datalink_->send(request);
      return;
    }
    case DataLinkState::SENT:
      this->datalink_->listen(RESPONSE_TIMEOUT_MS);
      return;
    case DataLinkState::RECEIVED: {
      Frame const response = this->datalink_->get_frame();
      ESP_LOGD(TAG, "RX: %s id=%u hb=%u lb=%u", message_type_to_string(static_cast<MessageType>(response.type)),
               response.id, response.value_hb, response.value_lb);
      this->last_conversation_end_ms_ = millis();
      this->datalink_->stop();
      return;
    }
    case DataLinkState::ERROR:
      ESP_LOGE(TAG, "Conversation failed: %s", data_link_error_to_string(this->datalink_->get_error()));
      this->last_conversation_end_ms_ = millis();
      this->datalink_->stop();
      return;
    default:
      return;  // SENDING/LISTENING/RECEIVING: bit-level progress driven by the datalink's hardware.
  }
}

void OpenTherm42Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm 4.2:");
  LOG_PIN("  In pin: ", this->in_pin_);
  LOG_PIN("  Out pin: ", this->out_pin_);
}

}  // namespace esphome::opentherm42
