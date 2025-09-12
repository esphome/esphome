#include "zwave_proxy.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zwave_proxy {

static const char *TAG = "zwave_proxy";

ZWaveProxy::ZWaveProxy() { global_zwave_proxy = this; }

void ZWaveProxy::loop() {
  if (this->response_handler_()) {
    // return;  // If a response was handled, exit early to avoid a CAN
  }
  if (this->api_connection_ != nullptr && !this->api_connection_->is_connection_setup()) {
    ESP_LOGW(TAG, "Subscriber disconnected");
    this->api_connection_ = nullptr;  // Unsubscribe if disconnected
  }

  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      this->status_set_warning("Failed reading from UART");
      return;
    }
    if (this->parse_byte_(byte)) {
      ESP_LOGD(TAG, "Sending to client: %s", YESNO(this->api_connection_ != nullptr));
      if (this->api_connection_ != nullptr) {
        // minimize copying to reduce CPU overhead
        this->outgoing_proto_msg_.data_len = this->buffer_[0] == ZWAVE_FRAME_TYPE_START ? this->buffer_[1] + 2 : 1;
        std::memcpy(this->outgoing_proto_msg_.data, this->buffer_, this->outgoing_proto_msg_.data_len);
        this->api_connection_->send_message(this->outgoing_proto_msg_, api::ZWaveProxyFrame::MESSAGE_TYPE);
      }
    }
  }
  this->status_clear_warning();
}

void ZWaveProxy::dump_config() { ESP_LOGCONFIG(TAG, "Z-Wave Proxy"); }

void ZWaveProxy::subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags) {
  if (this->api_connection_ != nullptr) {
    ESP_LOGE(TAG, "Only one API subscription is allowed at a time");
    return;
  }
  this->api_connection_ = api_connection;
}

void ZWaveProxy::unsubscribe_api_connection(api::APIConnection *api_connection) {
  if (this->api_connection_ != api_connection) {
    ESP_LOGV(TAG, "API connection is not subscribed");
    return;
  }
  this->api_connection_ = nullptr;
}

void ZWaveProxy::send_frame(const uint8_t *data, size_t length) {
  if (!length) {
    if (data[0] == ZWAVE_FRAME_TYPE_START) {
      length = data[1] + 2;  // data[1] is payload length, not including SoF + checksum
    } else {
      length = 1;  // assume ACK/NAK/CAN
    }
  }
  if (length == 1 && data[0] == this->last_response_) {
    ESP_LOGW(TAG, "Skipping sending duplicate response: 0x%02X", data[0]);
    return;
  }
  ESP_LOGD(TAG, "Sending: %s", format_hex_pretty(data, length).c_str());
  this->write_array(data, length);
}

bool ZWaveProxy::parse_byte_(uint8_t byte) {
  bool frame_completed = false;
  // Basic parsing logic for received frames
  switch (this->parsing_state_) {
    case ZWAVE_PARSING_STATE_WAIT_START:
      this->parse_start_(byte);
      break;
    case ZWAVE_PARSING_STATE_WAIT_LENGTH:
      if (!byte) {
        ESP_LOGW(TAG, "Invalid LENGTH: %u", byte);
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_NAK;
        return false;
      }
      ESP_LOGD(TAG, "Received LENGTH: %u", byte);
      this->end_frame_after_ = this->buffer_index_ + byte;
      ESP_LOGVV(TAG, "Calculated EOF: %u", this->end_frame_after_);
      this->buffer_[this->buffer_index_++] = byte;
      this->checksum_ ^= byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_TYPE;
      break;
    case ZWAVE_PARSING_STATE_WAIT_TYPE:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGD(TAG, "Received TYPE: 0x%02X", byte);
      this->checksum_ ^= byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_COMMAND_ID;
      break;
    case ZWAVE_PARSING_STATE_WAIT_COMMAND_ID:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGD(TAG, "Received COMMAND ID: 0x%02X", byte);
      this->checksum_ ^= byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_PAYLOAD;
      break;
    case ZWAVE_PARSING_STATE_WAIT_PAYLOAD:
      this->buffer_[this->buffer_index_++] = byte;
      this->checksum_ ^= byte;
      ESP_LOGVV(TAG, "Received PAYLOAD: 0x%02X", byte);
      if (this->buffer_index_ >= this->end_frame_after_) {
        this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_CHECKSUM;
      }
      break;
    case ZWAVE_PARSING_STATE_WAIT_CHECKSUM:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGD(TAG, "Received CHECKSUM: 0x%02X", byte);
      ESP_LOGV(TAG, "Calculated CHECKSUM: 0x%02X", this->checksum_);
      if (this->checksum_ != byte) {
        ESP_LOGW(TAG, "Bad checksum: expected 0x%02X, got 0x%02X", this->checksum_, byte);
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_NAK;
      } else {
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_ACK;
        ESP_LOGD(TAG, "Received frame: %s", format_hex_pretty(this->buffer_, this->buffer_index_).c_str());
        frame_completed = true;
      }
      this->response_handler_();
      break;
    case ZWAVE_PARSING_STATE_SEND_ACK:
    case ZWAVE_PARSING_STATE_SEND_NAK:
      break;  // Should not happen, handled in loop()
    default:
      ESP_LOGD(TAG, "Received unknown byte: 0x%02X", byte);
      break;
  }
  return frame_completed;
}

void ZWaveProxy::parse_start_(uint8_t byte) {
  this->buffer_index_ = 0;
  this->checksum_ = 0xFF;
  this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
  switch (byte) {
    case ZWAVE_FRAME_TYPE_START:
      ESP_LOGD(TAG, "Received START");
      this->buffer_[this->buffer_index_++] = byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_LENGTH;
      return;
    case ZWAVE_FRAME_TYPE_ACK:
      ESP_LOGD(TAG, "Received ACK");
      break;
    case ZWAVE_FRAME_TYPE_NAK:
      ESP_LOGW(TAG, "Received NAK");
      break;
    case ZWAVE_FRAME_TYPE_CAN:
      ESP_LOGW(TAG, "Received CAN");
      break;
    default:
      ESP_LOGW(TAG, "Unexpected type: 0x%02X", byte);
      return;
  }
  // Forward response (ACK/NAK/CAN) back to client for processing
  if (this->api_connection_ != nullptr) {
    this->outgoing_proto_msg_.data[0] = byte;
    this->outgoing_proto_msg_.data_len = 1;
    this->api_connection_->send_message(this->outgoing_proto_msg_, api::ZWaveProxyFrame::MESSAGE_TYPE);
  }
}

bool ZWaveProxy::response_handler_() {
  switch (this->parsing_state_) {
    case ZWAVE_PARSING_STATE_SEND_ACK:
      this->last_response_ = ZWAVE_FRAME_TYPE_ACK;
      break;
    case ZWAVE_PARSING_STATE_SEND_CAN:
      this->last_response_ = ZWAVE_FRAME_TYPE_CAN;
      break;
    case ZWAVE_PARSING_STATE_SEND_NAK:
      this->last_response_ = ZWAVE_FRAME_TYPE_NAK;
      break;
    default:
      return false;  // No response handled
  }

  ESP_LOGD(TAG, "Sending %s (0x%02X)", this->last_response_ == ZWAVE_FRAME_TYPE_ACK ? "ACK" : "NAK/CAN",
           this->last_response_);
  this->write_byte(this->last_response_);
  this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
  return true;
}

ZWaveProxy *global_zwave_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace zwave_proxy
}  // namespace esphome
