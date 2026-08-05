#include "zigbee_proxy.h"

#ifdef USE_ZIGBEE_PROXY

#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"
#include "ezsp_commands.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#ifdef USE_ESP32
#include <esp_wifi.h>
#endif
#endif

namespace esphome::zigbee_proxy {

static const char *const TAG = "zigbee_proxy";

// A freshly attached USB device answers its enumeration before its CDC endpoints will
// actually carry bytes, so an RST sent the instant it appears is written into a void and
// is only recovered by the 3 s RSTACK retry. zwave_proxy defers its own first query for
// the same reason.
static constexpr uint32_t DEVICE_SETTLE_MS = 500;
static constexpr uint32_t BOOT_SEQUENCE_TIMEOUT_MS = 10000;  // Overall boot-harvest timeout
static constexpr size_t NETWORK_INFO_PAYLOAD_SIZE = 19;      // ieee(8) + extended_pan(8) + pan_id(2) + channel(1)
static constexpr size_t ZIGBEE_MAX_LOG_BYTES = 168;          // Cap verbose hex dumps (168 * 3 = 504 byte buffer)

ZigbeeProxy *global_zigbee_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

ZigbeeProxy::ZigbeeProxy() { global_zigbee_proxy = this; }

void ZigbeeProxy::setup() {
  this->setup_time_ = millis();

  // The port reads and forwards on its own; we only observe what passes and inject the
  // occasional acknowledgement. The harvest below runs before any client connects, so the
  // port has to keep reading with nobody subscribed -- hence the explicit request.
  this->parent_->set_tap(this);
  this->parent_->tap_request_port();

  // Initialize state
  this->ash_state_ = AshState::DISCONNECTED;
  this->parsing_state_ = ParsingState::WAIT_FLAG_START;
  this->tx_sequence_ = 0;
  this->rx_sequence_ = 0;

  // Send RST frame to initialize NCP
  this->reset_ash_protocol_();
}

void ZigbeeProxy::loop() {
  // Watch for the radio being unplugged and plugged back in. The whole point of the
  // metadata is that a stick moved from another host is recognised here, and that move is
  // a hot-plug: harvesting only at boot would miss it entirely and leave the device
  // advertising nothing for a radio that is sitting right there.
  const bool connected = this->parent_->is_device_connected();
  if (connected != this->was_connected_) {
    this->was_connected_ = connected;
    this->on_device_presence_changed_(connected);
  }

  // A re-harvest owed from on_protocol_disabled(), now that the port is idle again
  if (this->reharvest_pending_ && !this->boot_sequence_active_ && this->parent_->get_api_connection() == nullptr) {
    ESP_LOGI(TAG, "Port idle again, re-reading network info");
    this->reharvest_pending_ = false;
    this->parent_->tap_request_port();
    this->reset_ash_protocol_();
    return;
  }

  // Bytes arrive through on_device_rx(), so the only work left on an idle tick is the
  // presence check above -- an atomic load and a compare. The loop deliberately stays
  // enabled for it: disabling it would mean a stick plugged in later is never noticed.
  if (!this->boot_sequence_active_) {
    return;
  }

  // Check for ACK timeout and handle retransmission
  if (this->tx_buffer_pending_ && this->check_ack_timeout_()) {
    this->handle_retransmission_();
  }

  this->check_boot_timeouts_();
}

void ZigbeeProxy::on_device_rx(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    const uint8_t byte = data[i];
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
      continue;
    }

    // Observation only: the detector never gates forwarding, so it adds no latency and a
    // frame it cannot parse still reaches the client, which judges it for itself.
    this->detector_.from_ncp(byte);

    // Outside the harvest the detector is the only thing watching the link, so its
    // progress is what tells us the NCP is alive -- and hence that any earlier bootloader
    // detection is stale.
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
}

void ZigbeeProxy::on_client_tx(const uint8_t *data, size_t len) {
  // Scanning this direction only matters while waiting for the version command that
  // completes the handshake. Outside that window it is skipped entirely -- which is what
  // makes a firmware upload, all of which flows this way, essentially free.
  if (!this->detector_.needs_host_scan()) {
    return;
  }
  for (size_t i = 0; i < len; i++) {
    this->detector_.from_host(data[i]);
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
  this->parent_->tap_pump();
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
    case api::enums::ZIGBEE_PROXY_REQUEST_TYPE_NETWORK_INFO:
      this->send_network_info_changed_msg_(api_connection);
      break;

    default:
      ESP_LOGW(TAG, "Unknown request type: %d", static_cast<int>(msg.type));
      break;
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

  this->send_rst_frame_();
}

void ZigbeeProxy::send_rst_frame_() {
  // Build a combined buffer: 32 CAN bytes followed immediately by the RST frame,
  // sent as a single write. This ensures correct byte ordering and
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
  this->parent_->write_from_tap(combined, CAN_COUNT + rst_len);
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

    // Stale bytes preceding the RSTACK (leftover UART FIFO content, or a partial prior
    // frame) need no draining: the port owns the read side now, so anything before the
    // RSTACK has already passed through the parser and been discarded by frame delimiting.

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
  } else {
    // An RSTACK outside the harvest belongs to whoever reset the NCP -- a client opening
    // its own session, most likely. Nothing to do but note that the link is alive.
    ESP_LOGV(TAG, "RSTACK received outside boot sequence (boot_state=%d)", static_cast<int>(this->boot_state_));
    this->ash_state_ = AshState::CONNECTED;
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

  // Reported only. This frame is only ever seen during the boot harvest, whose overall
  // timeout already guarantees forward progress; resetting the NCP here would restart that
  // timeout and could block startup indefinitely on a link that keeps erroring.
  ESP_LOGE(TAG, "NCP error: %s (0x%02X)", error_str, error_code);
}

bool ZigbeeProxy::send_ack_frame_(uint8_t ack_num) {
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, sizeof(frame), nullptr, 0, AshFrameType::ACK, 0, ack_num);
  this->parent_->write_from_tap(frame, length);
  this->last_ack_sent_ = ack_num;
  ESP_LOGV(TAG, "Sent ACK for frame %d", ack_num);
  return true;
}

bool ZigbeeProxy::send_nak_frame_(uint8_t ack_num) {
  uint8_t frame[8];
  size_t length = this->build_frame_(frame, sizeof(frame), nullptr, 0, AshFrameType::NAK, 0, ack_num);
  this->parent_->write_from_tap(frame, length);
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
  this->parent_->write_from_tap(this->tx_buffer_.data(), frame_length);

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
  this->parent_->write_from_tap(this->tx_pending_buffer_.data(), this->tx_pending_length_);
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
namespace {

// 802.11b/g/n in the 2.4 GHz band: channels 1-13 sit 5 MHz apart starting at 2412 MHz,
// with channel 14 an outlier. Returns 0 for anything not in the band.
uint16_t wifi_center_mhz(uint8_t channel) {
  if (channel == 14) {
    return 2484;
  }
  if (channel >= 1 && channel <= 13) {
    return static_cast<uint16_t>(2412 + 5 * (channel - 1));
  }
  return 0;
}

// 802.15.4 in the 2.4 GHz band: channels 11-26, 5 MHz apart starting at 2405 MHz.
uint16_t zigbee_center_mhz(uint8_t channel) {
  if (channel >= 11 && channel <= 26) {
    return static_cast<uint16_t>(2405 + 5 * (channel - 11));
  }
  return 0;
}

// 802.15.4 O-QPSK occupies about 2 MHz. WiFi is not fixed: the ESP32 supports HT40 as
// well as HT20, and an HT40 link is both twice as wide and re-centred 10 MHz towards its
// secondary channel, so assuming 20 MHz would mispredict overlap at both edges.
constexpr uint16_t ZIGBEE_BANDWIDTH_MHZ = 2;
constexpr uint16_t WIFI_BANDWIDTH_HT20_MHZ = 20;
constexpr uint16_t WIFI_BANDWIDTH_HT40_MHZ = 40;

struct WifiOccupancy {
  uint16_t center_mhz;
  uint16_t bandwidth_mhz;
};

// Two carriers clash when their halves meet: |f1 - f2| < (bw1 + bw2) / 2.
bool channels_overlap(const WifiOccupancy &wifi, uint16_t zigbee_mhz) {
  const uint16_t separation =
      wifi.center_mhz > zigbee_mhz ? wifi.center_mhz - zigbee_mhz : zigbee_mhz - wifi.center_mhz;
  return separation * 2 < wifi.bandwidth_mhz + ZIGBEE_BANDWIDTH_MHZ;
}

// Resolve what the radio is actually using, rather than assuming. Falls back to HT20,
// which is what every non-ESP32 target here supports anyway.
WifiOccupancy wifi_occupancy(uint8_t primary_channel) {
  WifiOccupancy occupancy{wifi_center_mhz(primary_channel), WIFI_BANDWIDTH_HT20_MHZ};
#ifdef USE_ESP32
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (occupancy.center_mhz != 0 && esp_wifi_get_channel(&primary, &second) == ESP_OK &&
      second != WIFI_SECOND_CHAN_NONE) {
    occupancy.bandwidth_mhz = WIFI_BANDWIDTH_HT40_MHZ;
    // The 40 MHz block spans the primary and its neighbour, so its centre sits half a
    // 20 MHz channel away from the primary's, on the secondary's side.
    occupancy.center_mhz =
        static_cast<uint16_t>(second == WIFI_SECOND_CHAN_ABOVE ? occupancy.center_mhz + 10 : occupancy.center_mhz - 10);
  }
#endif
  return occupancy;
}

}  // namespace

void ZigbeeProxy::check_wifi_zigbee_conflict_() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component == nullptr || !this->network_info_.valid || this->network_info_.channel == 0) {
    return;
  }

  const uint8_t wifi_channel = wifi::global_wifi_component->get_wifi_channel();
  const WifiOccupancy wifi = wifi_occupancy(wifi_channel);
  if (wifi.center_mhz == 0) {
    return;  // Not connected yet, or a 5 GHz channel that cannot clash by definition
  }

  const uint8_t zigbee_channel = this->network_info_.channel;
  const uint16_t zigbee_mhz = zigbee_center_mhz(zigbee_channel);
  if (zigbee_mhz == 0) {
    return;
  }

  if (!channels_overlap(wifi, zigbee_mhz)) {
    ESP_LOGV(TAG, "No WiFi/Zigbee channel conflict (WiFi %u @ %u MHz/%u MHz wide, Zigbee %u @ %u MHz)", wifi_channel,
             wifi.center_mhz, wifi.bandwidth_mhz, zigbee_channel, zigbee_mhz);
    return;
  }

  // Naming the channels that are actually clear beats a fixed suggestion: which ones those
  // are depends entirely on where WiFi happens to be, and an auto-channel AP is rarely on
  // 1, 6 or 11.
  char clear[64];
  size_t offset = 0;
  for (uint8_t candidate = 11; candidate <= 26; candidate++) {
    if (channels_overlap(wifi, zigbee_center_mhz(candidate))) {
      continue;
    }
    const int written = snprintf(clear + offset, sizeof(clear) - offset, offset == 0 ? "%u" : ", %u", candidate);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(clear) - offset) {
      break;
    }
    offset += static_cast<size_t>(written);
  }

  ESP_LOGW(TAG,
           "WiFi/Zigbee channel conflict detected\n"
           "  WiFi channel %u (%u MHz, %u MHz wide) overlaps Zigbee channel %u (%u MHz)\n"
           "  Zigbee channels clear of this WiFi channel: %s",
           wifi_channel, wifi.center_mhz, wifi.bandwidth_mhz, zigbee_channel, zigbee_mhz, offset > 0 ? clear : "none");
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

// A proxied getNetworkParameters response is the only authoritative view of the network
// available while a client owns the link, so metadata is refreshed from the client's own
// traffic rather than by injecting commands. Read-only: a frame that fails any check
// simply leaves the previous values in place.
void ZigbeeProxy::on_device_presence_changed_(bool connected) {
  if (!connected) {
    ESP_LOGD(TAG, "Radio disconnected, discarding network info");
    this->boot_sequence_active_ = false;
    this->boot_state_ = BootState::IDLE;
    this->reharvest_pending_ = false;
    if (this->network_info_.valid) {
      this->network_info_ = {};
      this->send_network_info_changed_msg_();
    }
    return;
  }

  // A radio just appeared. Whatever we knew described a different one, so start over.
  ESP_LOGI(TAG, "Radio connected, reading network info");
  if (this->network_info_.valid) {
    this->network_info_ = {};
    this->send_network_info_changed_msg_();
  }
  this->reharvest_pending_ = true;
  this->reharvest_after_ = millis() + DEVICE_SETTLE_MS;
}

void ZigbeeProxy::on_protocol_disabled() {
  // Everything here was read from a radio that a client is now taking over, so none of it
  // can be trusted: it survives a reflash to Thread, or to nothing at all, and would leave
  // us advertising a network that no longer exists. Reporting nothing is the honest answer
  // until a fresh harvest says otherwise.
  //
  // The harvest cannot run now -- the client holds the port -- so it is deferred. Once the
  // client goes away the port stays open for us (tap_needs_port) and loop() picks it up.
  this->reharvest_pending_ = true;
  this->reharvest_after_ = 0;
  this->enable_loop();

  if (!this->network_info_.valid) {
    return;
  }
  ESP_LOGD(TAG, "Protocol handling disabled, discarding network info");
  this->network_info_ = {};
  this->send_network_info_changed_msg_();
}

void ZigbeeProxy::sniff_network_info_(const uint8_t *frame, size_t length) {
  // Every frame after the version handshake uses extended framing, so the header size is
  // fixed and needs no knowledge of the negotiated version.
  if (length < EZSP_EXTENDED_HEADER_SIZE + NETWORK_PARAMS_RESPONSE_SIZE) {
    return;
  }

  // The observed bytes are the port's, not ours, so work on a copy: they are already on
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

}  // namespace esphome::zigbee_proxy

#endif  // USE_ZIGBEE_PROXY
