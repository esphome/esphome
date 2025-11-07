#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP_IDF
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#else
#include <WiFiClient.h>
#endif

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace esphome {
namespace network_serial {

static const char *const TAG = "network_serial";

//========================================================================
// Telnet Protocol Constants (RFC 854)
//========================================================================

/// Telnet command codes
enum TelnetCommand : uint8_t {
  TELNET_SE = 240,     ///< End of subnegotiation parameters
  TELNET_NOP = 241,    ///< No operation
  TELNET_DM = 242,     ///< Data mark
  TELNET_BRK = 243,    ///< Break
  TELNET_IP = 244,     ///< Interrupt process
  TELNET_AO = 245,     ///< Abort output
  TELNET_AYT = 246,    ///< Are you there
  TELNET_EC = 247,     ///< Erase character
  TELNET_EL = 248,     ///< Erase line
  TELNET_GA = 249,     ///< Go ahead
  TELNET_SB = 250,     ///< Subnegotiation begin
  TELNET_WILL = 251,   ///< Will (option negotiation)
  TELNET_WONT = 252,   ///< Won't (option negotiation)
  TELNET_DO = 253,     ///< Do (option negotiation)
  TELNET_DONT = 254,   ///< Don't (option negotiation)
  TELNET_IAC = 255,    ///< Interpret as command
};

/// Telnet option codes
enum TelnetOption : uint8_t {
  TELNET_BINARY = 0,          ///< Binary transmission
  TELNET_ECHO = 1,            ///< Echo
  TELNET_SUPPRESS_GO_AHEAD = 3, ///< Suppress Go Ahead
  TELNET_STATUS = 5,          ///< Status
  TELNET_TIMING_MARK = 6,     ///< Timing mark
  TELNET_COM_PORT = 44,       ///< Com Port Control (RFC 2217)
};

//========================================================================
// RFC 2217 Com Port Control Constants
//========================================================================

/// RFC 2217 server-to-client commands
enum RFC2217ServerCommand : uint8_t {
  RFC2217_SET_BAUDRATE = 1,      ///< Set baud rate
  RFC2217_SET_DATASIZE = 2,      ///< Set data size (5-8 bits)
  RFC2217_SET_PARITY = 3,        ///< Set parity
  RFC2217_SET_STOPSIZE = 4,      ///< Set stop bits
  RFC2217_SET_CONTROL = 5,       ///< Set control (flow control)
  RFC2217_NOTIFY_LINESTATE = 6,  ///< Line state notification
  RFC2217_NOTIFY_MODEMSTATE = 7, ///< Modem state notification
  RFC2217_FLOWCONTROL_SUSPEND = 8,   ///< Flow control suspend
  RFC2217_FLOWCONTROL_RESUME = 9,    ///< Flow control resume
  RFC2217_SET_LINESTATE_MASK = 10,   ///< Set line state mask
  RFC2217_SET_MODEMSTATE_MASK = 11,  ///< Set modem state mask
  RFC2217_PURGE_DATA = 12,       ///< Purge data
};

/// RFC 2217 client-to-server commands (add 100 to server command)
enum RFC2217ClientCommand : uint8_t {
  RFC2217_SET_BAUDRATE_CS = 101,
  RFC2217_SET_DATASIZE_CS = 102,
  RFC2217_SET_PARITY_CS = 103,
  RFC2217_SET_STOPSIZE_CS = 104,
  RFC2217_SET_CONTROL_CS = 105,
  RFC2217_NOTIFY_LINESTATE_CS = 106,
  RFC2217_NOTIFY_MODEMSTATE_CS = 107,
  RFC2217_FLOWCONTROL_SUSPEND_CS = 108,
  RFC2217_FLOWCONTROL_RESUME_CS = 109,
  RFC2217_SET_LINESTATE_MASK_CS = 110,
  RFC2217_SET_MODEMSTATE_MASK_CS = 111,
  RFC2217_PURGE_DATA_CS = 112,
};

/// Parity modes
enum ParityMode : uint8_t {
  PARITY_NONE = 1,
  PARITY_ODD = 2,
  PARITY_EVEN = 3,
  PARITY_MARK = 4,
  PARITY_SPACE = 5,
};

/// Data size (bits per character)
enum DataSize : uint8_t {
  DATA_SIZE_5 = 5,
  DATA_SIZE_6 = 6,
  DATA_SIZE_7 = 7,
  DATA_SIZE_8 = 8,
};

/// Stop bits
enum StopBits : uint8_t {
  STOP_BITS_1 = 1,
  STOP_BITS_2 = 2,
  STOP_BITS_1_5 = 3,
};

/// Flow control modes
enum FlowControl : uint8_t {
  FLOW_NONE = 1,
  FLOW_XONXOFF = 2,
  FLOW_HARDWARE = 3,
};

/// Modem state bits
enum ModemState : uint8_t {
  MODEM_STATE_DELTA_CTS = (1 << 0),  ///< CTS changed
  MODEM_STATE_DELTA_DSR = (1 << 1),  ///< DSR changed
  MODEM_STATE_TRAILING_EDGE_RI = (1 << 2), ///< RI trailing edge
  MODEM_STATE_DELTA_DCD = (1 << 3),  ///< DCD changed
  MODEM_STATE_CTS = (1 << 4),        ///< CTS state
  MODEM_STATE_DSR = (1 << 5),        ///< DSR state
  MODEM_STATE_RI = (1 << 6),         ///< RI state
  MODEM_STATE_DCD = (1 << 7),        ///< DCD state
};

/// Line state bits
enum LineState : uint8_t {
  LINE_STATE_DATA_READY = (1 << 0),        ///< Data ready
  LINE_STATE_OVERRUN_ERROR = (1 << 1),     ///< Overrun error
  LINE_STATE_PARITY_ERROR = (1 << 2),      ///< Parity error
  LINE_STATE_FRAMING_ERROR = (1 << 3),     ///< Framing error
  LINE_STATE_BREAK_DETECT = (1 << 4),      ///< Break detected
  LINE_STATE_TX_HOLDING_EMPTY = (1 << 5),  ///< TX holding register empty
  LINE_STATE_TX_SHIFT_EMPTY = (1 << 6),    ///< TX shift register empty
  LINE_STATE_TIMEOUT_ERROR = (1 << 7),     ///< Timeout error
};

/// Control signal bits
enum ControlSignal : uint8_t {
  CONTROL_DTR = (1 << 0),       ///< DTR (Data Terminal Ready)
  CONTROL_RTS = (1 << 1),       ///< RTS (Request To Send)
  CONTROL_DTR_ON = (1 << 3),    ///< DTR on
  CONTROL_DTR_OFF = (1 << 4),   ///< DTR off
  CONTROL_RTS_ON = (1 << 5),    ///< RTS on
  CONTROL_RTS_OFF = (1 << 6),   ///< RTS off
  CONTROL_FLOW_CONTROL = (1 << 7), ///< Flow control
};

/// Purge data flags
enum PurgeData : uint8_t {
  PURGE_RX_BUFFER = 1,  ///< Purge receive buffer
  PURGE_TX_BUFFER = 2,  ///< Purge transmit buffer
  PURGE_BOTH = 3,       ///< Purge both buffers
};

//========================================================================
// Serial Port Configuration
//========================================================================

/**
 * @brief Serial port configuration structure
 */
struct SerialConfig {
  uint32_t baudrate{9600};
  DataSize data_size{DATA_SIZE_8};
  ParityMode parity{PARITY_NONE};
  StopBits stop_bits{STOP_BITS_1};
  FlowControl flow_control{FLOW_NONE};
  bool dtr{false};
  bool rts{false};

  SerialConfig() = default;
};

//========================================================================
// Network Serial Client Component
//========================================================================

/**
 * @brief RFC 2217 Network Serial Client
 *
 * Implements full RFC 2217 protocol for serial port control over Telnet/TCP.
 * Provides virtual serial port accessible over network with complete control
 * over baud rate, parity, data bits, stop bits, flow control, and modem signals.
 *
 * Features:
 * - Full RFC 2217 Com Port Control implementation
 * - Telnet protocol negotiation (RFC 854)
 * - Configurable serial parameters (baud, parity, data bits, stop bits)
 * - Flow control support (None, XON/XOFF, Hardware)
 * - Modem control signals (DTR, RTS, DSR, CTS, DCD, RI)
 * - Line state notifications (errors, break detection)
 * - Automatic reconnection on disconnect
 * - Buffered I/O with flow control
 * - Storage host integration as /dev/ttyNET0
 *
 * Example configuration:
 * @code
 * network_serial:
 *   - id: my_serial
 *     host: 192.168.1.100
 *     port: 2217
 *     baudrate: 115200
 *     data_bits: 8
 *     parity: NONE
 *     stop_bits: 1
 *     device_node: /dev/ttyNET0  # Optional, for storage_host
 * @endcode
 */
class NetworkSerialClient : public Component {
 public:
  NetworkSerialClient() = default;
  ~NetworkSerialClient() override;

  // Component lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  //========================================================================
  // Configuration
  //========================================================================

  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_baudrate(uint32_t baudrate) { this->config_.baudrate = baudrate; }
  void set_data_bits(uint8_t data_bits);
  void set_parity(const std::string &parity);
  void set_stop_bits(uint8_t stop_bits);
  void set_flow_control(const std::string &flow_control);
  void set_dtr(bool dtr) { this->config_.dtr = dtr; }
  void set_rts(bool rts) { this->config_.rts = rts; }
  void set_device_node(const std::string &device_node) { this->device_node_ = device_node; }

  const std::string &get_device_node() const { return this->device_node_; }

  //========================================================================
  // Storage Host Integration (soft dependency)
  //========================================================================

  /**
   * @brief Register this serial client with storage_host
   */
  void register_with_storage_host();

  //========================================================================
  // Connection Management
  //========================================================================

  bool is_connected() const { return this->connected_; }
  bool connect();
  void disconnect();

  //========================================================================
  // Serial I/O Operations
  //========================================================================

  /**
   * @brief Write data to serial port
   */
  size_t write(const uint8_t *data, size_t length);

  /**
   * @brief Read data from serial port
   */
  size_t read(uint8_t *data, size_t length);

  /**
   * @brief Get number of bytes available to read
   */
  size_t available() const { return this->rx_buffer_.size(); }

  /**
   * @brief Flush transmit buffer
   */
  void flush();

  /**
   * @brief Purge buffers
   */
  void purge(PurgeData purge_flags);

  //========================================================================
  // Serial Configuration (Runtime)
  //========================================================================

  bool set_baudrate_runtime(uint32_t baudrate);
  bool set_data_size_runtime(DataSize data_size);
  bool set_parity_runtime(ParityMode parity);
  bool set_stop_bits_runtime(StopBits stop_bits);
  bool set_flow_control_runtime(FlowControl flow_control);

  //========================================================================
  // Modem Control
  //========================================================================

  bool set_dtr(bool state);
  bool set_rts(bool state);
  uint8_t get_modem_state() const { return this->modem_state_; }
  uint8_t get_line_state() const { return this->line_state_; }

  //========================================================================
  // Callbacks
  //========================================================================

  void add_on_connect_callback(std::function<void()> callback) { this->on_connect_callbacks_.push_back(callback); }
  void add_on_disconnect_callback(std::function<void()> callback) {
    this->on_disconnect_callbacks_.push_back(callback);
  }
  void add_on_data_callback(std::function<void(const std::vector<uint8_t> &)> callback) {
    this->on_data_callbacks_.push_back(callback);
  }

 protected:
  //========================================================================
  // Configuration
  //========================================================================

  std::string host_;
  uint16_t port_{2217};
  SerialConfig config_;
  std::string device_node_;

  //========================================================================
  // Network State
  //========================================================================

#ifdef USE_ESP_IDF
  int socket_{-1};
#else
  std::unique_ptr<WiFiClient> client_;
#endif

  bool connected_{false};
  bool telnet_negotiated_{false};
  uint32_t last_connect_attempt_{0};
  static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;

  //========================================================================
  // Serial State
  //========================================================================

  uint8_t modem_state_{0};
  uint8_t line_state_{0};
  bool flow_suspended_{false};

  //========================================================================
  // Buffers
  //========================================================================

  std::vector<uint8_t> rx_buffer_;
  std::vector<uint8_t> tx_buffer_;
  std::vector<uint8_t> telnet_buffer_;  ///< For Telnet protocol parsing

  static constexpr size_t MAX_BUFFER_SIZE = 2048;

  //========================================================================
  // Callbacks
  //========================================================================

  std::vector<std::function<void()>> on_connect_callbacks_;
  std::vector<std::function<void()>> on_disconnect_callbacks_;
  std::vector<std::function<void(const std::vector<uint8_t> &)>> on_data_callbacks_;

  //========================================================================
  // Internal Operations
  //========================================================================

  bool connect_socket_();
  void close_socket_();
  bool send_raw_(const uint8_t *data, size_t length);
  size_t receive_raw_(uint8_t *buffer, size_t length);

  //========================================================================
  // Telnet Protocol
  //========================================================================

  bool negotiate_telnet_();
  void send_telnet_command_(TelnetCommand cmd, TelnetOption option);
  void send_telnet_subnegotiation_(const uint8_t *data, size_t length);
  void process_telnet_data_(const uint8_t *data, size_t length);
  void handle_telnet_command_(TelnetCommand cmd, TelnetOption option);
  void handle_telnet_subnegotiation_(const uint8_t *data, size_t length);

  //========================================================================
  // RFC 2217 Protocol
  //========================================================================

  bool send_com_port_command_(uint8_t command, const uint8_t *data, size_t length);
  bool send_com_port_command_uint32_(uint8_t command, uint32_t value);
  void handle_com_port_control_(const uint8_t *data, size_t length);
  void apply_serial_config_();
};

}  // namespace network_serial
}  // namespace esphome
