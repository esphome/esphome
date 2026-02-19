#include "zigbee_proxy.h"

#ifdef USE_ZIGBEE_PROXY

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/api/api_server.h"
#include "ezsp_commands.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome {
namespace zigbee_proxy {

static const char *const TAG = "zigbee_proxy";

ZigbeeProxy *global_zigbee_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

ZigbeeProxy::ZigbeeProxy() { global_zigbee_proxy = this; }

void ZigbeeProxy::setup() {
  this->setup_time_ = millis();

  // Initialize state
  this->ash_state_ = AshState::DISCONNECTED;
  this->parsing_state_ = ParsingState::WAIT_FLAG_START;
  this->tx_sequence_ = 0;
  this->rx_sequence_ = 0;

  // Send RST frame to initialize NCP
  this->reset_ash_protocol_();
}

void ZigbeeProxy::loop() {
  // Process incoming UART data
  this->process_uart_();

  // Check for ACK timeout and handle retransmission
  if (this->tx_buffer_pending_ && this->check_ack_timeout_()) {
    this->handle_retransmission_();
  }

  // Check for boot sequence timeout
  if (this->boot_sequence_active_) {
    uint32_t elapsed = millis() - this->setup_time_;
    if (elapsed > 10000) {  // 10 second timeout for entire boot sequence
      ESP_LOGE(TAG, "Boot sequence timeout (state: %d)", static_cast<int>(this->boot_state_));
      this->boot_state_ = BootState::FAILED;
      this->boot_sequence_active_ = false;
      // Still mark as connected so proxy can work without network info
      if (this->ash_state_ != AshState::CONNECTED) {
        this->ash_state_ = AshState::FAILED;
      }
    }
  }

  // Check if we're stuck in CONNECTING state (before boot sequence starts)
  if (this->ash_state_ == AshState::CONNECTING && !this->boot_sequence_active_) {
    if (millis() - this->setup_time_ > ASH_RESET_TIMEOUT) {
      ESP_LOGE(TAG, "RSTACK timeout, NCP not responding");
      this->ash_state_ = AshState::FAILED;
    }
  }
}

void ZigbeeProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zigbee Proxy:\n"
                "  Buffer Size: %u bytes\n"
                "  Initial Timeout: %u ms\n"
                "  Min Timeout: %u ms\n"
                "  Max Timeout: %u ms",
                MAX_ASH_FRAME_SIZE, this->timeout_config_.initial_timeout_ms, this->timeout_config_.min_timeout_ms,
                this->timeout_config_.max_timeout_ms);

  if (this->network_info_.valid) {
    ESP_LOGCONFIG(TAG,
                  "  IEEE Address: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n"
                  "  PAN ID: 0x%04X\n"
                  "  Channel: %u",
                  this->network_info_.ieee_address[7], this->network_info_.ieee_address[6],
                  this->network_info_.ieee_address[5], this->network_info_.ieee_address[4],
                  this->network_info_.ieee_address[3], this->network_info_.ieee_address[2],
                  this->network_info_.ieee_address[1], this->network_info_.ieee_address[0], this->network_info_.pan_id,
                  this->network_info_.channel);
  }

  if (this->ash_state_ == AshState::FAILED) {
    ESP_LOGCONFIG(TAG, "  Status: Failed (NCP communication error)");
  } else if (this->ash_state_ == AshState::CONNECTED) {
    ESP_LOGCONFIG(TAG, "  Status: Connected");
  } else {
    ESP_LOGCONFIG(TAG, "  Status: Connecting...");
  }
}

float ZigbeeProxy::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

bool ZigbeeProxy::can_proceed() { return this->ash_state_ == AshState::CONNECTED; }

void ZigbeeProxy::api_connection_authenticated(api::APIConnection *conn) {
  // Notify client of network info if available
  if (this->network_info_.valid) {
    this->send_network_info_changed_msg_(conn);
  }
}

void ZigbeeProxy::zigbee_proxy_request(api::APIConnection *api_connection, const api::ZigbeeProxyRequest &msg) {
  switch (msg.type) {
    case api::enums::ZIGBEE_PROXY_REQUEST_TYPE_SUBSCRIBE:
      if (this->api_connection_ != nullptr) {
        ESP_LOGW(TAG, "Another client is already subscribed");
        return;
      }
      ESP_LOGI(TAG, "Client subscribed");
      this->api_connection_ = api_connection;
      this->client_reset_session_();
      break;

    case api::enums::ZIGBEE_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      if (this->api_connection_ == api_connection) {
        ESP_LOGI(TAG, "Client unsubscribed");
        this->api_connection_ = nullptr;
      }
      break;

    default:
      ESP_LOGW(TAG, "Unknown request type: %d", static_cast<int>(msg.type));
      break;
  }
}

void ZigbeeProxy::zigbee_proxy_frame(api::APIConnection *api_connection, const api::ZigbeeProxyFrame &msg) {
  if (this->api_connection_ != api_connection) {
    ESP_LOGW(TAG, "Frame received from non-subscribed client");
    return;
  }

  // Feed raw bytes into the client-side ASH parser
  for (size_t i = 0; i < msg.data_len; i++) {
    this->client_parse_byte_(msg.data[i]);
  }
}

uint64_t ZigbeeProxy::get_ieee_address() const {
  uint64_t addr = 0;
  for (size_t i = 0; i < ZIGBEE_IEEE_ADDR_SIZE; i++) {
    addr |= static_cast<uint64_t>(this->network_info_.ieee_address[i]) << (i * 8);
  }
  return addr;
}

void ZigbeeProxy::send_frame(const uint8_t *data, size_t length) {
  if (this->ash_state_ != AshState::CONNECTED) {
    ESP_LOGW(TAG, "Cannot send frame, not connected");
    return;
  }

  if (this->tx_buffer_pending_) {
    ESP_LOGW(TAG, "Cannot send frame, previous frame still pending");
    return;
  }

  // Validate frame size
  if (length + 4 > MAX_ASH_FRAME_SIZE) {  // +4 for control, CRC, FLAGS
    ESP_LOGE(TAG, "Frame too large: %u bytes (max %u)", length, MAX_ASH_FRAME_SIZE - 4);
    return;
  }

  this->send_data_frame_(data, length, false);
}

void ZigbeeProxy::set_timeout_config(uint32_t initial_ms, uint32_t min_ms, uint32_t max_ms) {
  this->timeout_config_.initial_timeout_ms = initial_ms;
  this->timeout_config_.min_timeout_ms = min_ms;
  this->timeout_config_.max_timeout_ms = max_ms;
  this->timeout_config_.current_timeout_ms = initial_ms;
  ESP_LOGI(TAG, "Timeout config updated: initial=%u, min=%u, max=%u", initial_ms, min_ms, max_ms);
}

// ASH Protocol State Machine
void ZigbeeProxy::reset_ash_protocol_() {
  ESP_LOGI(TAG, "Resetting ASH protocol");
  this->ash_state_ = AshState::CONNECTING;
  this->tx_sequence_ = 0;
  this->rx_sequence_ = 0;
  this->tx_buffer_pending_ = false;
  this->tx_retry_count_ = 0;
  this->parsing_state_ = ParsingState::WAIT_FLAG_START;
  this->setup_time_ = millis();

  // Start boot sequence
  this->boot_state_ = BootState::WAIT_RSTACK;
  this->boot_sequence_active_ = true;
  this->ezsp_sequence_ = 0;

  this->send_rst_frame_();
}

void ZigbeeProxy::send_rst_frame_() {
  // Send CANCEL bytes (0x1A) to clear NCP's receive buffer before RST
  // This is required by ASH protocol to ensure NCP ignores any partial frames
  // Note: 0x1A is ASH_CAN (cancel), NOT 0x18 which is ASH_SUB (substitute)
  static constexpr uint8_t ASH_CAN_BYTE = 0x1A;
  for (int i = 0; i < 32; i++) {
    this->write_byte(ASH_CAN_BYTE);
  }
  this->flush();

  // Small delay to allow NCP to process CANCEL bytes
  delay(10);

  // Now send the RST frame
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, nullptr, 0, AshFrameType::RST);

  // Debug: log exact bytes being sent
  ESP_LOGV(TAG, "RST frame bytes (%u): %s", length, format_hex_pretty(frame, length).c_str());

  this->write_array(frame, length);
  this->flush();
  ESP_LOGD(TAG, "Sent RST frame (with 32 CAN bytes prefix)");
}

void ZigbeeProxy::handle_rstack_frame_(const uint8_t *data, size_t length) {
  // Reset sequence numbers on any RSTACK
  this->tx_sequence_ = 0;
  this->rx_sequence_ = 0;
  this->clear_tx_buffer_();

  if (this->boot_state_ == BootState::WAIT_RSTACK) {
    // Initial RSTACK - start boot sequence
    ESP_LOGI(TAG, "Received RSTACK, starting EZSP initialization");
    this->ash_state_ = AshState::CONNECTED;

    // Small delay to let NCP settle and clear any pending data
    delay(50);

    // Drain any garbage bytes from UART buffer
    while (this->available()) {
      uint8_t byte;
      this->read_byte(&byte);
      ESP_LOGV(TAG, "Draining post-RSTACK byte: 0x%02X", byte);
    }

    this->boot_state_ = BootState::SEND_VERSION;
    this->advance_boot_state_();
  } else if (this->boot_state_ == BootState::WAIT_FINAL_RSTACK) {
    // Final RSTACK after harvesting - boot complete
    ESP_LOGI(TAG, "Boot sequence complete, NCP reset to clean state");
    this->ash_state_ = AshState::CONNECTED;
    this->boot_state_ = BootState::COMPLETE;
    this->boot_sequence_active_ = false;

    // Now check for WiFi/Zigbee channel conflicts
    this->check_wifi_zigbee_conflict_();
  } else if (this->ash_state_ == AshState::CONNECTING) {
    // RSTACK during connecting (triggered by client RST forwarding)
    ESP_LOGI(TAG, "Received RSTACK, NCP ready");
    this->ash_state_ = AshState::CONNECTED;
    // Forward to client if subscribed
    if (this->api_connection_ != nullptr) {
      this->forward_ncp_rstack_to_client_(this->rx_buffer_.data() + 1, this->rx_buffer_index_ - 3);
    }
  } else if (this->api_connection_ != nullptr) {
    // Client is subscribed, forward RSTACK
    ESP_LOGI(TAG, "Forwarding RSTACK to client");
    this->ash_state_ = AshState::CONNECTED;
    this->forward_ncp_rstack_to_client_(this->rx_buffer_.data() + 1, this->rx_buffer_index_ - 3);
  } else {
    ESP_LOGW(TAG, "Unexpected RSTACK received (boot_state=%d)", static_cast<int>(this->boot_state_));
  }
}

void ZigbeeProxy::handle_error_frame_(const uint8_t *data, size_t length) {
  if (length < 1) {
    ESP_LOGE(TAG, "ERROR frame too short");
    return;
  }

  uint8_t error_code = data[0];
  const char *error_str = "Unknown error";

  switch (static_cast<EzspError>(error_code)) {
    case EzspError::VERSION_NOT_SET:
      error_str = "Version not set";
      break;
    case EzspError::RESET_UNKNOWN:
      error_str = "Reset (unknown)";
      break;
    case EzspError::RESET_EXTERNAL:
      error_str = "External reset";
      break;
    case EzspError::RESET_POWER_ON:
      error_str = "Power-on reset";
      break;
    case EzspError::RESET_WATCHDOG:
      error_str = "Watchdog reset";
      break;
    case EzspError::RESET_ASSERT:
      error_str = "Assert reset";
      break;
    case EzspError::RESET_BOOTLOADER:
      error_str = "Bootloader reset";
      break;
    case EzspError::RESET_SOFTWARE:
      error_str = "Software reset";
      break;
    case EzspError::EXCEEDED_MAXIMUM_ACK_TIMEOUT_COUNT:
      error_str = "Exceeded maximum ACK timeout count";
      break;
  }

  ESP_LOGE(TAG, "NCP ERROR: %s (0x%02X)", error_str, error_code);

  if (this->api_connection_ != nullptr) {
    // Forward error to client
    this->forward_ncp_error_to_client_(data, length);
  } else {
    // No client, attempt recovery ourselves
    ESP_LOGI(TAG, "Attempting recovery...");
    this->reset_ash_protocol_();
  }
}

bool ZigbeeProxy::send_ack_frame_(uint8_t ack_num) {
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, nullptr, 0, AshFrameType::ACK, 0, ack_num);
  this->write_array(frame, length);
  this->flush();
  this->last_ack_sent_ = ack_num;
  ESP_LOGV(TAG, "Sent ACK for frame %d", ack_num);
  return true;
}

bool ZigbeeProxy::send_nak_frame_(uint8_t ack_num) {
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, nullptr, 0, AshFrameType::NAK, 0, ack_num);
  this->write_array(frame, length);
  this->flush();
  ESP_LOGW(TAG, "Sent NAK for frame %d", ack_num);
  return true;
}

bool ZigbeeProxy::send_data_frame_(const uint8_t *data, size_t length, bool retransmit) {
  // Validate frame size
  if (length + 4 > MAX_ASH_FRAME_SIZE) {
    ESP_LOGE(TAG, "Frame too large: %u bytes", length);
    return false;
  }

  // Build frame
  size_t frame_length = this->build_frame_(this->tx_buffer_.data(), data, length, AshFrameType::DATA,
                                           this->tx_sequence_, this->rx_sequence_, retransmit);

  // Store for potential retransmission
  if (!retransmit) {
    memcpy(this->tx_pending_buffer_.data(), this->tx_buffer_.data(), frame_length);
    this->tx_pending_length_ = frame_length;
    this->tx_pending_frame_num_ = this->tx_sequence_;
  }

  // Debug: log exact bytes being sent
  ESP_LOGV(TAG, "TX DATA frame (%u bytes): %s", frame_length,
           format_hex_pretty(this->tx_buffer_.data(), frame_length).c_str());

  // Send frame
  this->write_array(this->tx_buffer_.data(), frame_length);
  this->flush();

  // Start ACK timer
  this->tx_buffer_pending_ = true;
  this->start_ack_timer_();

  ESP_LOGV(TAG, "Sent DATA frame %d (%s), length: %u", this->tx_sequence_, retransmit ? "retransmit" : "new", length);

  // Increment TX sequence for next frame (only for new frames)
  if (!retransmit) {
    this->increment_tx_sequence_();
  }

  return true;
}

// Timeout management
void ZigbeeProxy::update_adaptive_timeout_(uint32_t measured_rtt_ms) {
  // Formula: new_timeout = (7/8) * current + (1/2) * measured_rtt
  uint32_t new_timeout = (this->timeout_config_.current_timeout_ms * 7 + measured_rtt_ms * 4) / 8;

  // Clamp to configured bounds
  if (new_timeout < this->timeout_config_.min_timeout_ms) {
    new_timeout = this->timeout_config_.min_timeout_ms;
  } else if (new_timeout > this->timeout_config_.max_timeout_ms) {
    new_timeout = this->timeout_config_.max_timeout_ms;
  }

  this->timeout_config_.current_timeout_ms = new_timeout;
  this->last_rtt_ms_ = measured_rtt_ms;

  ESP_LOGV(TAG, "Updated timeout: %u ms (RTT: %u ms)", new_timeout, measured_rtt_ms);
}

bool ZigbeeProxy::check_ack_timeout_() {
  if (!this->tx_buffer_pending_) {
    return false;
  }

  uint32_t elapsed = millis() - this->ack_timer_start_;
  return elapsed >= this->timeout_config_.current_timeout_ms;
}

// Retransmission
void ZigbeeProxy::handle_retransmission_() {
  if (!this->tx_buffer_pending_) {
    return;
  }

  this->tx_retry_count_++;

  if (this->tx_retry_count_ > ASH_MAX_RETRIES) {
    ESP_LOGE(TAG, "Max retries exceeded, entering FAILED state");
    this->ash_state_ = AshState::FAILED;
    this->clear_tx_buffer_();
    return;
  }

  ESP_LOGW(TAG, "Retransmitting frame %d (attempt %d/%d)", this->tx_pending_frame_num_, this->tx_retry_count_,
           ASH_MAX_RETRIES);

  // Resend the pending frame
  this->write_array(this->tx_pending_buffer_.data(), this->tx_pending_length_);
  this->flush();
  this->start_ack_timer_();
}

// Boot-time NCP initialization sequence
// Sequence: RST -> RSTACK -> version() -> networkInit() -> stackStatus -> getNetworkParameters() -> RST -> RSTACK

void ZigbeeProxy::start_boot_sequence_() {
  ESP_LOGI(TAG, "Starting boot sequence to harvest network info");
  this->boot_state_ = BootState::WAIT_RSTACK;
  this->boot_sequence_active_ = true;
  this->ezsp_sequence_ = 0;
  this->reset_ash_protocol_();
}

void ZigbeeProxy::advance_boot_state_() {
  switch (this->boot_state_) {
    case BootState::SEND_VERSION:
      this->send_ezsp_version_();
      this->boot_state_ = BootState::WAIT_VERSION;
      break;

    case BootState::SEND_NETWORK_INIT:
      this->send_network_init_();
      this->boot_state_ = BootState::WAIT_STACK_STATUS;
      break;

    case BootState::SEND_GET_NETWORK_PARAMS:
      this->send_get_network_params_();
      this->boot_state_ = BootState::WAIT_NETWORK_PARAMS;
      break;

    case BootState::SEND_FINAL_RST:
      ESP_LOGI(TAG, "Sending final RST to reset NCP to clean state");
      this->boot_state_ = BootState::WAIT_FINAL_RSTACK;
      this->send_rst_frame_();
      break;

    default:
      break;
  }
}

void ZigbeeProxy::handle_boot_data_frame_(const uint8_t *data, size_t length) {
  // EZSP frame format depends on negotiated version:
  // Legacy (v4-v7): [sequence] [frame_control] [frame_id] [data...]  (3-byte header)
  // Extended (v8+): [sequence] [frame_control_low] [frame_control_high] [frame_id_low] [frame_id_high] [data...]
  //
  // Note: Some NCPs may respond in legacy format for a few frames after version negotiation
  // before fully switching to extended format. We handle this by falling back to legacy
  // parsing if the frame is too short for extended format.

  if (length < 3) {
    ESP_LOGW(TAG, "Boot frame too short: %u bytes", length);
    return;
  }

  uint8_t frame_control;
  uint16_t frame_id;
  const uint8_t *payload;
  size_t payload_length;

  // Determine format: prefer extended for v8+, but fall back to legacy if frame is too short
  bool use_extended = (this->ezsp_version_ >= 8) && (length >= 5);

  if (use_extended) {
    frame_control = data[1];
    frame_id = data[3] | (static_cast<uint16_t>(data[4]) << 8);
    payload = data + 5;
    payload_length = length - 5;
  } else {
    frame_control = data[1];
    frame_id = data[2];
    payload = data + 3;
    payload_length = length - 3;
  }

  // Check if this is a response (not a callback)
  bool is_response = (frame_control & 0x80) != 0;
  bool is_callback = (frame_control & 0x10) != 0;

  ESP_LOGV(TAG, "Boot EZSP frame (%s): id=0x%04X, response=%d, callback=%d, payload_len=%u",
           use_extended ? "extended" : "legacy", frame_id, is_response, is_callback, payload_length);

  // Handle based on current boot state
  switch (this->boot_state_) {
    case BootState::WAIT_VERSION:
      if (frame_id == EZSP_VERSION && is_response) {
        this->handle_version_response_(payload, payload_length);
      }
      break;

    case BootState::WAIT_STACK_STATUS:
      if (frame_id == EZSP_STACK_STATUS_HANDLER && is_callback) {
        this->handle_stack_status_(payload, payload_length);
      } else if (frame_id == EZSP_NETWORK_INIT && is_response) {
        // networkInit response contains EmberStatus
        // Some NCPs proceed directly without stackStatusHandler callback
        if (payload_length >= 1) {
          uint8_t status = payload[0];
          ESP_LOGD(TAG, "networkInit response: status=0x%02X", status);
          // Proceed to getNetworkParameters regardless of status
          // getNetworkParameters will tell us if there's actually a network
          ESP_LOGI(TAG, "networkInit complete, querying network parameters");
          this->boot_state_ = BootState::SEND_GET_NETWORK_PARAMS;
          this->advance_boot_state_();
        }
      }
      break;

    case BootState::WAIT_NETWORK_PARAMS:
      if (frame_id == EZSP_GET_NETWORK_PARAMETERS && is_response) {
        this->handle_network_params_response_(payload, payload_length);
      }
      break;

    default:
      break;
  }
}

void ZigbeeProxy::send_ezsp_version_() {
  // EZSP version command must use LEGACY format (v4-v7) for initial handshake
  // because NCP starts in legacy mode until version negotiation completes.
  // Legacy format: [sequence] [frame_control] [frame_id] [desiredProtocolVersion]
  this->ezsp_requested_version_ = EZSP_MAX_VERSION;
  uint8_t cmd[] = {
      this->ezsp_sequence_++,      // Sequence
      EZSP_FRAME_CONTROL_COMMAND,  // Frame control (command, no callback)
      0x00,                        // Frame ID (version command = 0x00)
      EZSP_MAX_VERSION             // Desired protocol version
  };

  ESP_LOGD(TAG, "Sending EZSP version command (legacy format, requesting v%d)", EZSP_MAX_VERSION);
  this->send_data_frame_(cmd, sizeof(cmd), false);
}

void ZigbeeProxy::send_network_init_() {
  // networkInit command - use legacy format for compatibility
  // Some NCPs need a command or two in legacy format after version negotiation
  // before fully switching to extended format.
  // networkInitStruct: [bitmask (2 bytes)] - use 0x0000 for default
  uint8_t cmd[] = {
      this->ezsp_sequence_++,      // Sequence
      EZSP_FRAME_CONTROL_COMMAND,  // Frame control
      EZSP_NETWORK_INIT & 0xFF,    // Frame ID
      0x00, 0x00                   // networkInitStruct bitmask (default)
  };
  ESP_LOGD(TAG, "Sending EZSP networkInit command (legacy format)");
  this->send_data_frame_(cmd, sizeof(cmd), false);
}

void ZigbeeProxy::send_get_network_params_() {
  // getNetworkParameters command - use legacy format for boot sequence compatibility
  uint8_t cmd[] = {
      this->ezsp_sequence_++,             // Sequence
      EZSP_FRAME_CONTROL_COMMAND,         // Frame control
      EZSP_GET_NETWORK_PARAMETERS & 0xFF  // Frame ID
  };
  ESP_LOGD(TAG, "Sending EZSP getNetworkParameters command (legacy format)");
  this->send_data_frame_(cmd, sizeof(cmd), false);
}

void ZigbeeProxy::handle_version_response_(const uint8_t *data, size_t length) {
  // Version response format depends on whether NCP supports requested version:
  // - If supported: [protocolVersion] [stackType] [stackVersion (2 bytes)]
  // - If NOT supported: [protocolVersion] only (NCP's supported version)
  //
  // When NCP doesn't support the requested version, it responds with just
  // its supported version, indicating we should re-negotiate.

  if (length < 1) {
    ESP_LOGW(TAG, "Version response empty");
    this->boot_state_ = BootState::FAILED;
    return;
  }

  uint8_t ncp_version = data[0];

  if (length == 1) {
    // NCP responded with just its version
    // This happens when:
    // 1. We requested a version the NCP doesn't support -> re-negotiate
    // 2. We requested the NCP's version and it accepted -> treat as success

    if (ncp_version == this->ezsp_requested_version_) {
      // NCP accepted our requested version - treat as success
      ESP_LOGI(TAG, "NCP accepted EZSP v%d", ncp_version);
      this->ezsp_version_ = ncp_version;
      // Proceed to networkInit
      this->boot_state_ = BootState::SEND_NETWORK_INIT;
      this->advance_boot_state_();
      return;
    }

    // NCP doesn't support our version - re-negotiate
    ESP_LOGI(TAG, "NCP supports EZSP v%d, re-negotiating", ncp_version);
    this->ezsp_requested_version_ = ncp_version;

    // Re-send version command with NCP's supported version
    // Use legacy format for re-negotiation (NCP stays in legacy until handshake completes)
    uint8_t cmd[] = {
        this->ezsp_sequence_++,      // Sequence
        EZSP_FRAME_CONTROL_COMMAND,  // Frame control
        0x00,                        // Frame ID (version)
        ncp_version                  // Use NCP's version
    };
    ESP_LOGD(TAG, "Re-sending EZSP version command (requesting v%d)", ncp_version);
    this->send_data_frame_(cmd, sizeof(cmd), false);
    // Stay in WAIT_VERSION state
    return;
  }

  // Full response with stack info
  if (length < 4) {
    ESP_LOGW(TAG, "Version response too short: %u bytes", length);
    this->boot_state_ = BootState::FAILED;
    return;
  }

  this->ezsp_version_ = data[0];
  uint8_t stack_type = data[1];
  uint16_t stack_version = data[2] | (static_cast<uint16_t>(data[3]) << 8);

  ESP_LOGI(TAG, "NCP EZSP version: %d, stack type: %d, stack version: 0x%04X", this->ezsp_version_, stack_type,
           stack_version);

  if (this->ezsp_version_ < EZSP_MIN_VERSION) {
    ESP_LOGE(TAG, "EZSP version %d not supported (minimum: %d)", this->ezsp_version_, EZSP_MIN_VERSION);
    this->boot_state_ = BootState::FAILED;
    return;
  }

  // Proceed to networkInit
  this->boot_state_ = BootState::SEND_NETWORK_INIT;
  this->advance_boot_state_();
}

void ZigbeeProxy::handle_stack_status_(const uint8_t *data, size_t length) {
  // stackStatusHandler callback: [status]
  if (length < 1) {
    ESP_LOGW(TAG, "Stack status too short");
    return;
  }

  uint8_t status = data[0];
  ESP_LOGD(TAG, "Stack status: 0x%02X", status);

  // Check for network up status
  if (status == static_cast<uint8_t>(EmberStatus::NETWORK_UP) || status == static_cast<uint8_t>(EmberStatus::SUCCESS)) {
    ESP_LOGI(TAG, "Network is up, querying parameters");
    this->boot_state_ = BootState::SEND_GET_NETWORK_PARAMS;
    this->advance_boot_state_();
  } else if (status == static_cast<uint8_t>(EmberStatus::NOT_JOINED)) {
    // No network configured - that's fine, we just won't have network info
    ESP_LOGI(TAG, "No network configured on NCP");
    this->boot_state_ = BootState::SEND_FINAL_RST;
    this->advance_boot_state_();
  } else {
    ESP_LOGW(TAG, "Unexpected stack status: 0x%02X, continuing anyway", status);
    // Try to get network params anyway
    this->boot_state_ = BootState::SEND_GET_NETWORK_PARAMS;
    this->advance_boot_state_();
  }
}

void ZigbeeProxy::handle_network_params_response_(const uint8_t *data, size_t length) {
  // getNetworkParameters response:
  // [status] [nodeType] [extendedPanId (8)] [panId (2)] [radioTxPower] [radioChannel] ...
  if (length < 14) {
    ESP_LOGW(TAG, "Network params response too short: %u bytes", length);
    this->boot_state_ = BootState::SEND_FINAL_RST;
    this->advance_boot_state_();
    return;
  }

  uint8_t status = data[NETWORK_PARAMS_STATUS_OFFSET];
  if (status != static_cast<uint8_t>(EmberStatus::SUCCESS)) {
    ESP_LOGW(TAG, "getNetworkParameters failed with status: 0x%02X", status);
    this->boot_state_ = BootState::SEND_FINAL_RST;
    this->advance_boot_state_();
    return;
  }

  // Extract Extended PAN ID (8 bytes, little-endian)
  memcpy(this->network_info_.extended_pan_id.data(), data + NETWORK_PARAMS_EXT_PAN_ID_OFFSET, 8);

  // Extract PAN ID (2 bytes, little-endian)
  this->network_info_.pan_id =
      data[NETWORK_PARAMS_PAN_ID_OFFSET] | (static_cast<uint16_t>(data[NETWORK_PARAMS_PAN_ID_OFFSET + 1]) << 8);

  // Extract channel
  this->network_info_.channel = data[NETWORK_PARAMS_CHANNEL_OFFSET];

  this->network_info_.valid = true;

  ESP_LOGI(TAG,
           "Network info harvested:\n"
           "  Extended PAN ID: %02X%02X%02X%02X%02X%02X%02X%02X\n"
           "  PAN ID: 0x%04X\n"
           "  Channel: %u",
           this->network_info_.extended_pan_id[7], this->network_info_.extended_pan_id[6],
           this->network_info_.extended_pan_id[5], this->network_info_.extended_pan_id[4],
           this->network_info_.extended_pan_id[3], this->network_info_.extended_pan_id[2],
           this->network_info_.extended_pan_id[1], this->network_info_.extended_pan_id[0], this->network_info_.pan_id,
           this->network_info_.channel);

  // Now reset NCP to clean state
  this->boot_state_ = BootState::SEND_FINAL_RST;
  this->advance_boot_state_();
}

bool ZigbeeProxy::set_ieee_address_(const uint8_t *new_address) {
  bool changed = false;

  for (size_t i = 0; i < ZIGBEE_IEEE_ADDR_SIZE; i++) {
    if (this->network_info_.ieee_address[i] != new_address[i]) {
      changed = true;
      break;
    }
  }

  if (changed) {
    memcpy(this->network_info_.ieee_address.data(), new_address, ZIGBEE_IEEE_ADDR_SIZE);
    this->network_info_.valid = true;
    ESP_LOGI(TAG, "IEEE address updated: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", new_address[7], new_address[6],
             new_address[5], new_address[4], new_address[3], new_address[2], new_address[1], new_address[0]);
    this->send_network_info_changed_msg_();
    return true;
  }

  return false;
}

void ZigbeeProxy::send_network_info_changed_msg_(api::APIConnection *conn) {
  // This would send network info to the API client
  // For now, we'll log it
  ESP_LOGD(TAG, "Network info changed notification");
}

// WiFi/Zigbee channel conflict detection
void ZigbeeProxy::check_wifi_zigbee_conflict_() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component == nullptr || !this->network_info_.valid || this->network_info_.channel == 0) {
    return;
  }

  uint8_t wifi_channel = wifi::global_wifi_component->get_wifi_channel();
  if (wifi_channel == 0) {
    return;  // WiFi not connected yet
  }

  uint8_t zigbee_channel = this->network_info_.channel;

  // Check for overlap
  bool conflict = false;
  const char *recommendation = "";

  if (zigbee_channel >= 11 && zigbee_channel <= 14) {
    // Zigbee 11-14 overlaps WiFi 1
    if (wifi_channel == 1) {
      conflict = true;
      recommendation = "Use Zigbee channel 25-26 or WiFi channel 6/11";
    }
  } else if (zigbee_channel >= 15 && zigbee_channel <= 18) {
    // Zigbee 15-18 overlaps WiFi 6
    if (wifi_channel == 6) {
      conflict = true;
      recommendation = "Use Zigbee channel 25-26 or WiFi channel 1/11";
    }
  } else if (zigbee_channel >= 19 && zigbee_channel <= 22) {
    // Zigbee 19-22 overlaps WiFi 11
    if (wifi_channel == 11) {
      conflict = true;
      recommendation = "Use Zigbee channel 25-26 or WiFi channel 1/6";
    }
  } else if (zigbee_channel >= 23 && zigbee_channel <= 26) {
    // Zigbee 23-26 overlaps WiFi 13-14
    if (wifi_channel >= 13) {
      conflict = true;
      recommendation = "Use Zigbee channel 15-20 or WiFi channel 1/6/11";
    }
  }

  if (conflict) {
    ESP_LOGW(TAG,
             "WiFi/Zigbee channel conflict detected\n"
             "  WiFi channel: %u, Zigbee channel: %u\n"
             "  Recommendation: %s",
             wifi_channel, zigbee_channel, recommendation);
  } else {
    ESP_LOGI(TAG, "No WiFi/Zigbee channel conflict (WiFi: %u, Zigbee: %u)", wifi_channel, zigbee_channel);
  }
#endif
}

// Bootloader detection
void ZigbeeProxy::check_bootloader_mode_(const uint8_t *data, size_t length) {
  if (length < 2) {
    return;
  }

  // Check for Silicon Labs bootloader menu prompt (0xC1 0x0D)
  if (data[0] == 0xC1 && data[1] == 0x0D) {
    if (this->bootloader_state_ != BootloaderState::MENU) {
      ESP_LOGW(TAG, "NCP in bootloader menu mode detected\n"
                    "Please flash NCP firmware or power cycle the device");
      this->bootloader_state_ = BootloaderState::MENU;
    }
    return;
  }

  // Check for upload begin (0x43)
  if (data[0] == 0x43) {
    if (this->bootloader_state_ != BootloaderState::DETECTED) {
      ESP_LOGW(TAG, "NCP bootloader upload mode detected");
      this->bootloader_state_ = BootloaderState::DETECTED;
    }
    return;
  }

  // Reset bootloader state if we see normal traffic
  if (this->bootloader_state_ != BootloaderState::NORMAL && this->ash_state_ == AshState::CONNECTED) {
    ESP_LOGI(TAG, "NCP returned to normal operation");
    this->bootloader_state_ = BootloaderState::NORMAL;
  }
}

// UART processing
void ZigbeeProxy::process_uart_() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);

    // Verbose logging for debugging (ESP_LOGV already checks log level)
    ESP_LOGV(TAG, "RX: 0x%02X", byte);

    this->parse_byte_(byte);
  }
}

// ==================== Client-side ASH session ====================

void ZigbeeProxy::client_reset_session_() {
  this->client_tx_sequence_ = 0;
  this->client_rx_sequence_ = 0;
  this->client_rx_buffer_index_ = 0;
  this->client_escape_next_byte_ = false;
  this->client_ash_state_ = AshState::DISCONNECTED;
  this->client_parsing_state_ = ParsingState::WAIT_FLAG_START;
  ESP_LOGD(TAG, "Client ASH session reset");
}

void ZigbeeProxy::send_to_client_(const uint8_t *data, size_t length) {
  if (this->api_connection_ == nullptr) {
    return;
  }
  this->outgoing_proto_msg_.data = data;
  this->outgoing_proto_msg_.data_len = length;
  this->api_connection_->send_zigbee_proxy_frame(this->outgoing_proto_msg_);
}

void ZigbeeProxy::client_send_raw_frame_(const uint8_t *frame, size_t length) {
  this->send_to_client_(frame, length);
}

void ZigbeeProxy::client_send_ack_frame_(uint8_t ack_num) {
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, nullptr, 0, AshFrameType::ACK, 0, ack_num);
  this->client_send_raw_frame_(frame, length);
  ESP_LOGV(TAG, "Sent client ACK for frame %d", ack_num);
}

void ZigbeeProxy::client_send_rstack_frame_(uint8_t reset_code) {
  // RSTACK payload: [version] [reset_code]
  uint8_t payload[] = {0x02, reset_code};
  uint8_t frame[16];
  size_t length = this->build_frame_(frame, payload, sizeof(payload), AshFrameType::RSTACK);
  this->client_send_raw_frame_(frame, length);
  ESP_LOGD(TAG, "Sent client RSTACK (code=0x%02X)", reset_code);
}

void ZigbeeProxy::client_send_data_frame_(const uint8_t *data, size_t length) {
  uint8_t frame[MAX_ASH_FRAME_SIZE];
  size_t frame_length = this->build_frame_(frame, data, length, AshFrameType::DATA,
                                            this->client_tx_sequence_, this->client_rx_sequence_);
  this->client_tx_sequence_ = (this->client_tx_sequence_ + 1) & ASH_MAX_SEQUENCE;
  this->client_send_raw_frame_(frame, frame_length);
  ESP_LOGV(TAG, "Sent client DATA frame, payload %u bytes", length);
}

void ZigbeeProxy::client_send_error_frame_(uint8_t error_code) {
  uint8_t payload[] = {0x02, error_code};
  uint8_t frame[16];
  size_t length = this->build_frame_(frame, payload, sizeof(payload), AshFrameType::ERROR);
  this->client_send_raw_frame_(frame, length);
  ESP_LOGD(TAG, "Sent client ERROR (code=0x%02X)", error_code);
}

void ZigbeeProxy::forward_ncp_data_to_client_(const uint8_t *payload, size_t length) {
  this->client_send_data_frame_(payload, length);
}

void ZigbeeProxy::forward_ncp_rstack_to_client_(const uint8_t *data, size_t length) {
  // Build and send an RSTACK frame to the client with NCP's RSTACK data
  uint8_t frame[16];
  size_t frame_length = this->build_frame_(frame, data, length, AshFrameType::RSTACK);
  this->client_send_raw_frame_(frame, frame_length);

  // Reset client-side sequence numbers since RSTACK means new session
  this->client_tx_sequence_ = 0;
  this->client_rx_sequence_ = 0;
  this->client_ash_state_ = AshState::CONNECTED;
  ESP_LOGD(TAG, "Forwarded RSTACK to client");
}

void ZigbeeProxy::forward_ncp_error_to_client_(const uint8_t *data, size_t length) {
  uint8_t frame[16];
  size_t frame_length = this->build_frame_(frame, data, length, AshFrameType::ERROR);
  this->client_send_raw_frame_(frame, frame_length);
  ESP_LOGD(TAG, "Forwarded ERROR to client");
}

void ZigbeeProxy::client_parse_byte_(uint8_t byte) {
  static constexpr uint8_t ASH_CAN_BYTE = 0x1A;

  // CAN byte resets parser
  if (byte == ASH_CAN_BYTE) {
    this->client_rx_buffer_index_ = 0;
    this->client_escape_next_byte_ = false;
    this->client_parsing_state_ = ParsingState::WAIT_FLAG_START;
    return;
  }

  switch (this->client_parsing_state_) {
    case ParsingState::WAIT_FLAG_START:
      if (byte == ASH_ESCAPE_BYTE) {
        this->client_escape_next_byte_ = true;
        return;
      }
      if (this->client_escape_next_byte_) {
        byte ^= ASH_XOR_BYTE;
        this->client_escape_next_byte_ = false;
      }
      if (byte == ASH_FLAG_BYTE) {
        this->client_rx_buffer_index_ = 0;
        this->client_escape_next_byte_ = false;
        this->client_parsing_state_ = ParsingState::WAIT_CONTROL;
      } else if ((byte & 0x80) != 0 || this->client_ash_state_ == AshState::CONNECTED) {
        // Accept control byte without leading FLAG
        this->client_rx_buffer_index_ = 0;
        this->client_rx_buffer_[this->client_rx_buffer_index_++] = byte;
        this->client_parsing_state_ = ParsingState::WAIT_DATA;
      }
      break;

    case ParsingState::WAIT_CONTROL:
      if (byte == ASH_FLAG_BYTE) {
        // Empty frame or repeated FLAG
        this->client_rx_buffer_index_ = 0;
        return;
      }
      if (byte == ASH_ESCAPE_BYTE) {
        this->client_escape_next_byte_ = true;
        return;
      }
      if (this->client_escape_next_byte_) {
        byte ^= ASH_XOR_BYTE;
        this->client_escape_next_byte_ = false;
      }
      this->client_rx_buffer_[this->client_rx_buffer_index_++] = byte;
      this->client_parsing_state_ = ParsingState::WAIT_DATA;
      break;

    case ParsingState::WAIT_DATA:
      if (byte == ASH_FLAG_BYTE) {
        // End of frame - validate and process
        if (this->client_validate_frame_crc_()) {
          this->client_parse_control_byte_(this->client_rx_buffer_[0]);
        } else {
          ESP_LOGW(TAG, "Client frame CRC failed (%u bytes)", this->client_rx_buffer_index_);
        }
        this->client_parsing_state_ = ParsingState::WAIT_FLAG_START;
        return;
      }
      if (byte == ASH_ESCAPE_BYTE) {
        this->client_escape_next_byte_ = true;
        return;
      }
      if (this->client_escape_next_byte_) {
        byte ^= ASH_XOR_BYTE;
        this->client_escape_next_byte_ = false;
      }
      if (this->client_rx_buffer_index_ >= MAX_ASH_FRAME_SIZE) {
        ESP_LOGE(TAG, "Client RX buffer overflow");
        this->client_parsing_state_ = ParsingState::WAIT_FLAG_START;
        return;
      }
      this->client_rx_buffer_[this->client_rx_buffer_index_++] = byte;
      break;

    default:
      this->client_parsing_state_ = ParsingState::WAIT_FLAG_START;
      break;
  }
}

bool ZigbeeProxy::client_validate_frame_crc_() {
  if (this->client_rx_buffer_index_ < 3) {
    return false;
  }
  uint16_t calculated = this->calculate_crc_(this->client_rx_buffer_.data(), this->client_rx_buffer_index_ - 2);
  uint16_t received = (static_cast<uint16_t>(this->client_rx_buffer_[this->client_rx_buffer_index_ - 2]) << 8) |
                      this->client_rx_buffer_[this->client_rx_buffer_index_ - 1];
  return calculated == received;
}

void ZigbeeProxy::client_parse_control_byte_(uint8_t control) {
  AshFrameType frame_type;
  if ((control & 0x80) == 0) {
    frame_type = AshFrameType::DATA;
  } else if ((control & 0xC0) == 0x80) {
    frame_type = ((control & 0x20) == 0) ? AshFrameType::ACK : AshFrameType::NAK;
  } else {
    uint8_t control_bits = control & 0x07;
    if (control_bits == 0x00) {
      frame_type = AshFrameType::RST;
    } else if (control_bits == 0x01) {
      frame_type = AshFrameType::RSTACK;
    } else if (control_bits == 0x02) {
      frame_type = AshFrameType::ERROR;
    } else {
      ESP_LOGW(TAG, "Client: unknown control frame type: 0x%02X", control);
      return;
    }
  }

  uint8_t frame_num = (control >> 4) & 0x07;
  uint8_t ack_num = control & 0x07;

  switch (frame_type) {
    case AshFrameType::DATA: {
      // Verify sequence number
      if (frame_num != this->client_rx_sequence_) {
        ESP_LOGW(TAG, "Client: out of sequence DATA frame: expected %d, got %d", this->client_rx_sequence_, frame_num);
        return;
      }

      this->client_rx_sequence_ = (this->client_rx_sequence_ + 1) & ASH_MAX_SEQUENCE;

      // Extract EZSP payload (skip control byte, exclude CRC)
      size_t payload_length = this->client_rx_buffer_index_ > 3 ? this->client_rx_buffer_index_ - 3 : 0;
      const uint8_t *payload = this->client_rx_buffer_.data() + 1;

      if (payload_length > 0) {
        // Forward EZSP payload to NCP via right-side ASH
        ESP_LOGV(TAG, "Client DATA → NCP, EZSP payload %u bytes", payload_length);
        this->send_frame(payload, payload_length);
      }
      break;
    }

    case AshFrameType::ACK:
      // Client ACKed our data - nothing to retransmit on client side for now
      ESP_LOGV(TAG, "Client ACK received for frame %d", ack_num);
      break;

    case AshFrameType::NAK:
      ESP_LOGW(TAG, "Client NAK received for frame %d", ack_num);
      break;

    case AshFrameType::RST:
      // Client wants to reset - forward RST to NCP
      // Don't use reset_ash_protocol_() as that enters boot sequence
      ESP_LOGI(TAG, "Client RST → forwarding to NCP");
      this->ash_state_ = AshState::CONNECTING;
      this->tx_sequence_ = 0;
      this->rx_sequence_ = 0;
      this->tx_buffer_pending_ = false;
      this->tx_retry_count_ = 0;
      this->parsing_state_ = ParsingState::WAIT_FLAG_START;
      this->client_reset_session_();
      this->send_rst_frame_();
      break;

    case AshFrameType::RSTACK:
      // Client shouldn't send RSTACK, ignore
      ESP_LOGW(TAG, "Client sent unexpected RSTACK");
      break;

    case AshFrameType::ERROR:
      // Client sent error, log it
      ESP_LOGW(TAG, "Client sent ERROR frame");
      break;
  }
}

}  // namespace zigbee_proxy
}  // namespace esphome

#endif  // USE_ZIGBEE_PROXY
