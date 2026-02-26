#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZIGBEE_PROXY

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "ash_protocol.h"

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
};

// Boot-time initialization state machine
enum class BootState : uint8_t {
  IDLE,                     // Not initializing
  WAIT_RSTACK,              // Sent RST, waiting for RSTACK
  SEND_VERSION,             // Send EZSP version command
  WAIT_VERSION,             // Waiting for version response
  SEND_NETWORK_INIT,        // Send networkInit command
  WAIT_STACK_STATUS,        // Waiting for stackStatusHandler callback
  SEND_GET_NETWORK_PARAMS,  // Send getNetworkParameters command
  WAIT_NETWORK_PARAMS,      // Waiting for network parameters response
  SEND_FINAL_RST,           // Send final RST to reset NCP
  WAIT_FINAL_RSTACK,        // Waiting for final RSTACK
  COMPLETE,                 // Boot sequence complete
  FAILED,                   // Boot sequence failed
};

class ZigbeeProxy : public uart::UARTDevice, public Component {
 public:
  ZigbeeProxy();

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  bool can_proceed() override;

  // API integration
  void api_connection_authenticated(api::APIConnection *conn);
  void zigbee_proxy_request(api::APIConnection *api_connection, const api::ZigbeeProxyRequest &msg);
  void zigbee_proxy_frame(api::APIConnection *api_connection, const api::ZigbeeProxyFrame &msg);
  api::APIConnection *get_api_connection() { return this->api_connection_; }

  // Feature flags
  uint32_t get_feature_flags() const { return ZigbeeProxyFeature::FEATURE_ZIGBEE_PROXY_ENABLED; }

  // Network information accessors
  const NetworkInfo &get_network_info() const { return this->network_info_; }
  uint64_t get_ieee_address() const;

  // Frame sending (from API client to NCP)
  void send_frame(const uint8_t *data, size_t length);

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
  bool send_ack_frame_(uint8_t ack_num);
  bool send_nak_frame_(uint8_t ack_num);
  bool send_data_frame_(const uint8_t *data, size_t length, bool retransmit = false);

  // Frame parsing and building (implemented in ash_protocol.cpp)
  bool parse_byte_(uint8_t byte);
  void parse_control_byte_(uint8_t control);
  bool validate_frame_crc_();
  size_t build_frame_(uint8_t *output, const uint8_t *data, size_t length, AshFrameType type, uint8_t frame_num = 0,
                      uint8_t ack_num = 0, bool retx = false);
  uint16_t calculate_crc_(const uint8_t *data, size_t length);

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
  void start_boot_sequence_();
  void advance_boot_state_();
  void handle_boot_data_frame_(const uint8_t *data, size_t length);
  void send_ezsp_version_();
  void send_network_init_();
  void send_get_network_params_();
  void handle_version_response_(const uint8_t *data, size_t length);
  void handle_stack_status_(const uint8_t *data, size_t length);
  void handle_network_params_response_(const uint8_t *data, size_t length);

  // IEEE address and network info
  bool set_ieee_address_(const uint8_t *new_address);
  void send_network_info_changed_msg_(api::APIConnection *conn = nullptr);

  // WiFi/Zigbee channel conflict detection
  void check_wifi_zigbee_conflict_();

  // Bootloader detection
  void check_bootloader_mode_(const uint8_t *data, size_t length);

  // UART processing
  void process_uart_();

  // Client-side (left) ASH session
  void client_parse_byte_(uint8_t byte);
  void client_parse_control_byte_(uint8_t control);
  bool client_validate_frame_crc_();
  void client_send_ack_frame_(uint8_t ack_num);
  void client_send_rstack_frame_(uint8_t reset_code);
  void client_send_data_frame_(const uint8_t *data, size_t length);
  void client_send_error_frame_(uint8_t error_code);
  void client_send_raw_frame_(const uint8_t *frame, size_t length);
  void client_reset_session_();

  // Send raw bytes to API client
  void send_to_client_(const uint8_t *data, size_t length);

  // Forward NCP frames to client
  void forward_ncp_data_to_client_(const uint8_t *payload, size_t length);
  void forward_ncp_rstack_to_client_(const uint8_t *data, size_t length);
  void forward_ncp_error_to_client_(const uint8_t *data, size_t length);

  // Pre-allocated message - always ready to send
  api::ZigbeeProxyFrame outgoing_proto_msg_;

  // NCP-side (right) ASH buffers
  std::array<uint8_t, MAX_ASH_FRAME_SIZE> rx_buffer_;
  std::array<uint8_t, MAX_ASH_FRAME_SIZE> tx_buffer_;
  std::array<uint8_t, MAX_ASH_FRAME_SIZE> tx_pending_buffer_;  // For retransmission

  // Client-side (left) ASH buffers
  std::array<uint8_t, MAX_ASH_FRAME_SIZE> client_rx_buffer_;
  std::array<uint8_t, MAX_ASH_FRAME_SIZE> client_tx_buffer_;

  // Network information
  NetworkInfo network_info_;

  // Timeout configuration
  TimeoutConfig timeout_config_;

  // Pointers (aligned together)
  api::APIConnection *api_connection_{nullptr};  // Current subscribed client

  // NCP-side (right) 32-bit values
  uint32_t setup_time_{0};       // Time when last RST frame was sent
  uint32_t boot_start_time_{0};  // Time when the boot sequence began (for overall timeout)
  uint32_t ack_timer_start_{0};  // Time when ACK timer started
  uint32_t last_rtt_ms_{0};      // Last measured round-trip time

  // NCP-side (right) 16-bit values
  uint16_t rx_buffer_index_{0};    // Index for populating rx_buffer_
  uint16_t tx_pending_length_{0};  // Length of pending TX frame for retransmission
  uint16_t calculated_crc_{0};     // CRC calculated during frame reception

  // Client-side (left) 16-bit values
  uint16_t client_rx_buffer_index_{0};

  // NCP-side (right) 8-bit values
  uint8_t tx_sequence_{0};           // TX sequence number (0-7)
  uint8_t rx_sequence_{0};           // RX sequence number (0-7)
  uint8_t tx_retry_count_{0};        // Number of retransmission attempts
  uint8_t tx_pending_frame_num_{0};  // Frame number of pending TX frame
  uint8_t last_ack_sent_{0};         // Last ACK number sent

  // Client-side (left) 8-bit values
  uint8_t client_tx_sequence_{0};  // Client-facing TX sequence (proxy → client)
  uint8_t client_rx_sequence_{0};  // Client-facing RX sequence (client → proxy)

  // NCP-side enums and booleans
  AshState ash_state_{AshState::DISCONNECTED};
  ParsingState parsing_state_{ParsingState::WAIT_FLAG_START};
  BootloaderState bootloader_state_{BootloaderState::NORMAL};
  BootState boot_state_{BootState::IDLE};

  // Client-side enums and booleans
  AshState client_ash_state_{AshState::DISCONNECTED};
  ParsingState client_parsing_state_{ParsingState::WAIT_FLAG_START};

  uint8_t ezsp_version_{0};            // NCP's EZSP protocol version
  uint8_t ezsp_sequence_{0};           // EZSP frame sequence number
  uint8_t ezsp_requested_version_{0};  // Version we last requested (for re-negotiation)

  bool tx_buffer_pending_{false};        // True if waiting for ACK from NCP
  bool escape_next_byte_{false};         // True if next NCP byte should be unescaped
  bool client_escape_next_byte_{false};  // True if next client byte should be unescaped
  bool network_info_ready_{false};       // True when network info retrieved
  bool boot_sequence_active_{false};     // True during boot-time init
};

extern ZigbeeProxy *global_zigbee_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zigbee_proxy

#endif  // USE_ZIGBEE_PROXY
