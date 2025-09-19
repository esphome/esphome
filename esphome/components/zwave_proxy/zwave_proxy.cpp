#include "zwave_proxy.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

namespace esphome {
namespace zwave_proxy {

static const char *const TAG = "zwave_proxy";

ZWaveProxy::ZWaveProxy() { global_zwave_proxy = this; }

void ZWaveProxy::loop() {
  if (this->response_handler_()) {
    ESP_LOGV(TAG, "Handled late response");
  }
  if (this->api_connection_ != nullptr && (!this->api_connection_->is_connection_setup() || !api_is_connected())) {
    ESP_LOGW(TAG, "Subscriber disconnected");
    this->api_connection_ = nullptr;  // Unsubscribe if disconnected
  }

  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      this->status_set_warning("UART read failed");
      return;
    }
    if (this->parse_byte_(byte)) {
      ESP_LOGV(TAG, "Sending to client: %s", YESNO(this->api_connection_ != nullptr));
      if (this->api_connection_ != nullptr) {
        // minimize copying to reduce CPU overhead
        if (this->in_bootloader_) {
          this->outgoing_proto_msg_.data_len = this->buffer_index_;
        } else {
          // If this is a data frame, use frame length indicator + 2 (for SoF + checksum), else assume 1 for ACK/NAK/CAN
          this->outgoing_proto_msg_.data_len = this->buffer_[0] == ZWAVE_FRAME_TYPE_START ? this->buffer_[1] + 2 : 1;
        }
        std::memcpy(this->outgoing_proto_msg_.data, this->buffer_, this->outgoing_proto_msg_.data_len);
        this->api_connection_->send_message(this->outgoing_proto_msg_, api::ZWaveProxyFrame::MESSAGE_TYPE);
      }
    }
  }
  this->status_clear_warning();
}

void ZWaveProxy::dump_config() { ESP_LOGCONFIG(TAG, "Z-Wave Proxy"); }

void ZWaveProxy::zwave_proxy_request(api::APIConnection *api_connection, api::enums::ZWaveProxyRequestType type) {
  switch (type) {
    case api::enums::ZWAVE_PROXY_REQUEST_TYPE_SUBSCRIBE:
      if (this->api_connection_ != nullptr) {
        ESP_LOGE(TAG, "Only one API subscription is allowed at a time");
        return;
      }
      this->api_connection_ = api_connection;
      ESP_LOGV(TAG, "API connection is now subscribed");
      break;
    case api::enums::ZWAVE_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      if (this->api_connection_ != api_connection) {
        ESP_LOGV(TAG, "API connection is not subscribed");
        return;
      }
      this->api_connection_ = nullptr;
      break;
    default:
      ESP_LOGW(TAG, "Unknown request type: %d", type);
      break;
  }
}

void ZWaveProxy::send_frame(const uint8_t *data, size_t length) {
  if (length == 1 && data[0] == this->last_response_) {
    ESP_LOGV(TAG, "Skipping sending duplicate response: 0x%02X", data[0]);
    return;
  }
  ESP_LOGVV(TAG, "Sending: %s", format_hex_pretty(data, length).c_str());
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
      ESP_LOGVV(TAG, "Received LENGTH: %u", byte);
      this->end_frame_after_ = this->buffer_index_ + byte;
      ESP_LOGVV(TAG, "Calculated EOF: %u", this->end_frame_after_);
      this->buffer_[this->buffer_index_++] = byte;
      this->checksum_ ^= byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_TYPE;
      break;
    case ZWAVE_PARSING_STATE_WAIT_TYPE:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGVV(TAG, "Received TYPE: 0x%02X", byte);
      this->checksum_ ^= byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_COMMAND_ID;
      break;
    case ZWAVE_PARSING_STATE_WAIT_COMMAND_ID:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGVV(TAG, "Received COMMAND ID: 0x%02X", byte);
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
      ESP_LOGVV(TAG, "Received CHECKSUM: 0x%02X", byte);
      ESP_LOGV(TAG, "Calculated CHECKSUM: 0x%02X", this->checksum_);
      if (this->checksum_ != byte) {
        ESP_LOGW(TAG, "Bad checksum: expected 0x%02X, got 0x%02X", this->checksum_, byte);
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_NAK;
      } else {
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_ACK;
        ESP_LOGVV(TAG, "Received frame: %s", format_hex_pretty(this->buffer_, this->buffer_index_).c_str());
        frame_completed = true;
      }
      this->response_handler_();
      break;
    case ZWAVE_PARSING_STATE_READ_BL_MENU:
      this->buffer_[this->buffer_index_++] = byte;
      if (!byte) {
        this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
        frame_completed = true;
      }
      break;
    case ZWAVE_PARSING_STATE_SEND_ACK:
    case ZWAVE_PARSING_STATE_SEND_NAK:
      break;  // Should not happen, handled in loop()
    default:
      ESP_LOGW(TAG, "Bad parsing state; resetting");
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
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
      ESP_LOGVV(TAG, "Received START");
      if (this->in_bootloader_) {
        ESP_LOGD(TAG, "Exited bootloader mode");
        this->in_bootloader_ = false;
      }
      this->buffer_[this->buffer_index_++] = byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_LENGTH;
      return;
    case ZWAVE_FRAME_TYPE_BL_MENU:
      ESP_LOGVV(TAG, "Received BL_MENU");
      if (!this->in_bootloader_) {
        ESP_LOGD(TAG, "Entered bootloader mode");
        this->in_bootloader_ = true;
      }
      this->buffer_[this->buffer_index_++] = byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_READ_BL_MENU;
      return;
    case ZWAVE_FRAME_TYPE_BL_BEGIN_UPLOAD:
      ESP_LOGVV(TAG, "Received BL_BEGIN_UPLOAD");
      break;
    case ZWAVE_FRAME_TYPE_ACK:
      ESP_LOGVV(TAG, "Received ACK");
      break;
    case ZWAVE_FRAME_TYPE_NAK:
      ESP_LOGW(TAG, "Received NAK");
      break;
    case ZWAVE_FRAME_TYPE_CAN:
      ESP_LOGW(TAG, "Received CAN");
      break;
    default:
      ESP_LOGW(TAG, "Unrecognized START: 0x%02X", byte);
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

  ESP_LOGVV(TAG, "Sending %s (0x%02X)", this->last_response_ == ZWAVE_FRAME_TYPE_ACK ? "ACK" : "NAK/CAN",
            this->last_response_);
  this->write_byte(this->last_response_);
  this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
  return true;
}

ZWaveProxy *global_zwave_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace zwave_proxy
}  // namespace esphome
