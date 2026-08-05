#include "zigbee_proxy.h"

#ifdef USE_ZIGBEE_PROXY

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"
#include "esphome/components/api/api_server.h"
#include "ezsp_commands.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_ZIGBEE_PROXY_USB_UART
#include "esphome/components/usb_uart/usb_uart.h"
#endif

namespace esphome::zigbee_proxy {

static const char *const TAG = "zigbee_proxy";

static constexpr uint32_t BOOT_SEQUENCE_TIMEOUT_MS = 10000;    // Overall boot-harvest timeout
static constexpr uint32_t RECOVERY_RETRY_INTERVAL_MS = 30000;  // Retry interval for a failed NCP link
static constexpr uint32_t CLIENT_TX_RETRY_TIMEOUT_MS = 5000;   // Give up on a backpressured client frame
static constexpr size_t NETWORK_INFO_PAYLOAD_SIZE = 19;        // ieee(8) + extended_pan(8) + pan_id(2) + channel(1)
static constexpr size_t ZIGBEE_MAX_LOG_BYTES = 168;            // Cap verbose hex dumps (168 * 3 = 504 byte buffer)

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

  if (this->boot_sequence_active_) {
    this->check_boot_timeouts_();
  } else if (this->ash_state_ == AshState::CONNECTING && millis() - this->setup_time_ > ASH_RESET_TIMEOUT) {
    // Stuck in CONNECTING state (client-triggered RST, not the boot sequence)
    ESP_LOGE(TAG, "RSTACK timeout, NCP not responding");
    this->ash_state_ = AshState::FAILED;
  }

  // Guard against a subscriber that disconnected without unsubscribing
  if (this->api_connection_ != nullptr && (!this->api_connection_->is_connection_setup() || !api_is_connected())) {
    ESP_LOGW(TAG, "Subscriber disconnected");
    this->unsubscribe_api_connection(this->api_connection_);
  }

  // Retry any client-bound frame that hit API TX buffer backpressure
  this->try_send_pending_client_frame_();

  // Send any queued client frames if the ASH TX window opened up
  this->drain_ncp_tx_queue_();

  // Autonomous recovery: with no client subscribed, periodically retry a failed NCP link
  // (or a failed boot harvest) so a late-powered NCP does not require a client RST
  if (this->api_connection_ == nullptr &&
      (this->ash_state_ == AshState::FAILED ||
       (this->boot_state_ == BootState::FAILED && !this->boot_sequence_active_)) &&
      millis() - this->last_recovery_attempt_ > RECOVERY_RETRY_INTERVAL_MS) {
    ESP_LOGI(TAG, "Attempting NCP recovery");
    this->last_recovery_attempt_ = millis();
    this->reset_ash_protocol_();
  }
}

void ZigbeeProxy::check_boot_timeouts_() {
  uint32_t total_elapsed = millis() - this->boot_start_time_;
  if (total_elapsed > BOOT_SEQUENCE_TIMEOUT_MS) {
    ESP_LOGE(TAG, "Boot sequence timeout (state: %d)", static_cast<int>(this->boot_state_));
    this->boot_state_ = BootState::FAILED;
    this->boot_sequence_active_ = false;
    // Still mark as connected so proxy can work without network info
    if (this->ash_state_ != AshState::CONNECTED) {
      this->ash_state_ = AshState::FAILED;
    }
  } else if ((this->boot_state_ == BootState::WAIT_RSTACK || this->boot_state_ == BootState::WAIT_FINAL_RSTACK) &&
             (millis() - this->setup_time_) > ASH_RESET_TIMEOUT) {
    // RST was sent before USB device finished enumeration — retry
    ESP_LOGD(TAG, "No RSTACK received within %u ms, retrying RST", ASH_RESET_TIMEOUT);
    this->setup_time_ = millis();
    this->send_rst_frame_();
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

bool ZigbeeProxy::can_proceed() {
  // Block setup only while the boot harvest is running so network info (IEEE address,
  // PAN ID) is ready when the API starts. check_boot_timeouts_() guarantees forward
  // progress: a dead NCP flips the sequence to FAILED after BOOT_SEQUENCE_TIMEOUT_MS and
  // the device boots normally (recovery then happens from loop()).
  if (!this->boot_sequence_active_) {
    return true;
  }

  // loop() is not called while setup is blocked, so run the boot machinery here
  this->process_uart_();
  if (this->tx_buffer_pending_ && this->check_ack_timeout_()) {
    this->handle_retransmission_();
  }
  this->check_boot_timeouts_();

  return !this->boot_sequence_active_;
}

void ZigbeeProxy::api_connection_authenticated(api::APIConnection *conn) {
  // Notify client of network info if available
  if (this->network_info_.valid) {
    this->send_network_info_changed_msg_(conn);
  }
}

void ZigbeeProxy::zigbee_proxy_request(api::APIConnection *api_connection, const api::ZigbeeProxyRequest &msg) {
  switch (msg.type) {
    case api::enums::ZIGBEE_PROXY_REQUEST_TYPE_SUBSCRIBE:
      if (this->api_connection_ != nullptr && this->api_connection_ != api_connection) {
        ESP_LOGW(TAG, "Another client is already subscribed");
        return;
      }
      ESP_LOGD(TAG, "Client subscribed");
      this->api_connection_ = api_connection;
      this->client_reset_session_();
      break;

    case api::enums::ZIGBEE_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      if (this->api_connection_ == api_connection) {
        ESP_LOGD(TAG, "Client unsubscribed");
        this->unsubscribe_api_connection(api_connection);
      }
      break;

    case api::enums::ZIGBEE_PROXY_REQUEST_TYPE_NETWORK_INFO:
      this->send_network_info_response_(api_connection);
      break;

    default:
      ESP_LOGW(TAG, "Unknown request type: %d", static_cast<int>(msg.type));
      break;
  }
}

void ZigbeeProxy::unsubscribe_api_connection(api::APIConnection *conn) {
  if (this->api_connection_ != conn) {
    return;
  }
  this->api_connection_ = nullptr;
  // Frames belonging to the departed client's session must not linger
  this->client_tx_pending_length_ = 0;
  this->ncp_tx_queue_count_ = 0;
  this->client_reset_session_();
}

void ZigbeeProxy::zigbee_proxy_frame(api::APIConnection *api_connection, const api::ZigbeeProxyFrame &msg) {
  if (this->api_connection_ != api_connection) {
    ESP_LOGW(TAG, "Frame received from non-subscribed client");
    return;
  }

  if (this->in_raw_relay_()) {
    // The NCP is in the bootloader, so the client is speaking its menu/XMODEM
    // protocol rather than ASH. Pass it straight through.
    this->write_array(msg.data, msg.data_len);
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

bool ZigbeeProxy::send_frame(const uint8_t *data, size_t length) {
  if (this->ash_state_ != AshState::CONNECTED) {
    ESP_LOGW(TAG, "Cannot send frame, not connected");
    return false;
  }

  // Transmit directly when the ASH window is free and nothing is queued ahead
  if (!this->tx_buffer_pending_ && this->ncp_tx_queue_count_ == 0) {
    return this->send_data_frame_(data, length, false);
  }

  // Window occupied - queue for transmission when the pending frame is ACKed
  if (this->ncp_tx_queue_count_ >= NCP_TX_QUEUE_SIZE || length > MAX_ASH_FRAME_SIZE) {
    return false;  // Caller NAKs the client, which retransmits
  }
  QueuedTxFrame &entry =
      this->ncp_tx_queue_[(this->ncp_tx_queue_head_ + this->ncp_tx_queue_count_) % NCP_TX_QUEUE_SIZE];
  memcpy(entry.data.data(), data, length);
  entry.length = length;
  this->ncp_tx_queue_count_++;
  ESP_LOGV(TAG, "Queued frame (%u bytes, %u queued)", length, this->ncp_tx_queue_count_);
  return true;
}

void ZigbeeProxy::drain_ncp_tx_queue_() {
  if (this->tx_buffer_pending_ || this->ncp_tx_queue_count_ == 0 || this->ash_state_ != AshState::CONNECTED) {
    return;
  }
  QueuedTxFrame &entry = this->ncp_tx_queue_[this->ncp_tx_queue_head_];
  this->ncp_tx_queue_head_ = (this->ncp_tx_queue_head_ + 1) % NCP_TX_QUEUE_SIZE;
  this->ncp_tx_queue_count_--;
  this->send_data_frame_(entry.data.data(), entry.length, false);
}

void ZigbeeProxy::set_timeout_config(uint32_t initial_ms, uint32_t min_ms, uint32_t max_ms) {
  this->timeout_config_.initial_timeout_ms = initial_ms;
  this->timeout_config_.min_timeout_ms = min_ms;
  this->timeout_config_.max_timeout_ms = max_ms;
  this->timeout_config_.current_timeout_ms = initial_ms;
  ESP_LOGV(TAG, "Timeout config updated: initial=%u, min=%u, max=%u", initial_ms, min_ms, max_ms);
}

#ifdef USE_ZIGBEE_PROXY_USB_UART
void ZigbeeProxy::set_usb_uart_channel(usb_uart::USBUartChannel *channel) {
  channel->set_rx_callback([this]() { this->process_uart_(); });
  ESP_LOGD(TAG, "Registered USB UART RX callback for low-latency processing");
}
#endif

// ASH Protocol State Machine
void ZigbeeProxy::reset_ash_protocol_() {
  ESP_LOGV(TAG, "Resetting ASH protocol");
  this->raw_buffer_index_ = 0;
  this->ash_state_ = AshState::CONNECTING;
  this->tx_sequence_ = 0;
  this->rx_sequence_ = 0;
  this->tx_buffer_pending_ = false;
  this->tx_retry_count_ = 0;
  this->ncp_tx_queue_count_ = 0;
  this->parsing_state_ = ParsingState::WAIT_FLAG_START;
  this->setup_time_ = millis();
  this->boot_start_time_ = this->setup_time_;

  // An NCP reset drops it back to legacy framing, so the extended-format version
  // handshake has to be redone before any other command is accepted.
  this->ezsp_version_ = 0;
  this->ezsp_version_confirmed_ = false;

  // Start boot sequence
  this->boot_state_ = BootState::WAIT_RSTACK;
  this->boot_sequence_active_ = true;
  this->ezsp_sequence_ = 0;

  this->send_rst_frame_();
}

void ZigbeeProxy::send_rst_frame_() {
  // Build a combined buffer: 32 CAN bytes followed immediately by the RST frame,
  // sent as a single write_array call. This ensures correct byte ordering and
  // minimizes the number of USB bulk transfers (all bytes fit in one USB FS packet).
  static constexpr uint8_t ASH_CAN_BYTE = 0x1A;
  static constexpr size_t CAN_COUNT = 32;
  static constexpr size_t MAX_RST_FRAME_SIZE = 8;
  uint8_t combined[CAN_COUNT + MAX_RST_FRAME_SIZE];
  memset(combined, ASH_CAN_BYTE, CAN_COUNT);
  size_t rst_len = this->build_frame_(combined + CAN_COUNT, MAX_RST_FRAME_SIZE, nullptr, 0, AshFrameType::RST);

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MAX_RST_FRAME_SIZE)];
#endif
  ESP_LOGV(TAG, "RST frame bytes (%u): %s", rst_len, format_hex_pretty_to(hex_buf, combined + CAN_COUNT, rst_len));
  this->write_array(combined, CAN_COUNT + rst_len);
  this->flush();
  ESP_LOGV(TAG, "Sent RST frame (with %u CAN bytes prefix)", CAN_COUNT);
}

void ZigbeeProxy::handle_rstack_frame_(const uint8_t *data, size_t length) {
  // Reset sequence numbers on any RSTACK
  this->tx_sequence_ = 0;
  this->rx_sequence_ = 0;
  this->clear_tx_buffer_();

  if (this->boot_state_ == BootState::WAIT_RSTACK) {
    // Initial RSTACK - start boot sequence
    ESP_LOGV(TAG, "Received RSTACK, starting EZSP initialization");
    this->ash_state_ = AshState::CONNECTED;

    // Drain any stale bytes that arrived before the RSTACK (e.g. leftover
    // UART FIFO bytes on HW UART, or a partial prior frame on USB CDC).
    // For USB CDC the input_buffer_ is already fully up-to-date at this point
    // (the RX callback just moved all pending chunks into it), so this loop
    // completes immediately rather than spinning with yield().
    while (this->available()) {
      uint8_t discard;
      this->read_byte(&discard);
      ESP_LOGV(TAG, "Draining post-RSTACK byte: 0x%02X", discard);
    }

    this->boot_state_ = BootState::SEND_VERSION;
    this->advance_boot_state_();
  } else if (this->boot_state_ == BootState::WAIT_FINAL_RSTACK) {
    // Final RSTACK after harvesting - boot complete
    ESP_LOGV(TAG, "Boot sequence complete, NCP reset to clean state");
    this->ash_state_ = AshState::CONNECTED;
    this->boot_state_ = BootState::COMPLETE;
    this->boot_sequence_active_ = false;

    // Now check for WiFi/Zigbee channel conflicts
    this->check_wifi_zigbee_conflict_();
  } else if (this->ash_state_ == AshState::CONNECTING) {
    // RSTACK during connecting (triggered by client RST forwarding)
    ESP_LOGV(TAG, "Received RSTACK, NCP ready");
    this->ash_state_ = AshState::CONNECTED;
    // Forward to client if subscribed
    if (this->api_connection_ != nullptr) {
      this->forward_ncp_rstack_to_client_(this->rx_buffer_.data() + 1, this->rx_buffer_index_ - 3);
    }
  } else if (this->api_connection_ != nullptr) {
    // Client is subscribed, forward RSTACK
    ESP_LOGV(TAG, "Forwarding RSTACK to client");
    this->ash_state_ = AshState::CONNECTED;
    this->forward_ncp_rstack_to_client_(this->rx_buffer_.data() + 1, this->rx_buffer_index_ - 3);
  } else {
    ESP_LOGW(TAG, "Unexpected RSTACK received (boot_state=%d)", static_cast<int>(this->boot_state_));
  }
}

void ZigbeeProxy::handle_error_frame_(const uint8_t *data, size_t length) {
  if (length < 1) {
    ESP_LOGE(TAG, "Frame too short");
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

  ESP_LOGE(TAG, "NCP error: %s (0x%02X)", error_str, error_code);

  if (this->api_connection_ != nullptr) {
    // Forward error to client
    this->forward_ncp_error_to_client_(data, length);
  } else {
    // No client, attempt recovery ourselves
    ESP_LOGV(TAG, "Attempting recovery");
    this->reset_ash_protocol_();
  }
}

bool ZigbeeProxy::send_ack_frame_(uint8_t ack_num) {
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, sizeof(frame), nullptr, 0, AshFrameType::ACK, 0, ack_num);
  this->write_array(frame, length);
  this->last_ack_sent_ = ack_num;
  ESP_LOGV(TAG, "Sent ACK for frame %d", ack_num);
  return true;
}

bool ZigbeeProxy::send_nak_frame_(uint8_t ack_num) {
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, sizeof(frame), nullptr, 0, AshFrameType::NAK, 0, ack_num);
  this->write_array(frame, length);
  ESP_LOGW(TAG, "Sent NAK for frame %d", ack_num);
  return true;
}

bool ZigbeeProxy::send_data_frame_(const uint8_t *data, size_t length, bool retransmit) {
  // Build frame (returns 0 if the stuffed frame would exceed the buffer)
  size_t frame_length = this->build_frame_(this->tx_buffer_.data(), this->tx_buffer_.size(), data, length,
                                           AshFrameType::DATA, this->tx_sequence_, this->rx_sequence_, retransmit);
  if (frame_length == 0) {
    return false;
  }

  // Store for potential retransmission
  if (!retransmit) {
    memcpy(this->tx_pending_buffer_.data(), this->tx_buffer_.data(), frame_length);
    this->tx_pending_length_ = frame_length;
    this->tx_pending_frame_num_ = this->tx_sequence_;
  }

  // Debug: log exact bytes being sent (truncated to ZIGBEE_MAX_LOG_BYTES)
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(ZIGBEE_MAX_LOG_BYTES)];
#endif
  ESP_LOGV(TAG, "TX DATA frame (%u bytes): %s", frame_length,
           format_hex_pretty_to(hex_buf, this->tx_buffer_.data(), frame_length));

  // Send frame
  this->write_array(this->tx_buffer_.data(), frame_length);

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
    ESP_LOGE(TAG, "Max retries exceeded");
    this->ash_state_ = AshState::FAILED;
    this->clear_tx_buffer_();
    return;
  }

  ESP_LOGW(TAG, "Retransmitting frame %d (attempt %d/%d)", this->tx_pending_frame_num_, this->tx_retry_count_,
           ASH_MAX_RETRIES);

  // Resend the pending frame
  this->write_array(this->tx_pending_buffer_.data(), this->tx_pending_length_);
  this->start_ack_timer_();
}

// Boot-time NCP initialization sequence
// Sequence: RST -> RSTACK -> version() -> networkInit() -> stackStatus ->
//           getNetworkParameters() -> getEui64() -> RST -> RSTACK
//
// getEui64 comes last, after networkInit has brought the stack up: asking earlier
// makes the NCP answer with error frame 0x0058 instead of the address. bellows
// orders it the same way.

void ZigbeeProxy::advance_boot_state_() {
  switch (this->boot_state_) {
    case BootState::SEND_VERSION:
      this->send_ezsp_version_();
      this->boot_state_ = BootState::WAIT_VERSION;
      break;

    case BootState::SEND_GET_EUI64:
      this->send_get_eui64_();
      this->boot_state_ = BootState::WAIT_EUI64;
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
      ESP_LOGV(TAG, "Sending final RST to reset NCP to clean state");
      this->boot_state_ = BootState::WAIT_FINAL_RSTACK;
      this->send_rst_frame_();
      break;

    default:
      break;
  }
}

void ZigbeeProxy::handle_boot_data_frame_(const uint8_t *data, size_t length) {
  // EZSP frame format is decided solely by whether version negotiation has
  // completed, never by the frame's length:
  // Legacy: [sequence] [frame_control] [frame_id] [data...]  (3-byte header)
  // Extended (v13+): [sequence] [frame_control_low] [frame_control_high] [frame_id_low] [frame_id_high] [data...]
  //
  // Only the `version` response arrives in legacy format, before ezsp_version_ is
  // set. Inferring the format from the length instead misparses a short extended
  // frame -- an error reply, say -- as a legacy one, which then surfaces as a
  // plausible but wrong payload rather than as a protocol error.

  if (length < 3) {
    ESP_LOGW(TAG, "Boot frame too short: %u bytes", length);
    return;
  }

  uint8_t frame_control;
  uint16_t frame_id;
  const uint8_t *payload;
  size_t payload_length;

  bool use_extended = this->ezsp_version_ >= EZSP_MIN_VERSION;

  if (use_extended && length < 5) {
    ESP_LOGW(TAG, "Extended EZSP frame too short: %u bytes", length);
    return;
  }

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

    case BootState::WAIT_EUI64:
      if (frame_id == EZSP_GET_EUI64 && is_response) {
        this->handle_eui64_response_(payload, payload_length);
      }
      break;

    case BootState::WAIT_STACK_STATUS:
      if (frame_id == EZSP_STACK_STATUS_HANDLER && is_callback) {
        this->handle_stack_status_(payload, payload_length);
      } else if (frame_id == EZSP_NETWORK_INIT && is_response) {
        // networkInit response contains a 32-bit sl_status_t
        // Some NCPs proceed directly without stackStatusHandler callback
        if (payload_length >= 1) {
          uint8_t status = payload[0];
          ESP_LOGV(TAG, "networkInit response: status=0x%02X", status);
          // Proceed to getNetworkParameters regardless of status
          // getNetworkParameters will tell us if there's actually a network
          ESP_LOGV(TAG, "networkInit complete, querying network parameters");
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

  ash_randomize(cmd, sizeof(cmd));
  ESP_LOGV(TAG, "Sending EZSP version command (legacy format, requesting v%d)", EZSP_MAX_VERSION);
  this->send_data_frame_(cmd, sizeof(cmd), false);
}

void ZigbeeProxy::send_get_eui64_() {
  // Extended format: version negotiation has completed, so the NCP now requires
  // the 16-bit frame control and 16-bit frame ID.
  uint8_t cmd[] = {
      this->ezsp_sequence_++,        // Sequence
      EZSP_FRAME_CONTROL_COMMAND,    // Frame control (low)
      EZSP_FRAME_CONTROL_EXTENDED,   // Frame control (high)
      EZSP_GET_EUI64 & 0xFF,         // Frame ID (low)
      (EZSP_GET_EUI64 >> 8) & 0xFF,  // Frame ID (high)
  };
  ash_randomize(cmd, sizeof(cmd));
  ESP_LOGV(TAG, "Sending EZSP getEui64 command");
  this->send_data_frame_(cmd, sizeof(cmd), false);
}

void ZigbeeProxy::send_network_init_() {
  // networkInitStruct: [bitmask (2 bytes)] - use 0x0000 for default
  uint8_t cmd[] = {
      this->ezsp_sequence_++,           // Sequence
      EZSP_FRAME_CONTROL_COMMAND,       // Frame control (low)
      EZSP_FRAME_CONTROL_EXTENDED,      // Frame control (high)
      EZSP_NETWORK_INIT & 0xFF,         // Frame ID (low)
      (EZSP_NETWORK_INIT >> 8) & 0xFF,  // Frame ID (high)
      0x00,                             // networkInitStruct bitmask (default)
      0x00,
  };
  ash_randomize(cmd, sizeof(cmd));
  ESP_LOGV(TAG, "Sending EZSP networkInit command");
  this->send_data_frame_(cmd, sizeof(cmd), false);
}

void ZigbeeProxy::send_get_network_params_() {
  uint8_t cmd[] = {
      this->ezsp_sequence_++,                      // Sequence
      EZSP_FRAME_CONTROL_COMMAND,                  // Frame control (low)
      EZSP_FRAME_CONTROL_EXTENDED,                 // Frame control (high)
      EZSP_GET_NETWORK_PARAMETERS & 0xFF,          // Frame ID (low)
      (EZSP_GET_NETWORK_PARAMETERS >> 8) & 0xFF,   // Frame ID (high)
  };
  ash_randomize(cmd, sizeof(cmd));
  ESP_LOGV(TAG, "Sending EZSP getNetworkParameters command");
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
      ESP_LOGV(TAG, "NCP accepted EZSP v%d", ncp_version);
      this->ezsp_version_ = ncp_version;
      this->boot_state_ = BootState::SEND_NETWORK_INIT;
      this->advance_boot_state_();
      return;
    }

    // NCP doesn't support our version - re-negotiate
    ESP_LOGV(TAG, "NCP supports EZSP v%d, re-negotiating", ncp_version);
    this->ezsp_requested_version_ = ncp_version;

    // Re-send version command with NCP's supported version
    // Use legacy format for re-negotiation (NCP stays in legacy until handshake completes)
    uint8_t cmd[] = {
        this->ezsp_sequence_++,      // Sequence
        EZSP_FRAME_CONTROL_COMMAND,  // Frame control
        0x00,                        // Frame ID (version)
        ncp_version                  // Use NCP's version
    };
    ash_randomize(cmd, sizeof(cmd));
    ESP_LOGV(TAG, "Re-sending EZSP version command (requesting v%d)", ncp_version);
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

  ESP_LOGD(TAG, "NCP EZSP version: %d, stack type: %d, stack version: 0x%04X", this->ezsp_version_, stack_type,
           stack_version);

  if (this->ezsp_version_ < EZSP_MIN_VERSION) {
    ESP_LOGE(TAG, "EZSP version %d not supported (minimum: %d)", this->ezsp_version_, EZSP_MIN_VERSION);
    this->boot_state_ = BootState::FAILED;
    return;
  }

  if (!this->ezsp_version_confirmed_) {
    // Repeat `version` in the negotiated extended format. The NCP answers the
    // initial legacy command with its own version, but keeps rejecting extended
    // frames (error frame 0x0058) until the handshake is completed in that format.
    this->ezsp_version_confirmed_ = true;
    this->ezsp_requested_version_ = this->ezsp_version_;

    uint8_t cmd[] = {
        this->ezsp_sequence_++,       // Sequence
        EZSP_FRAME_CONTROL_COMMAND,   // Frame control (low)
        EZSP_FRAME_CONTROL_EXTENDED,  // Frame control (high)
        EZSP_VERSION & 0xFF,          // Frame ID (low)
        (EZSP_VERSION >> 8) & 0xFF,   // Frame ID (high)
        this->ezsp_version_,          // desiredProtocolVersion
    };
    ash_randomize(cmd, sizeof(cmd));
    ESP_LOGV(TAG, "Confirming EZSP v%d in extended format", this->ezsp_version_);
    this->send_data_frame_(cmd, sizeof(cmd), false);
    // Stay in WAIT_VERSION for the confirmation response
    return;
  }

  this->boot_state_ = BootState::SEND_NETWORK_INIT;
  this->advance_boot_state_();
}

void ZigbeeProxy::handle_eui64_response_(const uint8_t *data, size_t length) {
  // getEui64 response: [eui64 (8 bytes, little-endian)]
  if (length >= ZIGBEE_IEEE_ADDR_SIZE) {
    this->set_ieee_address_(data);
  } else {
    ESP_LOGW(TAG, "getEui64 response too short: %u bytes", length);
  }
  // Harvest is done either way; the proxy works without an IEEE address
  this->boot_state_ = BootState::SEND_FINAL_RST;
  this->advance_boot_state_();
}

void ZigbeeProxy::handle_stack_status_(const uint8_t *data, size_t length) {
  // stackStatusHandler callback: [status]
  if (length < 1) {
    ESP_LOGW(TAG, "Stack status too short");
    return;
  }

  uint8_t status = data[0];
  ESP_LOGV(TAG, "Stack status: 0x%02X", status);

  // Check for network up status
  if (status == static_cast<uint8_t>(SlStatus::NETWORK_UP) || status == static_cast<uint8_t>(SlStatus::OK)) {
    ESP_LOGV(TAG, "Network is up, querying parameters");
    this->boot_state_ = BootState::SEND_GET_NETWORK_PARAMS;
    this->advance_boot_state_();
  } else if (status == static_cast<uint8_t>(SlStatus::NOT_JOINED)) {
    // No network configured, so there are no parameters to read -- but still read
    // the EUI64. It is a hardware address that exists regardless of membership, and
    // it is what lets a client tell an unformed radio apart from an unreachable one.
    ESP_LOGD(TAG, "No network configured on NCP");
    this->boot_state_ = BootState::SEND_GET_EUI64;
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
  // A 1-byte response (status only) is normal when the NCP has no network configured.
  if (length < NETWORK_PARAMS_RESPONSE_SIZE) {
    if (length <= SL_STATUS_SIZE) {
      ESP_LOGD(TAG, "NCP has no network configured (status=0x%02X)", data[0]);
    } else {
      ESP_LOGW(TAG, "getNetworkParameters response unexpected length: %u bytes", length);
    }
    this->boot_state_ = BootState::SEND_GET_EUI64;
    this->advance_boot_state_();
    return;
  }

  uint8_t status = data[NETWORK_PARAMS_STATUS_OFFSET];
  if (status != static_cast<uint8_t>(SlStatus::OK)) {
    ESP_LOGW(TAG, "getNetworkParameters failed with status: 0x%02X", status);
    this->boot_state_ = BootState::SEND_GET_EUI64;
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
  this->send_network_info_changed_msg_();

  ESP_LOGD(TAG,
           "Network info:\n"
           "  Extended PAN ID: %02X%02X%02X%02X%02X%02X%02X%02X\n"
           "  PAN ID: 0x%04X\n"
           "  Channel: %u",
           this->network_info_.extended_pan_id[7], this->network_info_.extended_pan_id[6],
           this->network_info_.extended_pan_id[5], this->network_info_.extended_pan_id[4],
           this->network_info_.extended_pan_id[3], this->network_info_.extended_pan_id[2],
           this->network_info_.extended_pan_id[1], this->network_info_.extended_pan_id[0], this->network_info_.pan_id,
           this->network_info_.channel);

  // The stack is up, so the EUI64 can finally be read
  this->boot_state_ = BootState::SEND_GET_EUI64;
  this->advance_boot_state_();
}

bool ZigbeeProxy::set_ieee_address_(const uint8_t *new_address) {
  bool changed = memcmp(this->network_info_.ieee_address.data(), new_address, ZIGBEE_IEEE_ADDR_SIZE) != 0;

  if (changed) {
    memcpy(this->network_info_.ieee_address.data(), new_address, ZIGBEE_IEEE_ADDR_SIZE);
    this->network_info_.valid = true;
    ESP_LOGD(TAG, "IEEE address updated: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", new_address[7], new_address[6],
             new_address[5], new_address[4], new_address[3], new_address[2], new_address[1], new_address[0]);
    this->send_network_info_changed_msg_();
    return true;
  }

  return false;
}

// Packed network info payload: ieee(8) + extended_pan(8) + pan_id(2) + channel(1), little-endian
static void pack_network_info(const NetworkInfo &info, uint8_t *out) {
  memcpy(out, info.ieee_address.data(), ZIGBEE_IEEE_ADDR_SIZE);
  memcpy(out + 8, info.extended_pan_id.data(), 8);
  out[16] = info.pan_id & 0xFF;
  out[17] = (info.pan_id >> 8) & 0xFF;
  out[18] = info.channel;
}

void ZigbeeProxy::send_network_info_changed_msg_(api::APIConnection *conn) {
  uint8_t payload[NETWORK_INFO_PAYLOAD_SIZE];
  pack_network_info(this->network_info_, payload);
  api::ZigbeeProxyRequest msg;
  msg.type = api::enums::ZIGBEE_PROXY_REQUEST_TYPE_NETWORK_INFO;
  msg.data = payload;
  msg.data_len = sizeof(payload);
  if (conn != nullptr) {
    conn->send_message(msg);
  } else if (api::global_api_server != nullptr) {
    // Very infrequent and small - send to all clients rather than tracking a subscription
    api::global_api_server->on_zigbee_proxy_request(msg);
  }
}

void ZigbeeProxy::send_network_info_response_(api::APIConnection *conn) { this->send_network_info_changed_msg_(conn); }

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
    ESP_LOGV(TAG, "No WiFi/Zigbee channel conflict (WiFi: %u, Zigbee: %u)", wifi_channel, zigbee_channel);
  }
#endif
}

// Bootloader detection, fed consecutive raw byte pairs. The Gecko bootloader speaks
// neither ASH nor EZSP, so once detected the NCP link is relayed verbatim in both
// directions (see process_uart_slow_ and zigbee_proxy_frame) and normal ASH parsing
// is suspended -- a flasher needs the raw menu and XMODEM streams to reach the client.
void ZigbeeProxy::check_bootloader_mode_(uint8_t prev_byte, uint8_t byte) {
  // An ASH RSTACK means the application is running again, so ASH resumes. This is the
  // only way out of the relay: while it is active nothing else parses the NCP stream.
  if (prev_byte == ASH_FLAG_BYTE && byte == static_cast<uint8_t>(AshFrameType::RSTACK)) {
    if (this->in_raw_relay_()) {
      ESP_LOGI(TAG, "NCP returned to application mode, resuming ASH");
      this->bootloader_state_ = BootloaderState::NORMAL;
      // Clear the failure that put us in the relay, so the RSTACK below is parsed
      // as ASH and the link comes back up on its own.
      this->ash_state_ = AshState::CONNECTING;
      this->parsing_state_ = ParsingState::WAIT_FLAG_START;
      this->raw_buffer_index_ = 0;
    }
    return;
  }

  // Silicon Labs bootloader menu prompt (0xC1 0x0D)
  if (prev_byte == 0xC1 && byte == 0x0D) {
    if (this->bootloader_state_ != BootloaderState::MENU) {
      ESP_LOGW(TAG, "NCP in bootloader menu mode, relaying raw bytes to client");
      this->bootloader_state_ = BootloaderState::MENU;
    }
    return;
  }

  // XMODEM-CRC poll ('C'), only meaningful once the bootloader is already talking:
  // treating it as an entry condition on its own would false-positive on ASH payload.
  if (byte == 0x43 && this->bootloader_state_ == BootloaderState::MENU) {
    ESP_LOGW(TAG, "NCP bootloader upload mode detected");
    this->bootloader_state_ = BootloaderState::DETECTED;
    return;
  }
}

// The NCP is not speaking ASH: either it is in its bootloader, or the ASH link gave
// up entirely. Relaying raw in both cases is what lets a flasher reach -- and recover
// -- a device that is already sitting in the bootloader when the proxy starts.
bool ZigbeeProxy::in_raw_relay_() const {
  return this->bootloader_state_ != BootloaderState::NORMAL || this->ash_state_ == AshState::FAILED;
}

// True if a client-bound EZSP payload is `launchStandaloneBootloader`. The payload is
// still randomized here (it is forwarded to the NCP untouched), so only the frame ID
// bytes are unmasked, using the fixed prefix of the ASH pseudo-random sequence.
bool ZigbeeProxy::is_launch_bootloader_command_(const uint8_t *payload, size_t length) {
  // Extended EZSP header: [seq] [fc_lo] [fc_hi] [id_lo] [id_hi]
  if (length < 5) {
    return false;
  }

  uint8_t header[5];
  memcpy(header, payload, sizeof(header));
  ash_randomize(header, sizeof(header));

  // Commands only; a response or callback carrying the same ID is the NCP's reply
  if ((header[1] & (EZSP_FRAME_CONTROL_RESPONSE | EZSP_FRAME_CONTROL_CALLBACK)) != 0) {
    return false;
  }

  uint16_t frame_id = header[3] | (static_cast<uint16_t>(header[4]) << 8);
  return frame_id == EZSP_LAUNCH_STANDALONE_BOOTLOADER;
}

// Relay raw NCP bytes to the client, batching them so an XMODEM transfer does not
// become one API message per byte. Flushed when the UART drains or the buffer fills.
void ZigbeeProxy::queue_raw_to_client_(uint8_t byte) {
  if (this->api_connection_ == nullptr) {
    return;
  }

  this->raw_buffer_[this->raw_buffer_index_++] = byte;
  if (this->raw_buffer_index_ >= this->raw_buffer_.size()) {
    this->flush_raw_to_client_();
  }
}

void ZigbeeProxy::flush_raw_to_client_() {
  if (this->raw_buffer_index_ == 0) {
    return;
  }

  // Best effort: the bootloader protocols carry their own retries, and there is no
  // ASH session to resynchronize here.
  if (!this->send_to_client_(this->raw_buffer_.data(), this->raw_buffer_index_)) {
    ESP_LOGW(TAG, "Dropped %u raw bytes to client (API TX buffer full)", this->raw_buffer_index_);
  }
  this->raw_buffer_index_ = 0;
}

// UART processing (precondition: available() > 0, see inline process_uart_ in the header)
void ZigbeeProxy::process_uart_slow_() {
  do {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      return;
    }

    // Verbose logging for debugging (ESP_LOGV already checks log level)
    ESP_LOGV(TAG, "RX: 0x%02X", byte);

    // Runs unconditionally: a client can launch the bootloader over EZSP while the
    // ASH link is up, so detection cannot be gated on the link being down.
    this->check_bootloader_mode_(this->last_rx_byte_, byte);
    this->last_rx_byte_ = byte;

    if (this->in_raw_relay_()) {
      // Bootloader traffic is not ASH. Relay it untouched and keep the parser out of
      // it: feeding menu text or XMODEM to parse_byte_ would fail CRC and NAK the NCP
      // mid-transfer.
      this->queue_raw_to_client_(byte);
      continue;
    }

    this->parse_byte_(byte);
  } while (this->available());

  this->flush_raw_to_client_();
}

// ==================== Client-side ASH session ====================

void ZigbeeProxy::client_reset_session_() {
  this->client_tx_sequence_ = 0;
  this->client_rx_sequence_ = 0;
  this->client_rx_buffer_index_ = 0;
  this->client_escape_next_byte_ = false;
  this->client_tx_pending_length_ = 0;
  this->client_ash_state_ = AshState::DISCONNECTED;
  this->client_parsing_state_ = ParsingState::WAIT_FLAG_START;
  ESP_LOGV(TAG, "Client ASH session reset");
}

bool ZigbeeProxy::send_to_client_(const uint8_t *data, size_t length) {
  if (this->api_connection_ == nullptr) {
    return false;
  }
  this->outgoing_proto_msg_.data = data;
  this->outgoing_proto_msg_.data_len = length;
  return this->api_connection_->send_zigbee_proxy_frame(this->outgoing_proto_msg_);
}

void ZigbeeProxy::client_send_raw_frame_(const uint8_t *frame, size_t length) {
  // Failure here means API TX buffer backpressure. Losing a control frame is recoverable:
  // an unsent ACK triggers a client retransmit, which the duplicate-frame path re-ACKs.
  if (!this->send_to_client_(frame, length)) {
    ESP_LOGV(TAG, "Dropped %u byte control frame to client (API TX buffer full)", length);
  }
}

void ZigbeeProxy::client_send_ack_frame_(uint8_t ack_num) {
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, sizeof(frame), nullptr, 0, AshFrameType::ACK, 0, ack_num);
  this->client_send_raw_frame_(frame, length);
  ESP_LOGV(TAG, "Sent client ACK for frame %d", ack_num);
}

void ZigbeeProxy::client_send_nak_frame_(uint8_t ack_num) {
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, sizeof(frame), nullptr, 0, AshFrameType::NAK, 0, ack_num);
  this->client_send_raw_frame_(frame, length);
  ESP_LOGV(TAG, "Sent client NAK for frame %d", ack_num);
}

void ZigbeeProxy::client_send_rstack_frame_(uint8_t reset_code) {
  // RSTACK payload: [version] [reset_code]
  uint8_t payload[] = {0x02, reset_code};
  uint8_t frame[16];
  size_t length = this->build_frame_(frame, sizeof(frame), payload, sizeof(payload), AshFrameType::RSTACK);
  this->client_send_raw_frame_(frame, length);
  ESP_LOGV(TAG, "Sent client RSTACK (code=0x%02X)", reset_code);
}

void ZigbeeProxy::client_send_data_frame_(const uint8_t *data, size_t length) {
  size_t frame_length = this->build_frame_(this->client_tx_buffer_.data(), this->client_tx_buffer_.size(), data, length,
                                           AshFrameType::DATA, this->client_tx_sequence_, this->client_rx_sequence_);
  if (frame_length == 0) {
    return;  // build_frame_ logged the error; payload cannot be represented in the buffer
  }
  this->client_tx_sequence_ = (this->client_tx_sequence_ + 1) & ASH_MAX_SEQUENCE;
  if (!this->send_to_client_(this->client_tx_buffer_.data(), frame_length)) {
    // API TX buffer backpressure: keep the frame and retry from loop(). While a frame is
    // pending, incoming NCP DATA frames are left unACKed so the NCP provides flow control.
    ESP_LOGV(TAG, "Client DATA frame deferred (API TX buffer full)");
    this->client_tx_pending_length_ = frame_length;
    this->client_tx_pending_since_ = millis();
    return;
  }
  ESP_LOGV(TAG, "Sent client DATA frame, payload %u bytes", length);
}

void ZigbeeProxy::try_send_pending_client_frame_() {
  if (this->client_tx_pending_length_ == 0) {
    return;
  }
  if (this->send_to_client_(this->client_tx_buffer_.data(), this->client_tx_pending_length_)) {
    ESP_LOGV(TAG, "Sent deferred client DATA frame");
    this->client_tx_pending_length_ = 0;
    return;
  }
  if (millis() - this->client_tx_pending_since_ > CLIENT_TX_RETRY_TIMEOUT_MS) {
    // The API connection is not draining; abandon the frame and force the client to
    // re-establish a clean ASH session (best effort - the ERROR frame may also fail)
    ESP_LOGE(TAG, "Client TX stalled for %u ms, resetting client session", CLIENT_TX_RETRY_TIMEOUT_MS);
    this->client_tx_pending_length_ = 0;
    this->client_send_error_frame_(static_cast<uint8_t>(EzspError::EXCEEDED_MAXIMUM_ACK_TIMEOUT_COUNT));
    this->client_reset_session_();
  }
}

void ZigbeeProxy::client_send_error_frame_(uint8_t error_code) {
  uint8_t payload[] = {0x02, error_code};
  uint8_t frame[16];
  size_t length = this->build_frame_(frame, sizeof(frame), payload, sizeof(payload), AshFrameType::ERROR);
  this->client_send_raw_frame_(frame, length);
  ESP_LOGV(TAG, "Sent client ERROR (code=0x%02X)", error_code);
}

void ZigbeeProxy::forward_ncp_data_to_client_(const uint8_t *payload, size_t length) {
  this->client_send_data_frame_(payload, length);
}

void ZigbeeProxy::forward_ncp_rstack_to_client_(const uint8_t *data, size_t length) {
  // RSTACK payload is [version] [reset_code] per spec; cap defensively so a malformed
  // NCP frame cannot overflow the stack buffer
  length = std::min(length, static_cast<size_t>(2));
  uint8_t frame[16];
  size_t frame_length = this->build_frame_(frame, sizeof(frame), data, length, AshFrameType::RSTACK);
  this->client_send_raw_frame_(frame, frame_length);

  // Reset client-side sequence numbers since RSTACK means new session
  this->client_tx_sequence_ = 0;
  this->client_rx_sequence_ = 0;
  this->client_tx_pending_length_ = 0;
  this->client_ash_state_ = AshState::CONNECTED;
  ESP_LOGV(TAG, "Forwarded RSTACK to client");
}

void ZigbeeProxy::forward_ncp_error_to_client_(const uint8_t *data, size_t length) {
  // ERROR payload is [version] [error_code] per spec; cap defensively (see RSTACK above)
  length = std::min(length, static_cast<size_t>(2));
  uint8_t frame[16];
  size_t frame_length = this->build_frame_(frame, sizeof(frame), data, length, AshFrameType::ERROR);
  this->client_send_raw_frame_(frame, frame_length);
  ESP_LOGV(TAG, "Forwarded ERROR to client");
}

void ZigbeeProxy::client_parse_byte_(uint8_t byte) {
  static constexpr uint8_t ASH_CAN_BYTE = 0x1A;
  static constexpr uint8_t ASH_XON_BYTE = 0x11;
  static constexpr uint8_t ASH_XOFF_BYTE = 0x13;

  // Reserved bytes count only when bare, never as the second half of an escape
  // sequence (see the matching comment in parse_byte_).
  if (!this->client_escape_next_byte_) {
    if (byte == ASH_CAN_BYTE) {
      // Cancel: discard any partial frame
      this->client_rx_buffer_index_ = 0;
      this->client_parsing_state_ = ParsingState::WAIT_FLAG_START;
      return;
    }
    if (byte == ASH_XON_BYTE || byte == ASH_XOFF_BYTE) {
      // Flow control: not part of any frame
      return;
    }
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
        uint8_t retx_bit = this->client_rx_buffer_[0] & 0x08;
        if (retx_bit != 0 && frame_num == ((this->client_rx_sequence_ - 1) & ASH_MAX_SEQUENCE)) {
          // Retransmission of a frame we already accepted (our ACK was lost) - re-ACK and discard
          ESP_LOGV(TAG, "Client: duplicate DATA frame %d, re-sending ACK", frame_num);
          this->client_send_ack_frame_(this->client_rx_sequence_);
        } else {
          ESP_LOGW(TAG, "Client: out of sequence DATA frame: expected %d, got %d", this->client_rx_sequence_,
                   frame_num);
          this->client_send_nak_frame_(this->client_rx_sequence_);
        }
        return;
      }

      // Extract EZSP payload (skip control byte, exclude CRC)
      size_t payload_length = this->client_rx_buffer_index_ > 3 ? this->client_rx_buffer_index_ - 3 : 0;
      const uint8_t *payload = this->client_rx_buffer_.data() + 1;

      if (payload_length > 0) {
        // Forward EZSP payload to NCP via right-side ASH; NAK without consuming if the
        // NCP link is down or the TX queue is full so the client retransmits
        ESP_LOGV(TAG, "Client DATA → NCP, EZSP payload %u bytes", payload_length);
        bool launching_bootloader = is_launch_bootloader_command_(payload, payload_length);
        if (!this->send_frame(payload, payload_length)) {
          this->client_send_nak_frame_(this->client_rx_sequence_);
          return;
        }

        if (launching_bootloader) {
          // The NCP is about to reboot into its bootloader, which speaks neither ASH
          // nor EZSP. Switch to the raw relay now: waiting to recognize bootloader
          // output would deadlock, since that output only appears once the client's
          // (non-ASH) bytes reach the NCP, and those are exactly what the relay
          // carries. Cleared again when an ASH RSTACK shows the app is back.
          ESP_LOGI(TAG, "Client launched NCP bootloader, relaying raw bytes");
          this->bootloader_state_ = BootloaderState::MENU;
        }
      }

      // Accepted: advance the sequence and ACK immediately rather than relying on the
      // piggybacked ACK of an eventual NCP response
      this->client_rx_sequence_ = (this->client_rx_sequence_ + 1) & ASH_MAX_SEQUENCE;
      this->client_send_ack_frame_(this->client_rx_sequence_);
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
      ESP_LOGV(TAG, "Client RST → forwarding to NCP");
      this->ash_state_ = AshState::CONNECTING;
      this->setup_time_ = millis();  // Reset timeout reference for RSTACK wait
      this->tx_sequence_ = 0;
      this->rx_sequence_ = 0;
      this->tx_buffer_pending_ = false;
      this->tx_retry_count_ = 0;
      this->ncp_tx_queue_count_ = 0;
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

}  // namespace esphome::zigbee_proxy

#endif  // USE_ZIGBEE_PROXY
