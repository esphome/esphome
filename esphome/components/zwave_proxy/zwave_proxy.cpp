#include "zwave_proxy.h"

#ifdef USE_API

#include "esphome/components/api/api_server.h"

#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

namespace esphome::zwave_proxy {

static const char *const TAG = "zwave_proxy";

// Maximum bytes to log in very verbose hex output (168 * 3 = 504, under TX buffer size of 512)
static constexpr size_t ZWAVE_MAX_LOG_BYTES = 168;

static constexpr uint8_t ZWAVE_COMMAND_GET_NETWORK_IDS = 0x20;
// GET_NETWORK_IDS response: [SOF][LENGTH][TYPE][CMD][HOME_ID(4)][NODE_ID(1 or 2)][...]
// We only read the home ID, so the node ID (1 byte in 8-bit mode, 2 bytes in 16-bit mode) and
// anything after it are not required to be present
static constexpr uint8_t ZWAVE_COMMAND_TYPE_RESPONSE = 0x01;    // Response type field value
static constexpr uint8_t ZWAVE_MIN_GET_NETWORK_IDS_LENGTH = 7;  // TYPE + CMD + HOME_ID(4) + checksum
static constexpr uint8_t ZWAVE_MIN_FRAME_LENGTH = 3;            // TYPE + CMD + checksum (zero-payload frame)
static constexpr uint32_t ZWAVE_FRAME_TIMEOUT_MS = 1500;        // Abandon a frame this long after its start (SOF) byte
static constexpr uint32_t HOME_ID_TIMEOUT_MS = 100;             // Timeout for waiting for home ID during setup
static constexpr uint32_t RECONNECT_DELAY_MS = 500;             // Delay between home ID query attempts after reconnect
static constexpr uint8_t MAX_QUERY_RETRIES = 5;                 // Max attempts to query home ID after reconnect

static constexpr bool is_bootloader_menu_byte(uint8_t byte) {
  // Bootloader menu output is printable ASCII plus CR/LF, ending with a NUL terminator
  return byte == 0 || byte == '\r' || byte == '\n' || (byte >= 0x20 && byte <= 0x7E);
}

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
  this->was_connected_ = this->parent_->is_connected();
  if (this->was_connected_) {
    this->send_simple_command_(ZWAVE_COMMAND_GET_NETWORK_IDS);
  }
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
    // The modem may simply still be booting; keep querying from loop() using the same retry
    // machinery as a reconnect. This adds no setup delay — clients are notified of the home ID
    // via the HOME_ID_CHANGE message whenever it finally arrives.
    this->reconnect_time_ = now;
    this->query_retries_ = 0;
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

  const bool connected = this->parent_->is_connected();
  if (this->was_connected_ != connected) {
    this->on_connection_changed_(connected);
  }
  if (this->reconnect_time_ != 0) {
    this->retry_home_id_query_();
  }

  this->process_uart_();

  // Abandon a stalled frame reception. The Z-Wave API specification requires a receiver to abort
  // a data frame reception lasting more than 1500 ms after the SOF byte, without sending a NAK.
  // Without this, the stale bytes would silently corrupt the next frame. Any SEND_* state was
  // already resolved by response_handler_() above, so a state other than WAIT_START here always
  // means we are mid-frame.
  if (this->parsing_state_ != ZWAVE_PARSING_STATE_WAIT_START &&
      App.get_loop_component_start_time() - this->frame_start_time_ > ZWAVE_FRAME_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Timeout waiting for frame data; resetting parser");
    this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
    this->buffer_index_ = 0;
  }
}

void ZWaveProxy::process_uart_slow_() {
  // Caller (inline process_uart_) has already confirmed available() > 0, so use do/while to
  // drain bytes — available() is still checked at the tail, but not redundantly on entry.
  do {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      this->status_set_warning(LOG_STR("UART read failed"));
      return;
    }
    if (this->parse_byte_(byte)) {
      // Check if this is a GET_NETWORK_IDS response frame
      // Frame format: [SOF][LENGTH][TYPE][CMD][HOME_ID(4)][NODE_ID(1 or 2)][...]
      // Bootloader output is excluded up front: a completed bootloader "frame" is menu text, so
      // buffer_[1..3] would be meaningless (and possibly never written). Outside bootloader mode,
      // the parser guarantees a completed frame starts with SOF, so buffer_[0] needs no check.
      // We verify:
      // - buffer_[1]: Length field must be >= 7 so the frame contains the full home ID
      // - buffer_[2]: Command type (0x01 for response)
      // - buffer_[3]: Command ID (0x20 for GET_NETWORK_IDS)
      if (!this->in_bootloader_ && this->buffer_[1] >= ZWAVE_MIN_GET_NETWORK_IDS_LENGTH &&
          this->buffer_[2] == ZWAVE_COMMAND_TYPE_RESPONSE && this->buffer_[3] == ZWAVE_COMMAND_GET_NETWORK_IDS) {
        // Store the 4-byte Home ID, which starts at offset 4, and notify connected clients if it changed
        // The frame parser has already validated the checksum and ensured all bytes are present
        if (this->set_home_id_(&this->buffer_[4])) {
          char hex_buf[format_hex_pretty_size(ZWAVE_HOME_ID_SIZE)];
          ESP_LOGI(TAG, "Home ID: %s", format_hex_pretty_to(hex_buf, this->home_id_.data(), this->home_id_.size()));
          this->send_homeid_changed_msg_();
        }
        this->home_id_ready_ = true;
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
        this->api_connection_->send_message(this->outgoing_proto_msg_);
      }
    }
  } while (this->available());
  // Reaching here means every read succeeded, so clear any earlier read-failure warning.
  // (An early return on read failure skips this, leaving the warning visible until the
  // next successful drain.)
  this->status_clear_warning();
}

void ZWaveProxy::dump_config() {
  char hex_buf[format_hex_pretty_size(ZWAVE_HOME_ID_SIZE)];
  ESP_LOGCONFIG(
      TAG,
      "Z-Wave Proxy:\n"
      "  Home ID: %s",
      this->home_id_ready_ ? format_hex_pretty_to(hex_buf, this->home_id_.data(), this->home_id_.size()) : "unknown");
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
      if (this->api_connection_ == api_connection) {
        ESP_LOGV(TAG, "API connection is already subscribed");
        return;
      }
      if (this->api_connection_ != nullptr) {
        // A living subscriber keeps exclusive access. Its connection may be dead without
        // loop() having noticed yet (e.g. the client crashed and reconnected quickly);
        // in that case let the new client take over instead of locking it out.
        if (this->api_connection_->is_connection_setup()) {
          ESP_LOGE(TAG, "Only one API subscription is allowed at a time");
          return;
        }
        ESP_LOGW(TAG, "Previous subscriber disconnected; taking over subscription");
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
      ESP_LOGW(TAG, "Unknown request type: %" PRIu32, static_cast<uint32_t>(type));
      break;
  }
}

void ZWaveProxy::on_connection_changed_(bool connected) {
  this->was_connected_ = connected;
  if (connected) {
    ESP_LOGD(TAG, "Modem reconnected");
    this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
    this->buffer_index_ = 0;
    this->last_response_ = 0;
    this->in_bootloader_ = false;
    // Defer the query — the modem needs time to initialize after power is applied
    this->reconnect_time_ = App.get_loop_component_start_time();
    this->query_retries_ = 0;
  } else {
    ESP_LOGW(TAG, "Modem disconnected");
    this->clear_home_id_();
  }
}

void ZWaveProxy::retry_home_id_query_() {
  if (this->home_id_ready_) {
    // Got the home ID, cancel remaining retries
    this->reconnect_time_ = 0;
    return;
  }
  if (App.get_loop_component_start_time() - this->reconnect_time_ <= RECONNECT_DELAY_MS) {
    return;  // Not yet time for next attempt
  }
  this->reconnect_time_ = App.get_loop_component_start_time();  // Reset timer for next retry
  this->query_retries_++;
  if (this->query_retries_ <= MAX_QUERY_RETRIES) {
    ESP_LOGD(TAG, "Querying Home ID (attempt %u)", this->query_retries_);
    this->send_simple_command_(ZWAVE_COMMAND_GET_NETWORK_IDS);
  } else {
    ESP_LOGW(TAG, "Failed to read Home ID after %u attempts", MAX_QUERY_RETRIES);
    this->reconnect_time_ = 0;
  }
}

void ZWaveProxy::clear_home_id_() {
  static constexpr uint8_t ZERO_HOME_ID[ZWAVE_HOME_ID_SIZE] = {};
  if (this->set_home_id_(ZERO_HOME_ID)) {
    ESP_LOGV(TAG, "Home ID cleared");
    this->send_homeid_changed_msg_();
  }
  this->home_id_ready_ = false;
  this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
  this->buffer_index_ = 0;
  this->last_response_ = 0;
  this->in_bootloader_ = false;
}

bool ZWaveProxy::set_home_id_(const uint8_t *new_home_id) {
  if (std::memcmp(this->home_id_.data(), new_home_id, this->home_id_.size()) == 0) {
    ESP_LOGV(TAG, "Home ID unchanged");
    return false;  // No change
  }
  std::memcpy(this->home_id_.data(), new_home_id, this->home_id_.size());
  return true;  // Home ID was changed
}

void ZWaveProxy::send_frame(api::APIConnection *api_connection, const uint8_t *data, size_t length) {
  // Only the subscribed client may talk to the Z-Wave module; a frame from any other
  // (authenticated but unsubscribed) client would interleave with the subscriber's traffic
  if (api_connection != this->api_connection_) {
    ESP_LOGW(TAG, "Ignoring frame from unsubscribed client");
    return;
  }
  this->send_frame_(data, length);
}

void ZWaveProxy::send_frame_(const uint8_t *data, size_t length) {
  // Safety: validate pointer before any access
  if (data == nullptr) {
    ESP_LOGE(TAG, "Null data pointer");
    return;
  }
  if (length == 0) {
    ESP_LOGE(TAG, "Length 0");
    return;
  }

  // Skip duplicate single-byte responses (ACK/NAK/CAN)
  if (length == 1 && data[0] == this->last_response_) {
    ESP_LOGV(TAG, "Response already sent: 0x%02X", data[0]);
    return;
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char hex_buf[format_hex_pretty_size(ZWAVE_MAX_LOG_BYTES)];
#endif
  ESP_LOGVV(TAG, "Sending: %s", format_hex_pretty_to(hex_buf, data, length));

  this->write_array(data, length);
}

void ZWaveProxy::send_homeid_changed_msg_(api::APIConnection *conn) {
  api::ZWaveProxyRequest msg;
  msg.type = api::enums::ZWAVE_PROXY_REQUEST_TYPE_HOME_ID_CHANGE;
  msg.data = this->home_id_.data();
  msg.data_len = this->home_id_.size();
  if (conn != nullptr) {
    // Send to specific connection
    conn->send_message(msg);
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
  this->send_frame_(cmd, sizeof(cmd));
}

bool ZWaveProxy::parse_byte_(uint8_t byte) {
  bool frame_completed = false;
  // Basic parsing logic for received frames
  switch (this->parsing_state_) {
    case ZWAVE_PARSING_STATE_WAIT_START:
      this->parse_start_(byte);
      break;
    case ZWAVE_PARSING_STATE_WAIT_LENGTH:
      if (byte < ZWAVE_MIN_FRAME_LENGTH) {
        ESP_LOGW(TAG, "Invalid LENGTH: %u", byte);
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_NAK;
        // Send the NAK now; otherwise any bytes already buffered behind this one would be
        // silently discarded by the SEND_NAK case below until the next loop() iteration
        this->response_handler_();
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
      // A zero-payload frame (LENGTH == 3) has its checksum immediately after the command ID
      this->parsing_state_ = this->buffer_index_ >= this->end_frame_after_ ? ZWAVE_PARSING_STATE_WAIT_CHECKSUM
                                                                           : ZWAVE_PARSING_STATE_WAIT_PAYLOAD;
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
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
        char hex_buf[format_hex_pretty_size(ZWAVE_MAX_LOG_BYTES)];
#endif
        ESP_LOGVV(TAG, "Received frame: %s", format_hex_pretty_to(hex_buf, this->buffer_.data(), this->buffer_index_));
        frame_completed = true;
      }
      this->response_handler_();
      break;
    }
    case ZWAVE_PARSING_STATE_READ_BL_MENU:
      // This state is tentative (see parse_start_): bootloader mode is committed only when a
      // plausible menu — printable text ending in a NUL terminator — completes. A byte that
      // cannot be menu text means the 0x0D that started this state was not a menu after all,
      // so re-parse that byte as a frame start; it may be the SOF/ACK/NAK of real traffic.
      if (this->buffer_index_ >= this->buffer_.size() || !is_bootloader_menu_byte(byte)) {
        this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
        this->parse_start_(byte);
        break;
      }
      this->buffer_[this->buffer_index_++] = byte;
      if (!byte) {
        if (!this->in_bootloader_) {
          ESP_LOGD(TAG, "Entered bootloader mode");
          this->in_bootloader_ = true;
          // Reset response deduplication: in bootloader mode, single-byte client writes (XMODEM
          // ACK/NAK/CAN) are raw data and must never be suppressed as duplicate responses
          this->last_response_ = 0;
        }
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
      ESP_LOGV(TAG, "Received START");
      if (this->in_bootloader_) {
        ESP_LOGD(TAG, "Exited bootloader mode");
        this->in_bootloader_ = false;
      }
      this->frame_start_time_ = App.get_loop_component_start_time();
      this->buffer_[this->buffer_index_++] = byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_LENGTH;
      return;
    case ZWAVE_FRAME_TYPE_BL_MENU:
      ESP_LOGV(TAG, "Received BL_MENU");
      // Read the menu tentatively: a stray 0x0D can equally appear in garbled data after the
      // parser loses frame alignment, so bootloader mode is only committed once a plausible
      // menu completes (see READ_BL_MENU handling in parse_byte_)
      this->frame_start_time_ = App.get_loop_component_start_time();
      this->buffer_[this->buffer_index_++] = byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_READ_BL_MENU;
      return;
    case ZWAVE_FRAME_TYPE_BL_BEGIN_UPLOAD:
      ESP_LOGV(TAG, "Received BL_BEGIN_UPLOAD");
      break;
    case ZWAVE_FRAME_TYPE_ACK:
      ESP_LOGV(TAG, "Received ACK");
      break;
    case ZWAVE_FRAME_TYPE_NAK:
      ESP_LOGV(TAG, "Received NAK");
      break;
    case ZWAVE_FRAME_TYPE_CAN:
      ESP_LOGV(TAG, "Received CAN");
      break;
    default:
      ESP_LOGV(TAG, "Unrecognized START: 0x%02X", byte);
      return;
  }
  // Forward response (ACK/NAK/CAN) back to client for processing
  if (this->api_connection_ != nullptr) {
    // Store single byte in buffer and point to it
    this->buffer_[0] = byte;
    this->outgoing_proto_msg_.data = this->buffer_.data();
    this->outgoing_proto_msg_.data_len = 1;
    this->api_connection_->send_message(this->outgoing_proto_msg_);
  }
}

bool ZWaveProxy::response_handler_slow_() {
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

#endif  // USE_API
