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
  // Remember the configured line rate. Another device sharing this UART may change it
  // (a flasher stepping through baud rates to reach a bootloader, say) and has no way
  // to know what to restore, so we put it back ourselves when we take the bus again.
  this->configured_baud_rate_ = this->parent_->get_baud_rate();

  // Initialize state
  this->ash_state_ = AshState::DISCONNECTED;
  this->parsing_state_ = ParsingState::WAIT_FLAG_START;
  this->tx_sequence_ = 0;
  this->rx_sequence_ = 0;

  // Send RST frame to initialize NCP
  this->reset_ash_protocol_();
}

void ZigbeeProxy::loop() {
  // Own the UART only while harvesting network info or serving a subscriber. Idling on
  // the bus otherwise would fight whichever device holds it -- a serial proxy carrying a
  // firmware update, say -- and the autonomous recovery below would inject ASH resets
  // into the middle of someone else's transfer.
  if (!this->should_own_uart_()) {
    if (this->owns_uart_) {
      this->owns_uart_ = false;
      if (this->api_connection_ != nullptr) {
        ESP_LOGW(TAG, "UART claimed by another device, dropping subscriber");
        this->unsubscribe_api_connection(this->api_connection_);
      }
      ESP_LOGD(TAG, "Released UART");
      this->boot_sequence_active_ = false;
      // The NCP may be reflashed while we are away, so nothing about the link can be
      // assumed on return.
      this->ash_state_ = AshState::FAILED;
    }
    return;
  }

  if (!this->owns_uart_) {
    ESP_LOGI(TAG, "Acquired UART, resetting NCP link");
    this->owns_uart_ = true;
    if (this->parent_->get_baud_rate() != this->configured_baud_rate_) {
      ESP_LOGI(TAG, "Restoring baud rate %" PRIu32 " (was %" PRIu32 ")", this->configured_baud_rate_,
               this->parent_->get_baud_rate());
      this->parent_->set_baud_rate(this->configured_baud_rate_);
      this->parent_->load_settings(false);
    }
    // A subscriber drives its own session: it opens with an RST and negotiates its own
    // EZSP version. Harvesting here would put a second RST on the wire alongside the
    // client's and renegotiate the NCP underneath it, so only harvest when there is
    // something left to learn.
    if (this->api_connection_ != nullptr && this->network_info_.valid) {
      this->reset_ncp_link_();
    } else {
      this->reset_ash_protocol_();
    }
  }

  // Process incoming UART data
  this->process_uart_();

  // Check for ACK timeout and handle retransmission
  if (this->tx_buffer_pending_ && this->check_ack_timeout_()) {
    this->handle_retransmission_();
  }

  if (this->boot_sequence_active_) {
    this->check_boot_timeouts_();
  } else if (this->api_connection_ == nullptr && this->ash_state_ == AshState::CONNECTING &&
             millis() - this->setup_time_ > ASH_RESET_TIMEOUT) {
    ESP_LOGE(TAG, "RSTACK timeout, NCP not responding");
    this->ash_state_ = AshState::FAILED;
  }

  // Guard against a subscriber that disconnected without unsubscribing
  if (this->api_connection_ != nullptr && (!this->api_connection_->is_connection_setup() || !api_is_connected())) {
    ESP_LOGW(TAG, "Subscriber disconnected");
    this->unsubscribe_api_connection(this->api_connection_);
  }

  // No autonomous recovery while a client is subscribed. A subscriber owns the link: it
  // opens with its own RST and resets whenever it decides it needs to. Resetting on its
  // behalf relays an RSTACK it never asked for, which bellows treats as fatal -- and if it
  // happens to be driving a bootloader over this interface, injecting ASH into the
  // transfer is worse still. A broken link is the client's to notice and repair.
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
        // A living subscriber keeps exclusive access. Its connection may be dead without
        // loop() having noticed yet (e.g. the client crashed and reconnected quickly);
        // in that case let the new client take over instead of locking it out for the
        // full API keepalive timeout.
        if (this->api_connection_->is_connection_setup()) {
          ESP_LOGW(TAG, "Another client is already subscribed");
          return;
        }
        ESP_LOGW(TAG, "Previous subscriber disconnected; taking over subscription");
      }
      ESP_LOGD(TAG, "Client subscribed");
      this->api_connection_ = api_connection;
      // A subscriber owns the link from here on, so abandon any harvest in flight rather
      // than interleaving our own EZSP commands with the client's session. Metadata for
      // this session comes from watching the client's own traffic instead.
      if (this->boot_sequence_active_) {
        ESP_LOGD(TAG, "Abandoning boot harvest, client owns the link");
        this->boot_sequence_active_ = false;
        this->boot_state_ = BootState::IDLE;
      }
      this->detector_.reset();
      break;

    case api::enums::ZIGBEE_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      if (this->api_connection_ == api_connection) {
        ESP_LOGD(TAG, "Client unsubscribed");
        this->unsubscribe_api_connection(api_connection);
      }
      break;

    case api::enums::ZIGBEE_PROXY_REQUEST_TYPE_NETWORK_INFO:
      this->send_network_info_changed_msg_(api_connection);
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
  // Anything already buffered belongs to the departed session
  this->relay_length_ = 0;
  this->detector_.reset();
}

void ZigbeeProxy::zigbee_proxy_frame(api::APIConnection *api_connection, const api::ZigbeeProxyFrame &msg) {
  if (this->api_connection_ != api_connection) {
    ESP_LOGW(TAG, "Frame received from non-subscribed client");
    return;
  }

  // Transparent relay: the client's ASH bytes reach the NCP untouched, so the two share
  // one sequence space and nothing here can desynchronize it.
  this->write_array(msg.data, msg.data_len);

  // Scanning this direction only matters while waiting for the version command that
  // completes the handshake. Outside that window it is a pure passthrough -- which is
  // what makes a firmware upload, all of which flows this way, essentially free.
  if (this->detector_.needs_host_scan()) {
    for (size_t i = 0; i < msg.data_len; i++) {
      this->detector_.from_host(msg.data[i]);
    }
  }
}

uint64_t ZigbeeProxy::get_ieee_address() const {
  uint64_t addr = 0;
  for (size_t i = 0; i < ZIGBEE_IEEE_ADDR_SIZE; i++) {
    addr |= static_cast<uint64_t>(this->network_info_.ieee_address[i]) << (i * 8);
  }
  return addr;
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
  this->ash_state_ = AshState::CONNECTING;
  this->tx_sequence_ = 0;
  this->rx_sequence_ = 0;
  this->tx_buffer_pending_ = false;
  this->tx_retry_count_ = 0;
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

  // Abandon RSTACKs owed from a previous attempt: they can no longer arrive in a state
  // where suppressing them is correct, and a stale count would swallow a real reset.
  this->own_rst_outstanding_ = 0;

  this->send_rst_frame_();
}

void ZigbeeProxy::reset_ncp_link_() {
  this->ash_state_ = AshState::CONNECTING;
  this->setup_time_ = millis();  // Reset timeout reference for the RSTACK wait
  this->tx_sequence_ = 0;
  this->rx_sequence_ = 0;
  this->tx_buffer_pending_ = false;
  this->tx_retry_count_ = 0;
  this->parsing_state_ = ParsingState::WAIT_FLAG_START;
  this->relay_length_ = 0;
  this->detector_.reset();
  this->send_rst_frame_(false);
}

void ZigbeeProxy::send_rst_frame_(bool own_reset) {
  if (own_reset) {
    this->own_rst_outstanding_++;
  }

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

  // Account for this RSTACK before deciding whether the client should see it. Only a
  // reset we did not cause is news to the client; relaying one of ours makes bellows
  // call enter_failed_state() and cancel every command it has in flight.
  bool solicited_by_us = this->own_rst_outstanding_ > 0;
  if (solicited_by_us) {
    this->own_rst_outstanding_--;
  }

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
    if (solicited_by_us) {
      // One of our own resets answered while a client reset is still outstanding. Stay in
      // CONNECTING and keep waiting for the RSTACK the client is actually owed.
      ESP_LOGV(TAG, "Consumed own RSTACK while awaiting the client's");
      return;
    }
    // RSTACK during connecting (triggered by client RST forwarding)
    ESP_LOGV(TAG, "Received RSTACK, NCP ready");
    this->ash_state_ = AshState::CONNECTED;
  } else if (solicited_by_us) {
    // Surplus RSTACK from one of our own resets, most often an RST retry racing a reply
    // that was merely slow. The client never asked for it, so swallow it.
    ESP_LOGV(TAG, "Consumed surplus RSTACK from own reset");
    this->ash_state_ = AshState::CONNECTED;
  } else if (this->api_connection_ != nullptr) {
    // A reset we did not cause: the NCP rebooted on its own, which invalidates the
    // client's session, so it has to hear about it.
    ESP_LOGW(TAG, "NCP reset unexpectedly, notifying client");
    this->ash_state_ = AshState::CONNECTED;
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

// Boot-time NCP metadata harvest
// Sequence: RST -> RSTACK -> version() -> getTokenData(NVM3KEY_STACK_NODE_DATA) ->
//           getEui64() -> RST -> RSTACK
//
// Both values are read with the stack left down. The alternative -- set the stack
// profile, call networkInit, wait for a stackStatusHandler callback announcing
// NETWORK_UP, then call getNetworkParameters -- does work, but it joins the network
// just to read four fields, so merely powering the device on brings the radio up.
// Reading the NVM3 token needs none of it, and getEui64 answers with the stack down
// as well, so nothing in this sequence starts the stack.

void ZigbeeProxy::advance_boot_state_() {
  switch (this->boot_state_) {
    case BootState::SEND_VERSION:
      this->send_ezsp_version_();
      this->boot_state_ = BootState::WAIT_VERSION;
      break;

    case BootState::SEND_TOKEN_DATA:
      this->send_get_token_data_();
      this->boot_state_ = BootState::WAIT_TOKEN_DATA;
      break;

    case BootState::SEND_GET_EUI64:
      this->send_get_eui64_();
      this->boot_state_ = BootState::WAIT_EUI64;
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

    case BootState::WAIT_TOKEN_DATA:
      if (frame_id == EZSP_GET_TOKEN_DATA && is_response) {
        this->handle_token_data_response_(payload, payload_length);
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

void ZigbeeProxy::send_get_token_data_() {
  // getTokenData takes a 32-bit NVM3 key and a 32-bit index, both little-endian.
  uint8_t cmd[] = {
      this->ezsp_sequence_++,                  // Sequence
      EZSP_FRAME_CONTROL_COMMAND,              // Frame control (low)
      EZSP_FRAME_CONTROL_EXTENDED,             // Frame control (high)
      EZSP_GET_TOKEN_DATA & 0xFF,              // Frame ID (low)
      (EZSP_GET_TOKEN_DATA >> 8) & 0xFF,       // Frame ID (high)
      NVM3KEY_STACK_NODE_DATA & 0xFF,          // token
      (NVM3KEY_STACK_NODE_DATA >> 8) & 0xFF,   //
      (NVM3KEY_STACK_NODE_DATA >> 16) & 0xFF,  //
      (NVM3KEY_STACK_NODE_DATA >> 24) & 0xFF,  //
      0x00,                                    // index
      0x00,                                    //
      0x00,                                    //
      0x00,                                    //
  };
  ash_randomize(cmd, sizeof(cmd));
  ESP_LOGV(TAG, "Sending EZSP getTokenData(NVM3KEY_STACK_NODE_DATA)");
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
      this->boot_state_ = BootState::SEND_TOKEN_DATA;
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

  this->boot_state_ = BootState::SEND_TOKEN_DATA;
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

void ZigbeeProxy::handle_token_data_response_(const uint8_t *data, size_t length) {
  // getTokenData response: [status (4)] [length (4)] [value (length)]
  // The EUI64 read follows regardless of what happens here: it is a hardware address
  // that exists whether or not the radio is commissioned, and it is what lets a client
  // tell an unformed radio apart from an unreachable one.
  if (length < TOKEN_DATA_VALUE_OFFSET + NV3_NODE_DATA_SIZE) {
    ESP_LOGW(TAG, "getTokenData response too short: %u bytes", length);
    this->boot_state_ = BootState::SEND_GET_EUI64;
    this->advance_boot_state_();
    return;
  }

  if (data[0] != static_cast<uint8_t>(SlStatus::OK)) {
    ESP_LOGW(TAG, "getTokenData(NVM3KEY_STACK_NODE_DATA) failed: 0x%02X", data[0]);
    this->boot_state_ = BootState::SEND_GET_EUI64;
    this->advance_boot_state_();
    return;
  }

  const uint8_t *node_data = data + TOKEN_DATA_VALUE_OFFSET;

  if (node_data[NV3_NODE_DATA_NODE_TYPE_OFFSET] == NV3_NODE_TYPE_UNKNOWN_DEVICE) {
    ESP_LOGD(TAG, "NCP has no network configured");
    this->boot_state_ = BootState::SEND_GET_EUI64;
    this->advance_boot_state_();
    return;
  }

  memcpy(this->network_info_.extended_pan_id.data(), node_data + NV3_NODE_DATA_EXT_PAN_ID_OFFSET, 8);
  this->network_info_.pan_id =
      node_data[NV3_NODE_DATA_PAN_ID_OFFSET] | (static_cast<uint16_t>(node_data[NV3_NODE_DATA_PAN_ID_OFFSET + 1]) << 8);
  this->network_info_.channel = node_data[NV3_NODE_DATA_CHANNEL_OFFSET];
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

// Bootloader detection - fed consecutive raw byte pairs while the ASH link is not CONNECTED
// (bootloader output only ever appears in place of the RSTACK after a reset). Detection is
// advisory only: raw bootloader traffic is carried by a `serial_proxy` bound to the same
// UART, not by this component.
void ZigbeeProxy::check_bootloader_mode_(uint8_t prev_byte, uint8_t byte) {
  // Check for Silicon Labs bootloader menu prompt (0xC1 0x0D)
  if (prev_byte == 0xC1 && byte == 0x0D) {
    if (this->bootloader_state_ != BootloaderState::MENU) {
      ESP_LOGW(TAG, "NCP in bootloader menu mode detected\n"
                    "  Flash NCP firmware via the serial proxy, or power cycle the device");
      this->bootloader_state_ = BootloaderState::MENU;
    }
    return;
  }

  // Check for upload begin (0x43)
  if (byte == 0x43) {
    if (this->bootloader_state_ != BootloaderState::DETECTED) {
      ESP_LOGW(TAG, "NCP bootloader upload mode detected");
      this->bootloader_state_ = BootloaderState::DETECTED;
    }
    return;
  }
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

    if (this->ash_state_ != AshState::CONNECTED) {
      this->check_bootloader_mode_(this->last_rx_byte_, byte);
      this->last_rx_byte_ = byte;
    } else if (this->bootloader_state_ != BootloaderState::NORMAL) {
      // Normal traffic while connected clears any stale bootloader detection
      ESP_LOGV(TAG, "NCP returned to normal operation");
      this->bootloader_state_ = BootloaderState::NORMAL;
    }

    if (this->boot_sequence_active_) {
      // Harvest: this component is the ASH endpoint and consumes the frames itself
      this->parse_byte_(byte);
    } else {
      this->relay_ncp_byte_(byte);
    }
  } while (this->available());

  this->relay_flush_();
}

// ==================== Transparent relay ====================

void ZigbeeProxy::relay_ncp_byte_(uint8_t byte) {
  if (this->relay_length_ >= sizeof(this->relay_buffer_)) {
    this->relay_flush_();
  }
  this->relay_buffer_[this->relay_length_++] = byte;

  // Observation only: the detector never gates forwarding, so it adds no latency and a
  // frame it cannot parse still reaches the client, which judges it for itself.
  this->detector_.from_ncp(byte);

  // In relay mode the detector is the only thing watching the link, so its progress is
  // what tells us the NCP is alive. Without this ash_state_ sits at CONNECTING, times out
  // into FAILED, and autonomous recovery resets the NCP underneath a working session --
  // relaying an RSTACK the client never asked for, which kills it outright.
  if (this->detector_.state() != AshDetectState::IDLE) {
    this->ash_state_ = AshState::CONNECTED;
  }

  uint8_t ack_num;
  if (this->detector_.take_pending_ack(ack_num)) {
    // The client suppresses its own ACKs, so this is the only acknowledgement the NCP
    // will see. Only ever sent for a frame that passed CRC and arrived in sequence.
    this->send_ack_frame_(ack_num);
    const uint8_t *ezsp = this->detector_.last_ezsp_frame();
    const size_t ezsp_length = this->detector_.last_ezsp_frame_length();
    this->sniff_network_info_(ezsp, ezsp_length);
    this->sniff_stack_status_(ezsp, ezsp_length);
  }
}

// A proxied getNetworkParameters response is the only authoritative view of the network
// available while a client owns the link, so metadata is refreshed from the client's own
// traffic rather than by injecting commands. Read-only: a frame that fails any check
// simply leaves the previous values in place.
void ZigbeeProxy::sniff_network_info_(const uint8_t *frame, size_t length) {
  // Every frame after the version handshake uses extended framing, so the header size is
  // fixed and needs no knowledge of the negotiated version.
  if (length < EZSP_EXTENDED_HEADER_SIZE + NETWORK_PARAMS_RESPONSE_SIZE) {
    return;
  }

  // The relay never derandomizes, so work on a copy: the original bytes are already on
  // their way to the client and must stay untouched.
  uint8_t decoded[EZSP_EXTENDED_HEADER_SIZE + NETWORK_PARAMS_RESPONSE_SIZE];
  memcpy(decoded, frame, sizeof(decoded));
  ash_randomize(decoded, sizeof(decoded));

  const uint16_t frame_id = decoded[3] | (static_cast<uint16_t>(decoded[4]) << 8);
  if (frame_id != EZSP_GET_NETWORK_PARAMETERS || (decoded[1] & EZSP_FRAME_CONTROL_RESPONSE) == 0) {
    return;
  }

  const uint8_t *params = decoded + EZSP_EXTENDED_HEADER_SIZE;
  if (params[NETWORK_PARAMS_STATUS_OFFSET] != static_cast<uint8_t>(SlStatus::OK)) {
    return;
  }

  const uint16_t pan_id =
      params[NETWORK_PARAMS_PAN_ID_OFFSET] | (static_cast<uint16_t>(params[NETWORK_PARAMS_PAN_ID_OFFSET + 1]) << 8);
  const uint8_t channel = params[NETWORK_PARAMS_CHANNEL_OFFSET];
  const bool changed =
      pan_id != this->network_info_.pan_id || channel != this->network_info_.channel ||
      memcmp(this->network_info_.extended_pan_id.data(), params + NETWORK_PARAMS_EXT_PAN_ID_OFFSET, 8) != 0;
  if (!changed) {
    return;
  }

  memcpy(this->network_info_.extended_pan_id.data(), params + NETWORK_PARAMS_EXT_PAN_ID_OFFSET, 8);
  this->network_info_.pan_id = pan_id;
  this->network_info_.channel = channel;
  this->network_info_.valid = true;
  ESP_LOGD(TAG, "Network info from proxied traffic: PAN 0x%04X, channel %u", pan_id, channel);
  this->send_network_info_changed_msg_();
}

void ZigbeeProxy::sniff_stack_status_(const uint8_t *frame, size_t length) {
  // stackStatusHandler: [status (4)]. It carries no network parameters, so it can only
  // invalidate, never refresh. Worth acting on anyway: a client leaving a network need not
  // read parameters afterwards, and without this the old PAN would be reported forever.
  if (length < EZSP_EXTENDED_HEADER_SIZE + 1) {
    return;
  }

  uint8_t decoded[EZSP_EXTENDED_HEADER_SIZE + 1];
  memcpy(decoded, frame, sizeof(decoded));
  ash_randomize(decoded, sizeof(decoded));

  const uint16_t frame_id = decoded[3] | (static_cast<uint16_t>(decoded[4]) << 8);
  // A callback sets both the response direction and the callback bit, so match on both
  if (frame_id != EZSP_STACK_STATUS_HANDLER ||
      (decoded[1] & EZSP_FRAME_CONTROL_CALLBACK) != EZSP_FRAME_CONTROL_CALLBACK) {
    return;
  }

  if (decoded[EZSP_EXTENDED_HEADER_SIZE] != static_cast<uint8_t>(SlStatus::NETWORK_DOWN)) {
    return;  // NETWORK_UP and everything else leaves what we have standing
  }

  if (this->network_info_.pan_id == 0 && this->network_info_.channel == 0) {
    return;  // Already reporting no network
  }

  ESP_LOGD(TAG, "Stack reported NETWORK_DOWN, dropping stale network info");
  this->network_info_.pan_id = 0;
  this->network_info_.channel = 0;
  this->network_info_.extended_pan_id.fill(0);
  // The IEEE address is a hardware property and survives leaving a network, so it stays.
  this->send_network_info_changed_msg_();
}

void ZigbeeProxy::relay_flush_() {
  if (this->relay_length_ == 0) {
    return;
  }
  const size_t length = this->relay_length_;
  this->relay_length_ = 0;
  if (this->api_connection_ == nullptr) {
    return;
  }
  this->outgoing_proto_msg_.data = this->relay_buffer_;
  this->outgoing_proto_msg_.data_len = length;
  if (!this->api_connection_->send_zigbee_proxy_frame(this->outgoing_proto_msg_)) {
    // API TX backpressure. Dropping bytes is recoverable: the client sees a truncated
    // frame, fails its CRC and NAKs, and the NCP retransmits. Withholding our ACK would
    // achieve the same thing more slowly, and buffering risks unbounded growth.
    ESP_LOGW(TAG, "Dropped %u relayed bytes (API TX buffer full)", length);
  }
}

}  // namespace esphome::zigbee_proxy

#endif  // USE_ZIGBEE_PROXY
