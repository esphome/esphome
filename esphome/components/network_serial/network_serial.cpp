#include "network_serial.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <cstring>
#include <algorithm>

// Forward declare storage_host for soft dependency
#if defined(USE_STORAGE_HOST)
namespace storage_host {
extern class StorageHost *global_storage_host;
}
#endif  // USE_STORAGE_HOST

namespace esphome {
namespace network_serial {

//========================================================================
// NetworkSerialClient Implementation
//========================================================================

NetworkSerialClient::~NetworkSerialClient() { this->disconnect(); }

void NetworkSerialClient::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Network Serial Client...");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Baudrate: %u", this->config_.baudrate);
  ESP_LOGCONFIG(TAG, "  Data bits: %u", this->config_.data_size);
  ESP_LOGCONFIG(TAG, "  Parity: %u", this->config_.parity);
  ESP_LOGCONFIG(TAG, "  Stop bits: %u", this->config_.stop_bits);
  ESP_LOGCONFIG(TAG, "  Flow control: %u", this->config_.flow_control);

  if (!this->device_node_.empty()) {
    ESP_LOGCONFIG(TAG, "  Device node: %s", this->device_node_.c_str());
  }

  // Reserve buffer space
  this->rx_buffer_.reserve(MAX_BUFFER_SIZE);
  this->tx_buffer_.reserve(MAX_BUFFER_SIZE);
  this->telnet_buffer_.reserve(256);

  // Register with storage_host if configured
  if (!this->device_node_.empty()) {
    this->register_with_storage_host();
  }
}

void NetworkSerialClient::loop() {
  // Try to connect if not connected
  if (!this->connected_) {
    uint32_t now = millis();
    if (now - this->last_connect_attempt_ >= RECONNECT_INTERVAL_MS) {
      this->last_connect_attempt_ = now;
      if (this->connect()) {
        ESP_LOGI(TAG, "Connected to %s:%u", this->host_.c_str(), this->port_);
        for (auto &callback : this->on_connect_callbacks_) {
          callback();
        }
      }
    }
    return;
  }

  // Receive data from network
  uint8_t buffer[256];
  size_t received = this->receive_raw_(buffer, sizeof(buffer));
  if (received > 0) {
    this->process_telnet_data_(buffer, received);
  } else if (received == 0) {
    // Connection closed
    ESP_LOGW(TAG, "Connection closed by remote host");
    this->disconnect();
    for (auto &callback : this->on_disconnect_callbacks_) {
      callback();
    }
  }

  // Send pending TX data
  if (!this->tx_buffer_.empty() && !this->flow_suspended_) {
    size_t sent = this->send_raw_(this->tx_buffer_.data(), this->tx_buffer_.size());
    if (sent > 0) {
      this->tx_buffer_.erase(this->tx_buffer_.begin(), this->tx_buffer_.begin() + sent);
    }
  }

  // Notify data callbacks if we have data
  if (!this->rx_buffer_.empty() && !this->on_data_callbacks_.empty()) {
    for (auto &callback : this->on_data_callbacks_) {
      callback(this->rx_buffer_);
    }
  }
}

void NetworkSerialClient::dump_config() {
  ESP_LOGCONFIG(TAG, "Network Serial Client:");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Status: %s", this->connected_ ? "Connected" : "Disconnected");
  if (!this->device_node_.empty()) {
    ESP_LOGCONFIG(TAG, "  Device node: %s", this->device_node_.c_str());
  }
}

void NetworkSerialClient::register_with_storage_host() {
#if defined(USE_STORAGE_HOST)
  // Check if storage_host is available (soft dependency)
  if (storage_host::global_storage_host != nullptr) {
    // Register as network_serial type device node
    // Note: This requires storage_host to support network_serial devices
    // For now, we just log the registration
    ESP_LOGI(TAG, "Network serial device node: %s", this->device_node_.c_str());
    // TODO: Extend storage_host to support network serial devices
  } else {
    ESP_LOGD(TAG, "storage_host not available, skipping device node registration");
  }
#else
  ESP_LOGD(TAG, "storage_host component not compiled, device node registration disabled");
#endif  // USE_STORAGE_HOST
}

//========================================================================
// Configuration Helpers
//========================================================================

void NetworkSerialClient::set_data_bits(uint8_t data_bits) {
  switch (data_bits) {
    case 5:
      this->config_.data_size = DATA_SIZE_5;
      break;
    case 6:
      this->config_.data_size = DATA_SIZE_6;
      break;
    case 7:
      this->config_.data_size = DATA_SIZE_7;
      break;
    case 8:
      this->config_.data_size = DATA_SIZE_8;
      break;
    default:
      ESP_LOGW(TAG, "Invalid data bits: %u, using 8", data_bits);
      this->config_.data_size = DATA_SIZE_8;
  }
}

void NetworkSerialClient::set_parity(const std::string &parity) {
  if (parity == "NONE") {
    this->config_.parity = PARITY_NONE;
  } else if (parity == "EVEN") {
    this->config_.parity = PARITY_EVEN;
  } else if (parity == "ODD") {
    this->config_.parity = PARITY_ODD;
  } else if (parity == "MARK") {
    this->config_.parity = PARITY_MARK;
  } else if (parity == "SPACE") {
    this->config_.parity = PARITY_SPACE;
  } else {
    ESP_LOGW(TAG, "Invalid parity: %s, using NONE", parity.c_str());
    this->config_.parity = PARITY_NONE;
  }
}

void NetworkSerialClient::set_stop_bits(uint8_t stop_bits) {
  if (stop_bits == 1) {
    this->config_.stop_bits = STOP_BITS_1;
  } else if (stop_bits == 2) {
    this->config_.stop_bits = STOP_BITS_2;
  } else {
    ESP_LOGW(TAG, "Invalid stop bits: %u, using 1", stop_bits);
    this->config_.stop_bits = STOP_BITS_1;
  }
}

void NetworkSerialClient::set_flow_control(const std::string &flow_control) {
  if (flow_control == "NONE") {
    this->config_.flow_control = FLOW_NONE;
  } else if (flow_control == "SOFTWARE" || flow_control == "XONXOFF") {
    this->config_.flow_control = FLOW_XONXOFF;
  } else if (flow_control == "HARDWARE" || flow_control == "RTS_CTS") {
    this->config_.flow_control = FLOW_HARDWARE;
  } else {
    ESP_LOGW(TAG, "Invalid flow control: %s, using NONE", flow_control.c_str());
    this->config_.flow_control = FLOW_NONE;
  }
}

//========================================================================
// Connection Management
//========================================================================

bool NetworkSerialClient::connect() {
  if (this->connected_) {
    return true;
  }

  ESP_LOGI(TAG, "Connecting to %s:%u...", this->host_.c_str(), this->port_);

  if (!this->connect_socket_()) {
    ESP_LOGW(TAG, "Failed to connect");
    return false;
  }

  // Negotiate Telnet protocol
  if (!this->negotiate_telnet_()) {
    ESP_LOGW(TAG, "Failed to negotiate Telnet protocol");
    this->disconnect();
    return false;
  }

  // Apply serial configuration
  this->apply_serial_config_();

  this->connected_ = true;
  return true;
}

void NetworkSerialClient::disconnect() {
  if (!this->connected_) {
    return;
  }

  ESP_LOGI(TAG, "Disconnecting from %s:%u", this->host_.c_str(), this->port_);

  this->close_socket_();
  this->connected_ = false;
  this->telnet_negotiated_ = false;

  // Clear buffers
  this->rx_buffer_.clear();
  this->tx_buffer_.clear();
  this->telnet_buffer_.clear();
}

bool NetworkSerialClient::connect_socket_() {
#ifdef USE_ESP_IDF
  // Create TCP socket
  this->socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (this->socket_ < 0) {
    ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
    return false;
  }

  // Resolve host address
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(this->port_);

  struct hostent *host = gethostbyname(this->host_.c_str());
  if (host == nullptr) {
    ESP_LOGE(TAG, "Failed to resolve host: %s", this->host_.c_str());
    close(this->socket_);
    this->socket_ = -1;
    return false;
  }
  memcpy(&server_addr.sin_addr, host->h_addr, sizeof(server_addr.sin_addr));

  // Connect
  if (::connect(this->socket_, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    ESP_LOGE(TAG, "Failed to connect: errno %d", errno);
    close(this->socket_);
    this->socket_ = -1;
    return false;
  }

  // Set non-blocking mode
  int flags = fcntl(this->socket_, F_GETFL, 0);
  fcntl(this->socket_, F_SETFL, flags | O_NONBLOCK);

  // Set TCP_NODELAY for low latency
  int nodelay = 1;
  setsockopt(this->socket_, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

  return true;
#else
  // Arduino WiFiClient
  this->client_ = std::make_unique<WiFiClient>();
  if (!this->client_->connect(this->host_.c_str(), this->port_)) {
    ESP_LOGE(TAG, "Failed to connect");
    this->client_ = nullptr;
    return false;
  }

  this->client_->setNoDelay(true);  // TCP_NODELAY
  return true;
#endif
}

void NetworkSerialClient::close_socket_() {
#ifdef USE_ESP_IDF
  if (this->socket_ >= 0) {
    close(this->socket_);
    this->socket_ = -1;
  }
#else
  if (this->client_) {
    this->client_->stop();
    this->client_ = nullptr;
  }
#endif
}

bool NetworkSerialClient::send_raw_(const uint8_t *data, size_t length) {
  if (!this->connected_) {
    return false;
  }

#ifdef USE_ESP_IDF
  int sent = send(this->socket_, data, length, 0);
  if (sent < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;  // Would block, try again later
    }
    ESP_LOGW(TAG, "Send error: errno %d", errno);
    return 0;
  }
  return sent;
#else
  if (!this->client_ || !this->client_->connected()) {
    return 0;
  }
  return this->client_->write(data, length);
#endif
}

size_t NetworkSerialClient::receive_raw_(uint8_t *buffer, size_t length) {
  if (!this->connected_) {
    return 0;
  }

#ifdef USE_ESP_IDF
  int received = recv(this->socket_, buffer, length, 0);
  if (received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;  // No data available
    }
    ESP_LOGW(TAG, "Receive error: errno %d", errno);
    return 0;
  }
  return received;
#else
  if (!this->client_ || !this->client_->connected()) {
    return 0;
  }
  int available = this->client_->available();
  if (available <= 0) {
    return 0;
  }
  size_t to_read = std::min((size_t) available, length);
  return this->client_->read(buffer, to_read);
#endif
}

//========================================================================
// Serial I/O Operations
//========================================================================

size_t NetworkSerialClient::write(const uint8_t *data, size_t length) {
  if (!this->connected_ || this->flow_suspended_) {
    return 0;
  }

  // Add to TX buffer
  size_t space = MAX_BUFFER_SIZE - this->tx_buffer_.size();
  size_t to_write = std::min(length, space);

  // Escape IAC bytes (0xFF) by doubling them
  for (size_t i = 0; i < to_write; i++) {
    this->tx_buffer_.push_back(data[i]);
    if (data[i] == TELNET_IAC) {
      this->tx_buffer_.push_back(TELNET_IAC);  // Double IAC
    }
  }

  return to_write;
}

size_t NetworkSerialClient::read(uint8_t *data, size_t length) {
  size_t to_read = std::min(length, this->rx_buffer_.size());
  if (to_read > 0) {
    std::copy(this->rx_buffer_.begin(), this->rx_buffer_.begin() + to_read, data);
    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + to_read);
  }
  return to_read;
}

void NetworkSerialClient::flush() {
  while (!this->tx_buffer_.empty() && this->connected_ && !this->flow_suspended_) {
    size_t sent = this->send_raw_(this->tx_buffer_.data(), this->tx_buffer_.size());
    if (sent > 0) {
      this->tx_buffer_.erase(this->tx_buffer_.begin(), this->tx_buffer_.begin() + sent);
    } else {
      delay(10);
    }
  }
}

void NetworkSerialClient::purge(PurgeData purge_flags) {
  if (purge_flags == PURGE_RX_BUFFER || purge_flags == PURGE_BOTH) {
    this->rx_buffer_.clear();
  }
  if (purge_flags == PURGE_TX_BUFFER || purge_flags == PURGE_BOTH) {
    this->tx_buffer_.clear();
  }

  // Send purge command to server
  if (this->connected_ && this->telnet_negotiated_) {
    uint8_t cmd_data[] = {static_cast<uint8_t>(purge_flags)};
    this->send_com_port_command_(RFC2217_PURGE_DATA_CS, cmd_data, sizeof(cmd_data));
  }
}

//========================================================================
// Telnet Protocol Implementation
//========================================================================

bool NetworkSerialClient::negotiate_telnet_() {
  ESP_LOGD(TAG, "Negotiating Telnet protocol...");

  // Send: IAC WILL BINARY
  this->send_telnet_command_(TELNET_WILL, TELNET_BINARY);

  // Send: IAC WILL ECHO
  this->send_telnet_command_(TELNET_WILL, TELNET_ECHO);

  // Send: IAC WILL SUPPRESS_GO_AHEAD
  this->send_telnet_command_(TELNET_WILL, TELNET_SUPPRESS_GO_AHEAD);

  // Send: IAC DO COM_PORT (RFC 2217)
  this->send_telnet_command_(TELNET_DO, TELNET_COM_PORT);

  // Wait for server responses (simplified - in production should wait for actual responses)
  delay(100);

  this->telnet_negotiated_ = true;
  ESP_LOGD(TAG, "Telnet negotiation complete");

  return true;
}

void NetworkSerialClient::send_telnet_command_(TelnetCommand cmd, TelnetOption option) {
  uint8_t buffer[3] = {TELNET_IAC, cmd, option};
  this->send_raw_(buffer, sizeof(buffer));
  ESP_LOGVV(TAG, "Sent Telnet: IAC %u %u", cmd, option);
}

void NetworkSerialClient::send_telnet_subnegotiation_(const uint8_t *data, size_t length) {
  // Send: IAC SB <data> IAC SE
  std::vector<uint8_t> buffer;
  buffer.push_back(TELNET_IAC);
  buffer.push_back(TELNET_SB);
  buffer.insert(buffer.end(), data, data + length);
  buffer.push_back(TELNET_IAC);
  buffer.push_back(TELNET_SE);

  this->send_raw_(buffer.data(), buffer.size());
  ESP_LOGVV(TAG, "Sent subnegotiation: %zu bytes", length);
}

void NetworkSerialClient::process_telnet_data_(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = data[i];

    // Check for IAC (Interpret As Command)
    if (byte == TELNET_IAC) {
      // Start of Telnet command
      if (i + 1 < length) {
        uint8_t cmd = data[++i];

        if (cmd == TELNET_IAC) {
          // Escaped IAC (0xFF 0xFF) -> single 0xFF data byte
          this->rx_buffer_.push_back(TELNET_IAC);
        } else if (cmd == TELNET_SB) {
          // Subnegotiation begin
          std::vector<uint8_t> subneg_data;
          i++;
          while (i < length) {
            if (data[i] == TELNET_IAC && i + 1 < length && data[i + 1] == TELNET_SE) {
              // End of subnegotiation
              i++;  // Skip SE
              this->handle_telnet_subnegotiation_(subneg_data.data(), subneg_data.size());
              break;
            }
            subneg_data.push_back(data[i++]);
          }
        } else if (cmd == TELNET_WILL || cmd == TELNET_WONT || cmd == TELNET_DO || cmd == TELNET_DONT) {
          // Option negotiation
          if (i + 1 < length) {
            uint8_t option = data[++i];
            this->handle_telnet_command_(static_cast<TelnetCommand>(cmd), static_cast<TelnetOption>(option));
          }
        }
        // Other commands ignored
      }
    } else {
      // Regular data byte
      if (this->rx_buffer_.size() < MAX_BUFFER_SIZE) {
        this->rx_buffer_.push_back(byte);
      }
    }
  }
}

void NetworkSerialClient::handle_telnet_command_(TelnetCommand cmd, TelnetOption option) {
  ESP_LOGVV(TAG, "Telnet command: %u %u", cmd, option);

  // Handle option negotiation
  if (cmd == TELNET_DO && option == TELNET_COM_PORT) {
    // Server wants us to enable COM_PORT option
    this->send_telnet_command_(TELNET_WILL, TELNET_COM_PORT);
  } else if (cmd == TELNET_WILL && option == TELNET_COM_PORT) {
    // Server will enable COM_PORT option
    this->send_telnet_command_(TELNET_DO, TELNET_COM_PORT);
  }
  // Other negotiations: accept or reject as needed
}

void NetworkSerialClient::handle_telnet_subnegotiation_(const uint8_t *data, size_t length) {
  if (length < 1) {
    return;
  }

  // Check if this is COM_PORT subnegotiation
  if (data[0] == TELNET_COM_PORT) {
    if (length >= 2) {
      this->handle_com_port_control_(data + 1, length - 1);
    }
  }
}

//========================================================================
// RFC 2217 Com Port Control Implementation
//========================================================================

bool NetworkSerialClient::send_com_port_command_(uint8_t command, const uint8_t *data, size_t length) {
  std::vector<uint8_t> subneg_data;
  subneg_data.push_back(TELNET_COM_PORT);  // Option code
  subneg_data.push_back(command);          // Command code
  if (data && length > 0) {
    subneg_data.insert(subneg_data.end(), data, data + length);
  }

  this->send_telnet_subnegotiation_(subneg_data.data(), subneg_data.size());
  return true;
}

bool NetworkSerialClient::send_com_port_command_uint32_(uint8_t command, uint32_t value) {
  // Send value as big-endian 32-bit integer
  uint8_t data[4];
  data[0] = (value >> 24) & 0xFF;
  data[1] = (value >> 16) & 0xFF;
  data[2] = (value >> 8) & 0xFF;
  data[3] = value & 0xFF;
  return this->send_com_port_command_(command, data, sizeof(data));
}

void NetworkSerialClient::handle_com_port_control_(const uint8_t *data, size_t length) {
  if (length < 1) {
    return;
  }

  uint8_t command = data[0];
  const uint8_t *param_data = data + 1;
  size_t param_length = length - 1;

  ESP_LOGVV(TAG, "RFC2217 command: %u, params: %zu bytes", command, param_length);

  switch (command) {
    case RFC2217_SET_BAUDRATE:
      if (param_length >= 4) {
        uint32_t baudrate = (param_data[0] << 24) | (param_data[1] << 16) | (param_data[2] << 8) | param_data[3];
        ESP_LOGD(TAG, "Server set baudrate: %u", baudrate);
        this->config_.baudrate = baudrate;
      }
      break;

    case RFC2217_SET_DATASIZE:
      if (param_length >= 1) {
        ESP_LOGD(TAG, "Server set data size: %u", param_data[0]);
        this->config_.data_size = static_cast<DataSize>(param_data[0]);
      }
      break;

    case RFC2217_SET_PARITY:
      if (param_length >= 1) {
        ESP_LOGD(TAG, "Server set parity: %u", param_data[0]);
        this->config_.parity = static_cast<ParityMode>(param_data[0]);
      }
      break;

    case RFC2217_SET_STOPSIZE:
      if (param_length >= 1) {
        ESP_LOGD(TAG, "Server set stop bits: %u", param_data[0]);
        this->config_.stop_bits = static_cast<StopBits>(param_data[0]);
      }
      break;

    case RFC2217_SET_CONTROL:
      if (param_length >= 1) {
        ESP_LOGD(TAG, "Server set control: 0x%02X", param_data[0]);
        this->config_.flow_control = static_cast<FlowControl>(param_data[0]);
      }
      break;

    case RFC2217_NOTIFY_LINESTATE:
      if (param_length >= 1) {
        this->line_state_ = param_data[0];
        ESP_LOGVV(TAG, "Line state: 0x%02X", this->line_state_);
      }
      break;

    case RFC2217_NOTIFY_MODEMSTATE:
      if (param_length >= 1) {
        this->modem_state_ = param_data[0];
        ESP_LOGVV(TAG, "Modem state: 0x%02X (CTS=%d DSR=%d RI=%d DCD=%d)", this->modem_state_,
                  !!(this->modem_state_ & MODEM_STATE_CTS), !!(this->modem_state_ & MODEM_STATE_DSR),
                  !!(this->modem_state_ & MODEM_STATE_RI), !!(this->modem_state_ & MODEM_STATE_DCD));
      }
      break;

    case RFC2217_FLOWCONTROL_SUSPEND:
      ESP_LOGD(TAG, "Flow control suspended");
      this->flow_suspended_ = true;
      break;

    case RFC2217_FLOWCONTROL_RESUME:
      ESP_LOGD(TAG, "Flow control resumed");
      this->flow_suspended_ = false;
      break;

    default:
      ESP_LOGV(TAG, "Unhandled RFC2217 command: %u", command);
      break;
  }
}

void NetworkSerialClient::apply_serial_config_() {
  if (!this->connected_ || !this->telnet_negotiated_) {
    return;
  }

  ESP_LOGD(TAG, "Applying serial configuration...");

  // Set baudrate
  this->send_com_port_command_uint32_(RFC2217_SET_BAUDRATE_CS, this->config_.baudrate);

  // Set data size
  uint8_t data_size = this->config_.data_size;
  this->send_com_port_command_(RFC2217_SET_DATASIZE_CS, &data_size, 1);

  // Set parity
  uint8_t parity = this->config_.parity;
  this->send_com_port_command_(RFC2217_SET_PARITY_CS, &parity, 1);

  // Set stop bits
  uint8_t stop_bits = this->config_.stop_bits;
  this->send_com_port_command_(RFC2217_SET_STOPSIZE_CS, &stop_bits, 1);

  // Set flow control
  uint8_t flow_control = this->config_.flow_control;
  this->send_com_port_command_(RFC2217_SET_CONTROL_CS, &flow_control, 1);

  // Set DTR/RTS
  uint8_t control = 0;
  if (this->config_.dtr) {
    control |= CONTROL_DTR_ON;
  } else {
    control |= CONTROL_DTR_OFF;
  }
  if (this->config_.rts) {
    control |= CONTROL_RTS_ON;
  } else {
    control |= CONTROL_RTS_OFF;
  }
  this->send_com_port_command_(RFC2217_NOTIFY_MODEMSTATE_CS, &control, 1);

  ESP_LOGD(TAG, "Serial configuration applied");
}

//========================================================================
// Runtime Configuration
//========================================================================

bool NetworkSerialClient::set_baudrate_runtime(uint32_t baudrate) {
  this->config_.baudrate = baudrate;
  if (this->connected_ && this->telnet_negotiated_) {
    return this->send_com_port_command_uint32_(RFC2217_SET_BAUDRATE_CS, baudrate);
  }
  return false;
}

bool NetworkSerialClient::set_data_size_runtime(DataSize data_size) {
  this->config_.data_size = data_size;
  if (this->connected_ && this->telnet_negotiated_) {
    uint8_t value = data_size;
    return this->send_com_port_command_(RFC2217_SET_DATASIZE_CS, &value, 1);
  }
  return false;
}

bool NetworkSerialClient::set_parity_runtime(ParityMode parity) {
  this->config_.parity = parity;
  if (this->connected_ && this->telnet_negotiated_) {
    uint8_t value = parity;
    return this->send_com_port_command_(RFC2217_SET_PARITY_CS, &value, 1);
  }
  return false;
}

bool NetworkSerialClient::set_stop_bits_runtime(StopBits stop_bits) {
  this->config_.stop_bits = stop_bits;
  if (this->connected_ && this->telnet_negotiated_) {
    uint8_t value = stop_bits;
    return this->send_com_port_command_(RFC2217_SET_STOPSIZE_CS, &value, 1);
  }
  return false;
}

bool NetworkSerialClient::set_flow_control_runtime(FlowControl flow_control) {
  this->config_.flow_control = flow_control;
  if (this->connected_ && this->telnet_negotiated_) {
    uint8_t value = flow_control;
    return this->send_com_port_command_(RFC2217_SET_CONTROL_CS, &value, 1);
  }
  return false;
}

//========================================================================
// Modem Control
//========================================================================

bool NetworkSerialClient::set_dtr(bool state) {
  this->config_.dtr = state;
  if (this->connected_ && this->telnet_negotiated_) {
    uint8_t control = state ? CONTROL_DTR_ON : CONTROL_DTR_OFF;
    return this->send_com_port_command_(RFC2217_NOTIFY_MODEMSTATE_CS, &control, 1);
  }
  return false;
}

bool NetworkSerialClient::set_rts(bool state) {
  this->config_.rts = state;
  if (this->connected_ && this->telnet_negotiated_) {
    uint8_t control = state ? CONTROL_RTS_ON : CONTROL_RTS_OFF;
    return this->send_com_port_command_(RFC2217_NOTIFY_MODEMSTATE_CS, &control, 1);
  }
  return false;
}

}  // namespace network_serial
}  // namespace esphome
