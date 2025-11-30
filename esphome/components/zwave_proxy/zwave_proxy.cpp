#include "zwave_proxy.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

namespace esphome::zwave_proxy {

static const char *const TAG = "zwave_proxy";

static constexpr uint8_t ZWAVE_COMMAND_GET_NETWORK_IDS = 0x20;
// GET_NETWORK_IDS response: [SOF][LENGTH][TYPE][CMD][HOME_ID(4)][NODE_ID][...]
static constexpr uint8_t ZWAVE_COMMAND_TYPE_RESPONSE = 0x01;    // Response type field value
static constexpr uint8_t ZWAVE_MIN_GET_NETWORK_IDS_LENGTH = 9;  // TYPE + CMD + HOME_ID(4) + NODE_ID + checksum
static constexpr uint32_t HOME_ID_TIMEOUT_MS = 100;             // Timeout for waiting for home ID during setup

static uint8_t calculate_frame_checksum(const uint8_t *data, uint8_t length) {
  // Calculate Z-Wave frame checksum
  // XOR all bytes between SOF and checksum position (exclusive)
  // Initial value is 0xFF per Z-Wave protocol specification
  uint8_t checksum = 0xFF;
  for (uint8_t i = 1; i < length - 1; i++) {
    checksum ^= data[i];
  }
  return checksum;
}

ZWaveProxy::ZWaveProxy() { global_zwave_proxy = this; }

void ZWaveProxy::setup() {
  this->setup_time_ = App.get_loop_component_start_time();
  this->send_simple_command_(ZWAVE_COMMAND_GET_NETWORK_IDS);
}

float ZWaveProxy::get_setup_priority() const {
  // Set up before API so home ID is ready when API starts
  return setup_priority::BEFORE_CONNECTION;
}

bool ZWaveProxy::can_proceed() {
  // If we already have the home ID, we can proceed
  if (this->home_id_ready_) {
    return true;
  }

  // Handle any pending responses
  if (this->response_handler_()) {
    ESP_LOGV(TAG, "Handled response during setup");
  }

  // Process UART data to check for home ID
  this->process_uart_();

  // Check if we got the home ID after processing
  if (this->home_id_ready_) {
    return true;
  }

  // Wait up to HOME_ID_TIMEOUT_MS for home ID response
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->setup_time_ > HOME_ID_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Timeout reading Home ID during setup");
    return true;  // Proceed anyway after timeout
  }

  return false;  // Keep waiting
}

void ZWaveProxy::loop() {
  if (this->response_handler_()) {
    ESP_LOGV(TAG, "Handled late response");
  }
  if (this->api_connection_ != nullptr && (!this->api_connection_->is_connection_setup() || !api_is_connected())) {
    ESP_LOGW(TAG, "Subscriber disconnected");
    this->api_connection_ = nullptr;  // Unsubscribe if disconnected
  }

  this->process_uart_();
  this->status_clear_warning();
}

void ZWaveProxy::process_uart_() {
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      this->status_set_warning("UART read failed");
      return;
    }
    if (this->parse_byte_(byte)) {
      // Check if this is a GET_NETWORK_IDS response frame
      // Frame format: [SOF][LENGTH][TYPE][CMD][HOME_ID(4)][NODE_ID][...]
      // We verify:
      // - buffer_[0]: Start of frame marker (0x01)
      // - buffer_[1]: Length field must be >= 9 to contain all required data
      // - buffer_[2]: Command type (0x01 for response)
      // - buffer_[3]: Command ID (0x20 for GET_NETWORK_IDS)
      if (this->buffer_[3] == ZWAVE_COMMAND_GET_NETWORK_IDS && this->buffer_[2] == ZWAVE_COMMAND_TYPE_RESPONSE &&
          this->buffer_[1] >= ZWAVE_MIN_GET_NETWORK_IDS_LENGTH && this->buffer_[0] == ZWAVE_FRAME_TYPE_START) {
        // Store the 4-byte Home ID, which starts at offset 4, and notify connected clients if it changed
        // The frame parser has already validated the checksum and ensured all bytes are present
        if (this->set_home_id(&this->buffer_[4])) {
          this->send_homeid_changed_msg_();
        }
      }
      ESP_LOGV(TAG, "Sending to client: %s", YESNO(this->api_connection_ != nullptr));
      if (this->api_connection_ != nullptr) {
        // Zero-copy: point directly to our buffer
        this->outgoing_proto_msg_.data = this->buffer_.data();
        if (this->in_bootloader_) {
          this->outgoing_proto_msg_.data_len = this->buffer_index_;
        } else {
          // If this is a data frame, use frame length indicator + 2 (for SoF + checksum), else assume 1 for ACK/NAK/CAN
          this->outgoing_proto_msg_.data_len = this->buffer_[0] == ZWAVE_FRAME_TYPE_START ? this->buffer_[1] + 2 : 1;
        }
        this->api_connection_->send_message(this->outgoing_proto_msg_, api::ZWaveProxyFrame::MESSAGE_TYPE);
      }
    }
  }
}

void ZWaveProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Z-Wave Proxy:\n"
                "  Home ID: %s",
                format_hex_pretty(this->home_id_.data(), this->home_id_.size(), ':', false).c_str());
}

void ZWaveProxy::api_connection_authenticated(api::APIConnection *conn) {
  if (this->home_id_ready_) {
    // If a client just authenticated & HomeID is ready, send the current HomeID
    this->send_homeid_changed_msg_(conn);
  }
}

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

bool ZWaveProxy::set_home_id(const uint8_t *new_home_id) {
  if (std::memcmp(this->home_id_.data(), new_home_id, this->home_id_.size()) == 0) {
    ESP_LOGV(TAG, "Home ID unchanged");
    return false;  // No change
  }
  std::memcpy(this->home_id_.data(), new_home_id, this->home_id_.size());
  ESP_LOGI(TAG, "Home ID: %s", format_hex_pretty(this->home_id_.data(), this->home_id_.size(), ':', false).c_str());
  this->home_id_ready_ = true;
  return true;  // Home ID was changed
}

void ZWaveProxy::send_frame(const uint8_t *data, size_t length) {
  if (length == 1 && data[0] == this->last_response_) {
    ESP_LOGV(TAG, "Skipping sending duplicate response: 0x%02X", data[0]);
    return;
  }
  ESP_LOGVV(TAG, "Sending: %s", format_hex_pretty(data, length).c_str());
  this->write_array(data, length);
}

void ZWaveProxy::send_homeid_changed_msg_(api::APIConnection *conn) {
  api::ZWaveProxyRequest msg;
  msg.type = api::enums::ZWAVE_PROXY_REQUEST_TYPE_HOME_ID_CHANGE;
  msg.data = this->home_id_.data();
  msg.data_len = this->home_id_.size();
  if (conn != nullptr) {
    // Send to specific connection
    conn->send_message(msg, api::ZWaveProxyRequest::MESSAGE_TYPE);
  } else if (api::global_api_server != nullptr) {
    // We could add code to manage a second subscription type, but, since this message is
    //  very infrequent and small, we simply send it to all clients
    api::global_api_server->on_zwave_proxy_request(msg);
  }
}

void ZWaveProxy::send_simple_command_(const uint8_t command_id) {
  // Send a simple Z-Wave command with no parameters
  // Frame format: [SOF][LENGTH][TYPE][CMD][CHECKSUM]
  // Where LENGTH=0x03 (3 bytes: TYPE + CMD + CHECKSUM)
  uint8_t cmd[] = {0x01, 0x03, 0x00, command_id, 0x00};
  cmd[4] = calculate_frame_checksum(cmd, sizeof(cmd));
  this->send_frame(cmd, sizeof(cmd));
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
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_TYPE;
      break;
    case ZWAVE_PARSING_STATE_WAIT_TYPE:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGVV(TAG, "Received TYPE: 0x%02X", byte);
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_COMMAND_ID;
      break;
    case ZWAVE_PARSING_STATE_WAIT_COMMAND_ID:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGVV(TAG, "Received COMMAND ID: 0x%02X", byte);
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_PAYLOAD;
      break;
    case ZWAVE_PARSING_STATE_WAIT_PAYLOAD:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGVV(TAG, "Received PAYLOAD: 0x%02X", byte);
      if (this->buffer_index_ >= this->end_frame_after_) {
        this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_CHECKSUM;
      }
      break;
    case ZWAVE_PARSING_STATE_WAIT_CHECKSUM: {
      this->buffer_[this->buffer_index_++] = byte;
      auto checksum = calculate_frame_checksum(this->buffer_.data(), this->buffer_index_);
      ESP_LOGVV(TAG, "CHECKSUM Received: 0x%02X - Calculated: 0x%02X", byte, checksum);
      if (checksum != byte) {
        ESP_LOGW(TAG, "Bad checksum: expected 0x%02X, got 0x%02X", checksum, byte);
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_NAK;
      } else {
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_ACK;
        ESP_LOGVV(TAG, "Received frame: %s", format_hex_pretty(this->buffer_.data(), this->buffer_index_).c_str());
        frame_completed = true;
      }
      this->response_handler_();
      break;
    }
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
    // Store single byte in buffer and point to it
    this->buffer_[0] = byte;
    this->outgoing_proto_msg_.data = this->buffer_.data();
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

}  // namespace esphome::zwave_proxy
