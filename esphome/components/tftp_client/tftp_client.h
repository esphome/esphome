#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"

#ifdef USE_ESP_IDF
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#else
#include <WiFiUdp.h>
#endif

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace esphome {
namespace tftp_client {

static const char *const TAG = "tftp_client";

//========================================================================
// TFTP Protocol Constants (RFC 1350)
//========================================================================

/// Maximum TFTP data block size (standard: 512 bytes)
static constexpr size_t TFTP_DATA_SIZE = 512;

/// Maximum TFTP packet size (2 byte opcode + 2 byte block + 512 data)
static constexpr size_t TFTP_MAX_PACKET_SIZE = 516;

/// TFTP timeout in milliseconds
static constexpr uint32_t TFTP_TIMEOUT_MS = 5000;

/// Maximum retries for TFTP operations
static constexpr uint8_t TFTP_MAX_RETRIES = 3;

/// TFTP default port
static constexpr uint16_t TFTP_DEFAULT_PORT = 69;

/// TFTP Opcodes
enum TFTPOpcode : uint16_t {
  TFTP_OPCODE_RRQ = 1,    ///< Read request
  TFTP_OPCODE_WRQ = 2,    ///< Write request
  TFTP_OPCODE_DATA = 3,   ///< Data packet
  TFTP_OPCODE_ACK = 4,    ///< Acknowledgment
  TFTP_OPCODE_ERROR = 5,  ///< Error packet
};

/// TFTP Error Codes
enum TFTPErrorCode : uint16_t {
  TFTP_ERROR_NOT_DEFINED = 0,         ///< Not defined
  TFTP_ERROR_FILE_NOT_FOUND = 1,      ///< File not found
  TFTP_ERROR_ACCESS_VIOLATION = 2,    ///< Access violation
  TFTP_ERROR_DISK_FULL = 3,           ///< Disk full
  TFTP_ERROR_ILLEGAL_OPERATION = 4,   ///< Illegal TFTP operation
  TFTP_ERROR_UNKNOWN_TRANSFER_ID = 5, ///< Unknown transfer ID
  TFTP_ERROR_FILE_EXISTS = 6,         ///< File already exists
  TFTP_ERROR_NO_SUCH_USER = 7,        ///< No such user
};

/// TFTP Transfer Mode
enum TFTPMode {
  TFTP_MODE_OCTET,   ///< Binary mode (octet)
  TFTP_MODE_NETASCII ///< ASCII mode (netascii)
};

//========================================================================
// TFTP Packet Structures
//========================================================================

/**
 * @brief TFTP packet structure
 *
 * Handles encoding/decoding of TFTP protocol packets according to RFC 1350
 */
struct TFTPPacket {
  uint16_t opcode;
  std::vector<uint8_t> data;

  /**
   * @brief Create RRQ (Read Request) packet
   */
  static TFTPPacket create_rrq(const std::string &filename, TFTPMode mode = TFTP_MODE_OCTET);

  /**
   * @brief Create WRQ (Write Request) packet
   */
  static TFTPPacket create_wrq(const std::string &filename, TFTPMode mode = TFTP_MODE_OCTET);

  /**
   * @brief Create DATA packet
   */
  static TFTPPacket create_data(uint16_t block_number, const uint8_t *data, size_t length);

  /**
   * @brief Create ACK packet
   */
  static TFTPPacket create_ack(uint16_t block_number);

  /**
   * @brief Create ERROR packet
   */
  static TFTPPacket create_error(TFTPErrorCode error_code, const std::string &error_msg);

  /**
   * @brief Serialize packet to bytes
   */
  std::vector<uint8_t> serialize() const;

  /**
   * @brief Parse packet from bytes
   */
  static TFTPPacket parse(const uint8_t *buffer, size_t length);

  /**
   * @brief Get block number from DATA or ACK packet
   */
  uint16_t get_block_number() const;

  /**
   * @brief Get error code from ERROR packet
   */
  TFTPErrorCode get_error_code() const;

  /**
   * @brief Get error message from ERROR packet
   */
  std::string get_error_message() const;
};

//========================================================================
// TFTP File Operations
//========================================================================

/**
 * @brief TFTP file operation result
 */
struct TFTPResult {
  bool success;
  std::string error_message;
  std::vector<uint8_t> data;  ///< For read operations
  size_t bytes_transferred;

  TFTPResult() : success(false), bytes_transferred(0) {}
  static TFTPResult ok(size_t bytes = 0) {
    TFTPResult result;
    result.success = true;
    result.bytes_transferred = bytes;
    return result;
  }
  static TFTPResult error(const std::string &msg) {
    TFTPResult result;
    result.success = false;
    result.error_message = msg;
    return result;
  }
};

//========================================================================
// TFTP Client Component
//========================================================================

/**
 * @brief TFTP client for network file access
 *
 * Implements TFTP protocol (RFC 1350) for reading and writing files
 * to/from a remote TFTP server. Integrates with storage_host to provide
 * a virtual mount point for TFTP file access.
 *
 * Features:
 * - Read files from TFTP server
 * - Write files to TFTP server
 * - Configurable server address and port
 * - Automatic retry on timeout
 * - Storage host integration (soft dependency)
 *
 * Example configuration:
 * @code
 * tftp_client:
 *   - id: my_tftp
 *     server: 192.168.1.100
 *     port: 69  # Optional, default 69
 *     mount_path: /tftp/server1  # Optional, for storage_host integration
 * @endcode
 */
class TFTPClient : public Component {
 public:
  TFTPClient() = default;
  ~TFTPClient() override;

  // Component lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  //========================================================================
  // Configuration
  //========================================================================

  /**
   * @brief Set TFTP server address
   */
  void set_server(const std::string &server) { this->server_ = server; }

  /**
   * @brief Set TFTP server port
   */
  void set_port(uint16_t port) { this->port_ = port; }

  /**
   * @brief Set mount path for storage_host integration
   */
  void set_mount_path(const std::string &path) { this->mount_path_ = path; }

  /**
   * @brief Get mount path
   */
  const std::string &get_mount_path() const { return this->mount_path_; }

  //========================================================================
  // Storage Host Integration (soft dependency)
  //========================================================================

  /**
   * @brief Register this TFTP client with storage_host
   *
   * Creates a virtual mount point for TFTP file access.
   * Only works if storage_host component is present (soft dependency).
   */
  void register_with_storage_host();

  //========================================================================
  // TFTP File Operations
  //========================================================================

  /**
   * @brief Read file from TFTP server
   *
   * @param filename Remote filename
   * @return TFTPResult with file data or error
   */
  TFTPResult read_file(const std::string &filename);

  /**
   * @brief Write file to TFTP server
   *
   * @param filename Remote filename
   * @param data File data to write
   * @param length Length of data
   * @return TFTPResult with success status
   */
  TFTPResult write_file(const std::string &filename, const uint8_t *data, size_t length);

  /**
   * @brief Check if server is reachable
   *
   * @return true if server responds to TFTP requests
   */
  bool is_server_reachable();

 protected:
  //========================================================================
  // Configuration
  //========================================================================

  std::string server_;
  uint16_t port_{TFTP_DEFAULT_PORT};
  std::string mount_path_;

  //========================================================================
  // Network State
  //========================================================================

#ifdef USE_ESP_IDF
  int socket_{-1};
#else
  std::unique_ptr<WiFiUDP> udp_;
#endif

  bool initialized_{false};

  //========================================================================
  // Internal TFTP Operations
  //========================================================================

  /**
   * @brief Initialize UDP socket
   */
  bool init_socket_();

  /**
   * @brief Close UDP socket
   */
  void close_socket_();

  /**
   * @brief Send TFTP packet
   */
  bool send_packet_(const TFTPPacket &packet, const std::string &server, uint16_t port);

  /**
   * @brief Receive TFTP packet with timeout
   */
  bool receive_packet_(TFTPPacket &packet, std::string &server, uint16_t &port, uint32_t timeout_ms = TFTP_TIMEOUT_MS);

  /**
   * @brief Perform TFTP read operation
   */
  TFTPResult read_file_internal_(const std::string &filename);

  /**
   * @brief Perform TFTP write operation
   */
  TFTPResult write_file_internal_(const std::string &filename, const uint8_t *data, size_t length);
};

}  // namespace tftp_client
}  // namespace esphome
