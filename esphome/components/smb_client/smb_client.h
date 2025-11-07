#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

// Forward declaration for storage_host::NetworkStorage (soft dependency)
#if defined(USE_STORAGE_HOST)
#include "esphome/components/storage_host/network_storage.h"
#endif

#ifdef USE_ESP_IDF
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#else
#include <WiFiClient.h>
#endif

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace esphome {
namespace smb_client {

static const char *const TAG = "smb_client";

//========================================================================
// SMB2 Protocol Constants (MS-SMB2)
//========================================================================

/// SMB2 default port
static constexpr uint16_t SMB_PORT = 445;

/// SMB2 protocol ID
static constexpr uint32_t SMB2_PROTOCOL_ID = 0x424D53FE;  // 0xFE 'S' 'M' 'B'

/// SMB2 header size
static constexpr size_t SMB2_HEADER_SIZE = 64;

/// SMB2 dialect versions
enum SMB2Dialect : uint16_t {
  SMB_DIALECT_2_0_2 = 0x0202,
  SMB_DIALECT_2_1 = 0x0210,
  SMB_DIALECT_3_0 = 0x0300,
  SMB_DIALECT_3_0_2 = 0x0302,
  SMB_DIALECT_3_1_1 = 0x0311,
};

/// SMB2 commands
enum SMB2Command : uint16_t {
  SMB2_NEGOTIATE = 0x0000,
  SMB2_SESSION_SETUP = 0x0001,
  SMB2_LOGOFF = 0x0002,
  SMB2_TREE_CONNECT = 0x0003,
  SMB2_TREE_DISCONNECT = 0x0004,
  SMB2_CREATE = 0x0005,
  SMB2_CLOSE = 0x0006,
  SMB2_FLUSH = 0x0007,
  SMB2_READ = 0x0008,
  SMB2_WRITE = 0x0009,
  SMB2_LOCK = 0x000A,
  SMB2_IOCTL = 0x000B,
  SMB2_CANCEL = 0x000C,
  SMB2_ECHO = 0x000D,
  SMB2_QUERY_DIRECTORY = 0x000E,
  SMB2_CHANGE_NOTIFY = 0x000F,
  SMB2_QUERY_INFO = 0x0010,
  SMB2_SET_INFO = 0x0011,
  SMB2_OPLOCK_BREAK = 0x0012,
};

/// SMB2 status codes
enum SMB2Status : uint32_t {
  STATUS_SUCCESS = 0x00000000,
  STATUS_MORE_PROCESSING_REQUIRED = 0xC0000016,
  STATUS_INVALID_PARAMETER = 0xC000000D,
  STATUS_LOGON_FAILURE = 0xC000006D,
  STATUS_ACCESS_DENIED = 0xC0000022,
  STATUS_OBJECT_NAME_NOT_FOUND = 0xC0000034,
  STATUS_OBJECT_NAME_COLLISION = 0xC0000035,
  STATUS_OBJECT_PATH_NOT_FOUND = 0xC000003A,
  STATUS_NO_MORE_FILES = 0x80000006,
  STATUS_END_OF_FILE = 0xC0000011,
};

/// SMB2 header flags
enum SMB2Flags : uint32_t {
  SMB2_FLAGS_SERVER_TO_REDIR = 0x00000001,
  SMB2_FLAGS_ASYNC_COMMAND = 0x00000002,
  SMB2_FLAGS_RELATED_OPERATIONS = 0x00000004,
  SMB2_FLAGS_SIGNED = 0x00000008,
  SMB2_FLAGS_DFS_OPERATIONS = 0x10000000,
  SMB2_FLAGS_REPLAY_OPERATION = 0x20000000,
};

/// SMB2 security modes
enum SMB2SecurityMode : uint16_t {
  SMB2_NEGOTIATE_SIGNING_ENABLED = 0x0001,
  SMB2_NEGOTIATE_SIGNING_REQUIRED = 0x0002,
};

/// SMB2 capabilities
enum SMB2Capabilities : uint32_t {
  SMB2_GLOBAL_CAP_DFS = 0x00000001,
  SMB2_GLOBAL_CAP_LEASING = 0x00000002,
  SMB2_GLOBAL_CAP_LARGE_MTU = 0x00000004,
  SMB2_GLOBAL_CAP_MULTI_CHANNEL = 0x00000008,
  SMB2_GLOBAL_CAP_PERSISTENT_HANDLES = 0x00000010,
  SMB2_GLOBAL_CAP_DIRECTORY_LEASING = 0x00000020,
  SMB2_GLOBAL_CAP_ENCRYPTION = 0x00000040,
};

/// File access masks
enum FileAccessMask : uint32_t {
  FILE_READ_DATA = 0x00000001,
  FILE_WRITE_DATA = 0x00000002,
  FILE_APPEND_DATA = 0x00000004,
  FILE_READ_EA = 0x00000008,
  FILE_WRITE_EA = 0x00000010,
  FILE_EXECUTE = 0x00000020,
  FILE_READ_ATTRIBUTES = 0x00000080,
  FILE_WRITE_ATTRIBUTES = 0x00000100,
  DELETE = 0x00010000,
  READ_CONTROL = 0x00020000,
  WRITE_DAC = 0x00040000,
  WRITE_OWNER = 0x00080000,
  SYNCHRONIZE = 0x00100000,
  ACCESS_SYSTEM_SECURITY = 0x01000000,
  MAXIMUM_ALLOWED = 0x02000000,
  GENERIC_ALL = 0x10000000,
  GENERIC_EXECUTE = 0x20000000,
  GENERIC_WRITE = 0x40000000,
  GENERIC_READ = 0x80000000,
};

/// File attributes
enum FileAttributes : uint32_t {
  FILE_ATTRIBUTE_READONLY = 0x00000001,
  FILE_ATTRIBUTE_HIDDEN = 0x00000002,
  FILE_ATTRIBUTE_SYSTEM = 0x00000004,
  FILE_ATTRIBUTE_DIRECTORY = 0x00000010,
  FILE_ATTRIBUTE_ARCHIVE = 0x00000020,
  FILE_ATTRIBUTE_NORMAL = 0x00000080,
  FILE_ATTRIBUTE_TEMPORARY = 0x00000100,
};

/// Create dispositions
enum CreateDisposition : uint32_t {
  FILE_SUPERSEDE = 0x00000000,
  FILE_OPEN = 0x00000001,
  FILE_CREATE = 0x00000002,
  FILE_OPEN_IF = 0x00000003,
  FILE_OVERWRITE = 0x00000004,
  FILE_OVERWRITE_IF = 0x00000005,
};

/// Create options
enum CreateOptions : uint32_t {
  FILE_DIRECTORY_FILE = 0x00000001,
  FILE_WRITE_THROUGH = 0x00000002,
  FILE_SEQUENTIAL_ONLY = 0x00000004,
  FILE_NO_INTERMEDIATE_BUFFERING = 0x00000008,
  FILE_SYNCHRONOUS_IO_ALERT = 0x00000010,
  FILE_SYNCHRONOUS_IO_NONALERT = 0x00000020,
  FILE_NON_DIRECTORY_FILE = 0x00000040,
  FILE_DELETE_ON_CLOSE = 0x00001000,
  FILE_RANDOM_ACCESS = 0x00000800,
};

/// File info classes
enum FileInfoClass : uint8_t {
  FileDirectoryInformation = 1,
  FileFullDirectoryInformation = 2,
  FileBothDirectoryInformation = 3,
  FileBasicInformation = 4,
  FileStandardInformation = 5,
  FileInternalInformation = 6,
  FileEaInformation = 7,
  FileAccessInformation = 8,
  FileNameInformation = 9,
  FileRenameInformation = 10,
  FileLinkInformation = 11,
  FileNamesInformation = 12,
  FileDispositionInformation = 13,
  FilePositionInformation = 14,
  FileFullEaInformation = 15,
  FileModeInformation = 16,
  FileAlignmentInformation = 17,
  FileAllInformation = 18,
  FileAllocationInformation = 19,
  FileEndOfFileInformation = 20,
  FileAlternateNameInformation = 21,
  FileStreamInformation = 22,
  FilePipeInformation = 23,
  FilePipeLocalInformation = 24,
  FilePipeRemoteInformation = 25,
  FileCompressionInformation = 28,
  FileNetworkOpenInformation = 34,
  FileIdBothDirectoryInformation = 37,
  FileIdFullDirectoryInformation = 38,
};

//========================================================================
// NTLM Authentication Constants
//========================================================================

/// NTLM signature
static constexpr uint64_t NTLM_SIGNATURE = 0x005353504D544C4E;  // "NTLMSSP\0"

/// NTLM message types
enum NTLMMessageType : uint32_t {
  NTLM_NEGOTIATE = 1,
  NTLM_CHALLENGE = 2,
  NTLM_AUTHENTICATE = 3,
};

/// NTLM negotiate flags
enum NTLMFlags : uint32_t {
  NTLMSSP_NEGOTIATE_UNICODE = 0x00000001,
  NTLMSSP_NEGOTIATE_OEM = 0x00000002,
  NTLMSSP_REQUEST_TARGET = 0x00000004,
  NTLMSSP_NEGOTIATE_SIGN = 0x00000010,
  NTLMSSP_NEGOTIATE_SEAL = 0x00000020,
  NTLMSSP_NEGOTIATE_DATAGRAM = 0x00000040,
  NTLMSSP_NEGOTIATE_LM_KEY = 0x00000080,
  NTLMSSP_NEGOTIATE_NTLM = 0x00000200,
  NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED = 0x00001000,
  NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED = 0x00002000,
  NTLMSSP_NEGOTIATE_ALWAYS_SIGN = 0x00008000,
  NTLMSSP_TARGET_TYPE_DOMAIN = 0x00010000,
  NTLMSSP_TARGET_TYPE_SERVER = 0x00020000,
  NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY = 0x00080000,
  NTLMSSP_NEGOTIATE_IDENTIFY = 0x00100000,
  NTLMSSP_REQUEST_NON_NT_SESSION_KEY = 0x00400000,
  NTLMSSP_NEGOTIATE_TARGET_INFO = 0x00800000,
  NTLMSSP_NEGOTIATE_VERSION = 0x02000000,
  NTLMSSP_NEGOTIATE_128 = 0x20000000,
  NTLMSSP_NEGOTIATE_KEY_EXCH = 0x40000000,
  NTLMSSP_NEGOTIATE_56 = 0x80000000,
};

//========================================================================
// SMB2 Structures
//========================================================================

/**
 * @brief SMB2 packet header
 */
struct SMB2Header {
  uint32_t protocol_id{SMB2_PROTOCOL_ID};
  uint16_t structure_size{64};
  uint16_t credit_charge{0};
  uint32_t status{0};
  uint16_t command{0};
  uint16_t credit_request{1};
  uint32_t flags{0};
  uint32_t next_command{0};
  uint64_t message_id{0};
  uint32_t process_id{0};
  uint32_t tree_id{0};
  uint64_t session_id{0};
  uint8_t signature[16]{0};

  void encode(std::vector<uint8_t> &buffer) const;
  bool decode(const uint8_t *data, size_t length);
};

/**
 * @brief SMB2 file ID (persistent + volatile)
 */
struct SMB2FileId {
  uint64_t persistent{0};
  uint64_t volatile_{0};

  SMB2FileId() = default;
  SMB2FileId(uint64_t p, uint64_t v) : persistent(p), volatile_(v) {}

  void encode(std::vector<uint8_t> &buffer) const;
  bool decode(const uint8_t *data);
};

/**
 * @brief SMB2 directory entry
 */
struct SMB2DirEntry {
  std::string filename;
  uint64_t file_size{0};
  uint32_t file_attributes{0};
  uint64_t creation_time{0};
  uint64_t last_access_time{0};
  uint64_t last_write_time{0};
  uint64_t change_time{0};
  uint64_t end_of_file{0};
  uint64_t allocation_size{0};

  bool is_directory() const { return (this->file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
};

//========================================================================
// SMB2 Client Component
//========================================================================

/**
 * @brief SMB2/CIFS Client for Windows file shares
 *
 * Implements SMB 2.0.2 protocol for accessing Windows shares and Samba.
 * Provides file operations over CIFS/SMB protocol with NTLM authentication.
 *
 * Features:
 * - SMB 2.0.2 protocol support
 * - NTLM v2 authentication
 * - File operations: read, write, create, delete
 * - Directory operations: list, create, delete
 * - Storage host integration
 * - Windows and Samba compatibility
 *
 * Example configuration:
 * @code
 * smb_client:
 *   - id: windows_share
 *     server: DESKTOP-PC
 *     share: Public
 *     username: user
 *     password: !secret smb_password
 *     mount_path: /smb/windows  # For storage_host
 *     domain: WORKGROUP  # Optional
 * @endcode
 */
class SMBClient : public Component
#if defined(USE_STORAGE_HOST)
                  ,
                  public storage_host::NetworkStorage
#endif
{
 public:
  SMBClient() = default;
  ~SMBClient() override;

  // Component lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  //========================================================================
  // Configuration
  //========================================================================

  void set_server(const std::string &server) { this->server_ = server; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_share(const std::string &share) { this->share_ = share; }
  void set_username(const std::string &username) { this->username_ = username; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_domain(const std::string &domain) { this->domain_ = domain; }
  void set_mount_path(const std::string &mount_path) { this->mount_path_ = mount_path; }

  const std::string &get_mount_path() const { return this->mount_path_; }

  //========================================================================
  // Storage Host Integration (soft dependency)
  //========================================================================

  void register_with_storage_host();

  //========================================================================
  // Connection Management
  //========================================================================

  bool is_connected() const { return this->connected_; }
  bool connect();
  void disconnect();

  //========================================================================
  // File Operations
  //========================================================================

  bool read_file(const std::string &path, std::vector<uint8_t> &data);
  bool write_file(const std::string &path, const uint8_t *data, size_t length);
  bool delete_file(const std::string &path);
  bool file_exists(const std::string &path);

  //========================================================================
  // Directory Operations
  //========================================================================

  bool list_directory(const std::string &path, std::vector<SMB2DirEntry> &entries);
  bool create_directory(const std::string &path);
  bool delete_directory(const std::string &path);

  //========================================================================
  // NetworkStorage Interface Implementation (when USE_STORAGE_HOST is defined)
  //========================================================================

#if defined(USE_STORAGE_HOST)
  // NetworkStorage interface overrides
  bool is_connected() const override { return this->connected_; }
  const std::string &get_mount_path() const override { return this->mount_path_; }
  const char *get_protocol() const override { return "smb"; }

  // File operations (already implemented above, just marked as override)
  bool read_file(const std::string &path, std::vector<uint8_t> &data) override;
  bool write_file(const std::string &path, const uint8_t *data, size_t length) override;
  bool delete_file(const std::string &path) override;
  bool file_exists(const std::string &path) override;

  // Directory operations with NetworkStorage::DirEntry conversion
  bool list_directory(const std::string &path, std::vector<storage_host::NetworkStorage::DirEntry> &entries) override;
  bool create_directory(const std::string &path) override;
  bool delete_directory(const std::string &path) override;
#endif

 protected:
  //========================================================================
  // Configuration
  //========================================================================

  std::string server_;
  uint16_t port_{SMB_PORT};
  std::string share_;
  std::string username_;
  std::string password_;
  std::string domain_{"WORKGROUP"};
  std::string mount_path_;

  //========================================================================
  // Connection State
  //========================================================================

#ifdef USE_ESP_IDF
  int socket_{-1};
#else
  std::unique_ptr<WiFiClient> client_;
#endif

  bool connected_{false};
  uint64_t session_id_{0};
  uint32_t tree_id_{0};
  uint64_t message_id_{1};

  //========================================================================
  // Protocol State
  //========================================================================

  SMB2Dialect negotiated_dialect_{SMB_DIALECT_2_0_2};
  uint16_t security_mode_{0};
  std::vector<uint8_t> server_guid_;

  //========================================================================
  // Internal Operations
  //========================================================================

  bool connect_socket_();
  void close_socket_();
  bool send_smb2_(const std::vector<uint8_t> &request);
  bool receive_smb2_(std::vector<uint8_t> &response);

  //========================================================================
  // SMB2 Protocol Operations
  //========================================================================

  bool smb2_negotiate_();
  bool smb2_session_setup_();
  bool smb2_tree_connect_();
  bool smb2_tree_disconnect_();
  bool smb2_logoff_();

  //========================================================================
  // File Operations (Internal)
  //========================================================================

  bool smb2_create_(const std::string &path, uint32_t desired_access, uint32_t file_attributes,
                    uint32_t create_disposition, uint32_t create_options, SMB2FileId &file_id);
  bool smb2_close_(const SMB2FileId &file_id);
  bool smb2_read_(const SMB2FileId &file_id, uint64_t offset, uint32_t length, std::vector<uint8_t> &data);
  bool smb2_write_(const SMB2FileId &file_id, uint64_t offset, const uint8_t *data, size_t length);
  bool smb2_query_directory_(const SMB2FileId &file_id, const std::string &pattern,
                             std::vector<SMB2DirEntry> &entries);

  //========================================================================
  // NTLM Authentication
  //========================================================================

  std::vector<uint8_t> create_ntlm_negotiate_();
  std::vector<uint8_t> create_ntlm_authenticate_(const std::vector<uint8_t> &challenge_message);
  std::vector<uint8_t> ntlm_hash_(const std::string &password);
  std::vector<uint8_t> ntlmv2_hash_(const std::string &username, const std::string &password,
                                    const std::string &domain);
  std::vector<uint8_t> compute_ntlmv2_response_(const std::vector<uint8_t> &ntlmv2_hash,
                                                const std::vector<uint8_t> &server_challenge,
                                                const std::vector<uint8_t> &client_challenge,
                                                uint64_t timestamp, const std::vector<uint8_t> &target_info);

  //========================================================================
  // Helper Functions
  //========================================================================

  uint64_t next_message_id_() { return this->message_id_++; }
  std::string to_utf16le_(const std::string &str);
  std::string from_utf16le_(const uint8_t *data, size_t length);
  void encode_uint16_(std::vector<uint8_t> &buffer, uint16_t value);
  void encode_uint32_(std::vector<uint8_t> &buffer, uint32_t value);
  void encode_uint64_(std::vector<uint8_t> &buffer, uint64_t value);
  uint16_t decode_uint16_(const uint8_t *data);
  uint32_t decode_uint32_(const uint8_t *data);
  uint64_t decode_uint64_(const uint8_t *data);
};

}  // namespace smb_client
}  // namespace esphome
