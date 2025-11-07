#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP_IDF
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#else
#include <WiFiUdp.h>
#endif

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace esphome {
namespace tftp_server {

static const char *const TAG = "tftp_server";

//========================================================================
// TFTP Protocol Constants (RFC 1350) - Reused from tftp_client
//========================================================================

static constexpr size_t TFTP_DATA_SIZE = 512;
static constexpr size_t TFTP_MAX_PACKET_SIZE = 516;
static constexpr uint32_t TFTP_TIMEOUT_MS = 5000;
static constexpr uint16_t TFTP_DEFAULT_PORT = 69;

enum TFTPOpcode : uint16_t {
  TFTP_OPCODE_RRQ = 1,
  TFTP_OPCODE_WRQ = 2,
  TFTP_OPCODE_DATA = 3,
  TFTP_OPCODE_ACK = 4,
  TFTP_OPCODE_ERROR = 5,
};

enum TFTPErrorCode : uint16_t {
  TFTP_ERROR_NOT_DEFINED = 0,
  TFTP_ERROR_FILE_NOT_FOUND = 1,
  TFTP_ERROR_ACCESS_VIOLATION = 2,
  TFTP_ERROR_DISK_FULL = 3,
  TFTP_ERROR_ILLEGAL_OPERATION = 4,
  TFTP_ERROR_UNKNOWN_TRANSFER_ID = 5,
  TFTP_ERROR_FILE_EXISTS = 6,
  TFTP_ERROR_NO_SUCH_USER = 7,
};

enum TFTPMode {
  TFTP_MODE_OCTET,
  TFTP_MODE_NETASCII
};

//========================================================================
// TFTP Packet Helper Functions
//========================================================================

struct TFTPPacket {
  uint16_t opcode;
  std::vector<uint8_t> data;

  static TFTPPacket parse(const uint8_t *buffer, size_t length);
  std::vector<uint8_t> serialize() const;

  static TFTPPacket create_data(uint16_t block_number, const uint8_t *data, size_t length);
  static TFTPPacket create_ack(uint16_t block_number);
  static TFTPPacket create_error(TFTPErrorCode error_code, const std::string &error_msg);

  uint16_t get_block_number() const;
  std::string get_filename() const;
  std::string get_mode() const;
};

//========================================================================
// Access Control
//========================================================================

enum AccessMode : uint8_t {
  ACCESS_READ_ONLY = 1,   ///< Read-only access
  ACCESS_WRITE_ONLY = 2,  ///< Write-only access
  ACCESS_READ_WRITE = 3,  ///< Read and write access
};

//========================================================================
// TFTP Session Management
//========================================================================

/**
 * @brief TFTP session for a single client transfer
 *
 * Manages state for one file transfer (read or write) with a client.
 */
struct TFTPSession {
  std::string client_addr;
  uint16_t client_port;
  std::string filename;
  TFTPMode mode;
  bool is_write;  ///< true for WRQ (write), false for RRQ (read)

  // Transfer state
  uint16_t current_block;
  std::vector<uint8_t> file_data;  ///< For reads: file content, For writes: received data
  size_t bytes_transferred;
  uint32_t last_activity;  ///< millis() of last packet

  // File handle (if using storage)
  void *file_handle;  ///< Platform-specific file handle

  TFTPSession() : client_port(0), mode(TFTP_MODE_OCTET), is_write(false), current_block(0), bytes_transferred(0),
                  last_activity(0), file_handle(nullptr) {}

  bool is_expired(uint32_t timeout_ms) const {
    return (millis() - this->last_activity) > timeout_ms;
  }

  std::string get_session_key() const {
    return this->client_addr + ":" + std::to_string(this->client_port);
  }
};

//========================================================================
// TFTP Server Component
//========================================================================

/**
 * @brief TFTP Server for serving files over network
 *
 * Implements RFC 1350 TFTP server protocol for serving files from ESP32.
 * Can serve files from LittleFS, SPIFFS, SD card, or storage_host virtual filesystem.
 *
 * Features:
 * - Serve files over UDP (TFTP protocol)
 * - Support for read (RRQ) and write (WRQ) requests
 * - Multiple simultaneous client sessions
 * - Configurable access control (read-only, write-only, read-write)
 * - Configurable root directory
 * - Storage host integration
 * - Automatic session cleanup on timeout
 * - File size limits for safety
 *
 * Example configuration:
 * @code
 * tftp_server:
 *   port: 69  # Optional, default 69
 *   root_dir: /data  # Optional, default /
 *   access_mode: READ_WRITE  # Optional, default READ_ONLY
 *   max_file_size: 1048576  # Optional, default 1MB
 *   max_sessions: 4  # Optional, default 4
 * @endcode
 */
class TFTPServer : public Component {
 public:
  TFTPServer() = default;
  ~TFTPServer() override;

  // Component lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  //========================================================================
  // Configuration
  //========================================================================

  void set_port(uint16_t port) { this->port_ = port; }
  void set_root_dir(const std::string &root_dir) { this->root_dir_ = root_dir; }
  void set_access_mode(AccessMode mode) { this->access_mode_ = mode; }
  void set_max_file_size(size_t max_size) { this->max_file_size_ = max_size; }
  void set_max_sessions(uint8_t max_sessions) { this->max_sessions_ = max_sessions; }

  //========================================================================
  // Server Status
  //========================================================================

  bool is_running() const { return this->running_; }
  size_t get_active_sessions() const { return this->sessions_.size(); }

 protected:
  //========================================================================
  // Configuration
  //========================================================================

  uint16_t port_{TFTP_DEFAULT_PORT};
  std::string root_dir_{"/"};
  AccessMode access_mode_{ACCESS_READ_ONLY};
  size_t max_file_size_{1048576};  ///< 1MB default
  uint8_t max_sessions_{4};

  //========================================================================
  // Server State
  //========================================================================

#ifdef USE_ESP_IDF
  int socket_{-1};
#else
  std::unique_ptr<WiFiUDP> udp_;
#endif

  bool running_{false};

  //========================================================================
  // Session Management
  //========================================================================

  std::map<std::string, std::unique_ptr<TFTPSession>> sessions_;

  //========================================================================
  // Internal Operations
  //========================================================================

  bool init_socket_();
  void close_socket_();
  bool send_packet_(const TFTPPacket &packet, const std::string &addr, uint16_t port);
  bool receive_packet_(TFTPPacket &packet, std::string &addr, uint16_t &port);

  //========================================================================
  // Request Handlers
  //========================================================================

  void handle_rrq_(const std::string &filename, const std::string &mode, const std::string &client_addr,
                   uint16_t client_port);
  void handle_wrq_(const std::string &filename, const std::string &mode, const std::string &client_addr,
                   uint16_t client_port);
  void handle_ack_(uint16_t block_number, const std::string &client_addr, uint16_t client_port);
  void handle_data_(uint16_t block_number, const uint8_t *data, size_t length, const std::string &client_addr,
                    uint16_t client_port);
  void handle_error_(TFTPErrorCode error_code, const std::string &error_msg, const std::string &client_addr,
                     uint16_t client_port);

  //========================================================================
  // Session Management
  //========================================================================

  TFTPSession *get_session_(const std::string &client_addr, uint16_t client_port);
  TFTPSession *create_session_(const std::string &client_addr, uint16_t client_port, const std::string &filename,
                               TFTPMode mode, bool is_write);
  void remove_session_(const std::string &client_addr, uint16_t client_port);
  void cleanup_expired_sessions_();

  //========================================================================
  // File Operations
  //========================================================================

  bool validate_filename_(const std::string &filename);
  std::string get_full_path_(const std::string &filename);
  bool file_exists_(const std::string &path);
  bool can_read_(const std::string &path);
  bool can_write_(const std::string &path);
  bool read_file_(const std::string &path, std::vector<uint8_t> &data);
  bool write_file_(const std::string &path, const uint8_t *data, size_t length);

  //========================================================================
  // Transfer Operations
  //========================================================================

  void send_data_block_(TFTPSession *session);
  void send_error_(const std::string &client_addr, uint16_t client_port, TFTPErrorCode error_code,
                   const std::string &error_msg);
};

}  // namespace tftp_server
}  // namespace esphome
