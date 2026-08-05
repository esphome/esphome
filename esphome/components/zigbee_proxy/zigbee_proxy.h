#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZIGBEE_PROXY

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/serial_proxy/serial_proxy.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "ash_protocol.h"
#include "ash_detector.h"

#include <array>

namespace esphome::zigbee_proxy {

// Timeout configuration structure
struct TimeoutConfig {
  uint32_t initial_timeout_ms{1600};  // Initial ACK timeout
  uint32_t min_timeout_ms{400};       // Minimum adaptive timeout
  uint32_t max_timeout_ms{3200};      // Maximum adaptive timeout
  uint32_t current_timeout_ms{1600};  // Current adaptive timeout
};

// Network information structure
struct NetworkInfo {
  std::array<uint8_t, ZIGBEE_IEEE_ADDR_SIZE> ieee_address{};
  uint16_t pan_id{0};
  std::array<uint8_t, 8> extended_pan_id{};
  uint8_t channel{0};
  bool valid{false};
};

enum ZigbeeProxyFeature : uint32_t {
  FEATURE_ZIGBEE_PROXY_ENABLED = 1 << 0,
  // Set only when the harvest actually read a network off the radio. Without it a client
  // cannot tell "a Zigbee radio with no network formed" from "not a Zigbee radio at all"
  // -- both otherwise present as ENABLED with an all-zero payload, and the second happens
  // whenever the NCP has been reflashed to Thread or is simply not responding.
  FEATURE_ZIGBEE_NETWORK_INFO_VALID = 1 << 1,
};

// Boot-time initialization state machine
enum class BootState : uint8_t {
  IDLE,               // Not initializing
  WAIT_RSTACK,        // Sent RST, waiting for RSTACK
  SEND_VERSION,       // Send EZSP version command
  WAIT_VERSION,       // Waiting for version response
  SEND_TOKEN_DATA,    // Send getTokenData(NVM3KEY_STACK_NODE_DATA)
  WAIT_TOKEN_DATA,    // Waiting for token data response
  SEND_GET_EUI64,     // Send getEui64 command
  WAIT_EUI64,         // Waiting for EUI64 response
  SEND_FINAL_RST,     // Send final RST to reset NCP
  WAIT_FINAL_RSTACK,  // Waiting for final RSTACK
  COMPLETE,           // Boot sequence complete
  FAILED,             // Boot sequence failed
};

// Watches a `serial_proxy` port carrying an EZSP NCP and reports what it learns about the
// Zigbee network. It never carries client traffic: the serial proxy owns the port and the
// bytes, and this component only observes them, plus two exceptions where it writes to the
// port itself -- the boot-time metadata harvest, which runs before any client connects, and
// the ASH acknowledgements a client asks it to send on its behalf.
class ZigbeeProxy : public serial_proxy::SerialProxyTap, public Component {
 public:
  ZigbeeProxy();

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  bool can_proceed() override;

  void set_serial_proxy(serial_proxy::SerialProxy *parent) { this->parent_ = parent; }

  // SerialProxyTap
  void on_device_rx(const uint8_t *data, size_t len) override;
  void on_client_tx(const uint8_t *data, size_t len) override;
  bool tap_needs_port() const override {
    if (this->boot_sequence_active_) {
      return true;
    }
    // A pending re-harvest waits for the port to go idle. Starting one under a subscriber
    // would inject our own ASH frames into whatever it is doing -- most likely the very
    // firmware upload that invalidated the metadata.
    return this->reharvest_pending_ && this->parent_->get_api_connection() == nullptr;
  }

  /// The port stopped handling our protocol, so whatever we know about the radio may no
  /// longer be true -- a client asking for raw bytes is usually about to reflash it.
  void on_protocol_disabled() override;

  /// The radio was unplugged or a new one appeared; metadata describes neither.
  void on_device_presence_changed_(bool connected);

  // API integration
  void api_connection_authenticated(api::APIConnection *conn);
  void zigbee_proxy_request(api::APIConnection *api_connection, const api::ZigbeeProxyRequest &msg);

  // Feature flags
  uint32_t get_feature_flags() const {
    uint32_t flags = ZigbeeProxyFeature::FEATURE_ZIGBEE_PROXY_ENABLED;
    if (this->network_info_.valid) {
      flags |= ZigbeeProxyFeature::FEATURE_ZIGBEE_NETWORK_INFO_VALID;
    }
    return flags;
  }

  // Network information accessors
  const NetworkInfo &get_network_info() const { return this->network_info_; }
  uint64_t get_ieee_address() const;

  // Timeout configuration (callable from Python/API)
  void set_timeout_config(uint32_t initial_ms, uint32_t min_ms, uint32_t max_ms);
  void set_initial_timeout(uint32_t timeout_ms) { this->timeout_config_.initial_timeout_ms = timeout_ms; }
  void set_min_timeout(uint32_t timeout_ms) { this->timeout_config_.min_timeout_ms = timeout_ms; }
  void set_max_timeout(uint32_t timeout_ms) { this->timeout_config_.max_timeout_ms = timeout_ms; }

 protected:
  // ASH Protocol State Machine
  void reset_ash_protocol_();
  void send_rst_frame_();
  void handle_rstack_frame_(const uint8_t *data, size_t length);
  void handle_error_frame_(const uint8_t *data, size_t length);
  // Applies a frame's ackNum to the pending TX frame. Returns true if it
  // acknowledged one. Valid on DATA, ACK and NAK frames alike.
  bool handle_ack_num_(uint8_t ack_num);
  bool send_ack_frame_(uint8_t ack_num);
  bool send_nak_frame_(uint8_t ack_num);
  bool send_data_frame_(const uint8_t *data, size_t length, bool retransmit = false);

  // Frame parsing and building (implemented in ash_protocol.cpp)
  bool parse_byte_(uint8_t byte);
  void parse_control_byte_(uint8_t control);
  bool validate_frame_crc_();
  // Builds a stuffed frame into output; returns 0 if the frame (worst case 2*length + 8
  // bytes after byte stuffing) would exceed capacity.
  size_t build_frame_(uint8_t *output, size_t capacity, const uint8_t *data, size_t length, AshFrameType type,
                      uint8_t frame_num = 0, uint8_t ack_num = 0, bool retx = false);
  uint16_t calculate_crc_(const uint8_t *data, size_t length, uint16_t init = ASH_CRC_INIT);

  // Sequence number management
  void increment_tx_sequence_() { this->tx_sequence_ = (this->tx_sequence_ + 1) & ASH_MAX_SEQUENCE; }
  void increment_rx_sequence_() { this->rx_sequence_ = (this->rx_sequence_ + 1) & ASH_MAX_SEQUENCE; }

  // Timeout management
  void update_adaptive_timeout_(uint32_t measured_rtt_ms);
  void start_ack_timer_() { this->ack_timer_start_ = millis(); }
  bool check_ack_timeout_();

  // Retransmission
  void handle_retransmission_();
  void clear_tx_buffer_() {
    this->tx_buffer_pending_ = false;
    this->tx_retry_count_ = 0;
  }

  // Boot-time NCP initialization
  void advance_boot_state_();
  void check_boot_timeouts_();
  void handle_boot_data_frame_(const uint8_t *data, size_t length);
  void send_ezsp_version_();
  void send_get_eui64_();
  void send_get_token_data_();
  void handle_version_response_(const uint8_t *data, size_t length);
  void handle_eui64_response_(const uint8_t *data, size_t length);
  void handle_token_data_response_(const uint8_t *data, size_t length);

  // IEEE address and network info
  bool set_ieee_address_(const uint8_t *new_address);
  void send_network_info_changed_msg_(api::APIConnection *conn = nullptr);

  // WiFi/Zigbee channel conflict detection
  void check_wifi_zigbee_conflict_();

  // Bootloader detection (fed consecutive raw byte pairs while not CONNECTED)
  void check_bootloader_mode_(uint8_t prev_byte, uint8_t byte);

  // Reads network metadata out of a proxied getNetworkParameters response. Read-only, so a
  // misparse costs a missed update rather than corrupting anything.
  void sniff_network_info_(const uint8_t *frame, size_t length);
  // Invalidates network metadata when the stack reports it has left the network.
  void sniff_stack_status_(const uint8_t *frame, size_t length);

  // NCP-side ASH buffers
  std::array<uint8_t, MAX_ASH_FRAME_SIZE> rx_buffer_;
  std::array<uint8_t, MAX_ASH_FRAME_SIZE> tx_buffer_;
  std::array<uint8_t, MAX_ASH_FRAME_SIZE> tx_pending_buffer_;  // For retransmission

  // Network information
  NetworkInfo network_info_;

  // Timeout configuration
  TimeoutConfig timeout_config_;

  // The port this component observes. Owns the UART and the bytes; every write we make
  // goes through it.
  serial_proxy::SerialProxy *parent_{nullptr};

  uint32_t setup_time_{0};       // Time when last RST frame was sent
  uint32_t boot_start_time_{0};  // Time when the boot sequence began (for overall timeout)
  uint32_t ack_timer_start_{0};  // Time when ACK timer started
  uint32_t last_rtt_ms_{0};      // Last measured round-trip time

  uint16_t rx_buffer_index_{0};    // Index for populating rx_buffer_
  uint16_t tx_pending_length_{0};  // Length of pending TX frame for retransmission
  uint16_t calculated_crc_{0};     // CRC calculated during frame reception

  uint8_t tx_sequence_{0};           // TX sequence number (0-7)
  uint8_t rx_sequence_{0};           // RX sequence number (0-7)
  uint8_t tx_retry_count_{0};        // Number of retransmission attempts
  uint8_t tx_pending_frame_num_{0};  // Frame number of pending TX frame
  uint8_t last_ack_sent_{0};         // Last ACK number sent
  uint8_t last_rx_byte_{0};          // Previous raw RX byte (bootloader detection)

  AshState ash_state_{AshState::DISCONNECTED};
  ParsingState parsing_state_{ParsingState::WAIT_FLAG_START};
  BootloaderState bootloader_state_{BootloaderState::NORMAL};
  BootState boot_state_{BootState::IDLE};

  uint8_t ezsp_version_{0};            // NCP's EZSP protocol version
  uint8_t ezsp_sequence_{0};           // EZSP frame sequence number
  uint8_t ezsp_requested_version_{0};  // Version we last requested (for re-negotiation)
  // The NCP keeps using legacy framing until `version` is repeated in the
  // negotiated (extended) format; until then it rejects every extended command
  // with frame ID 0x0058. Tracks whether that second handshake has happened.
  bool ezsp_version_confirmed_{false};

  bool tx_buffer_pending_{false};  // True if waiting for ACK from NCP
  bool escape_next_byte_{false};   // True if next NCP byte should be unescaped

  bool boot_sequence_active_{false};  // True during boot-time init
  // Set when the metadata was discarded and a fresh harvest is owed once the port frees up
  bool reharvest_pending_{false};
  // Last observed device presence, for spotting a hot-plug
  bool was_connected_{false};
  // Earliest millis() at which a pending re-harvest may start
  uint32_t reharvest_after_{0};

  // Decides when acknowledging on the client's behalf is safe. Armed only by the ASH
  // session handshake, so a bootloader or Thread NCP never triggers it.
  AshDetector detector_;
};

extern ZigbeeProxy *global_zigbee_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zigbee_proxy

#endif  // USE_ZIGBEE_PROXY
