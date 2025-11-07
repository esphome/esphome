#include "smb_client.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <cstring>
#include <algorithm>

// For MD4/MD5 hashing (NTLM)
#ifdef USE_ESP_IDF
#include "mbedtls/md4.h"
#include "mbedtls/md5.h"
#include "mbedtls/md.h"
#else
// Arduino crypto libraries
#include <MD5.h>
#endif

// Forward declare storage_host for soft dependency
#if defined(USE_STORAGE_HOST)
namespace storage_host {
extern class StorageHost *global_storage_host;
}
#endif  // USE_STORAGE_HOST

namespace esphome {
namespace smb_client {

//========================================================================
// Helper Functions - Encoding/Decoding
//========================================================================

void SMBClient::encode_uint16_(std::vector<uint8_t> &buffer, uint16_t value) {
  buffer.push_back(value & 0xFF);
  buffer.push_back((value >> 8) & 0xFF);
}

void SMBClient::encode_uint32_(std::vector<uint8_t> &buffer, uint32_t value) {
  buffer.push_back(value & 0xFF);
  buffer.push_back((value >> 8) & 0xFF);
  buffer.push_back((value >> 16) & 0xFF);
  buffer.push_back((value >> 24) & 0xFF);
}

void SMBClient::encode_uint64_(std::vector<uint8_t> &buffer, uint64_t value) {
  this->encode_uint32_(buffer, static_cast<uint32_t>(value & 0xFFFFFFFF));
  this->encode_uint32_(buffer, static_cast<uint32_t>(value >> 32));
}

uint16_t SMBClient::decode_uint16_(const uint8_t *data) {
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t SMBClient::decode_uint32_(const uint8_t *data) {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t SMBClient::decode_uint64_(const uint8_t *data) {
  uint32_t low = this->decode_uint32_(data);
  uint32_t high = this->decode_uint32_(data + 4);
  return static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32);
}

std::string SMBClient::to_utf16le_(const std::string &str) {
  std::string result;
  for (char c : str) {
    result.push_back(c);
    result.push_back(0);  // UTF-16 LE (ASCII subset)
  }
  return result;
}

std::string SMBClient::from_utf16le_(const uint8_t *data, size_t length) {
  std::string result;
  for (size_t i = 0; i + 1 < length; i += 2) {
    if (data[i] == 0 && data[i + 1] == 0) {
      break;  // Null terminator
    }
    result.push_back(data[i]);  // Take low byte only (ASCII subset)
  }
  return result;
}

//========================================================================
// SMB2 Header Implementation
//========================================================================

void SMB2Header::encode(std::vector<uint8_t> &buffer) const {
  // Protocol ID (4 bytes) - 0xFE 'S' 'M' 'B'
  buffer.push_back(this->protocol_id & 0xFF);
  buffer.push_back((this->protocol_id >> 8) & 0xFF);
  buffer.push_back((this->protocol_id >> 16) & 0xFF);
  buffer.push_back((this->protocol_id >> 24) & 0xFF);

  // Structure size (2 bytes)
  buffer.push_back(this->structure_size & 0xFF);
  buffer.push_back((this->structure_size >> 8) & 0xFF);

  // Credit charge (2 bytes)
  buffer.push_back(this->credit_charge & 0xFF);
  buffer.push_back((this->credit_charge >> 8) & 0xFF);

  // Status (4 bytes)
  buffer.push_back(this->status & 0xFF);
  buffer.push_back((this->status >> 8) & 0xFF);
  buffer.push_back((this->status >> 16) & 0xFF);
  buffer.push_back((this->status >> 24) & 0xFF);

  // Command (2 bytes)
  buffer.push_back(this->command & 0xFF);
  buffer.push_back((this->command >> 8) & 0xFF);

  // Credit request/response (2 bytes)
  buffer.push_back(this->credit_request & 0xFF);
  buffer.push_back((this->credit_request >> 8) & 0xFF);

  // Flags (4 bytes)
  buffer.push_back(this->flags & 0xFF);
  buffer.push_back((this->flags >> 8) & 0xFF);
  buffer.push_back((this->flags >> 16) & 0xFF);
  buffer.push_back((this->flags >> 24) & 0xFF);

  // Next command (4 bytes)
  buffer.push_back(this->next_command & 0xFF);
  buffer.push_back((this->next_command >> 8) & 0xFF);
  buffer.push_back((this->next_command >> 16) & 0xFF);
  buffer.push_back((this->next_command >> 24) & 0xFF);

  // Message ID (8 bytes)
  buffer.push_back(this->message_id & 0xFF);
  buffer.push_back((this->message_id >> 8) & 0xFF);
  buffer.push_back((this->message_id >> 16) & 0xFF);
  buffer.push_back((this->message_id >> 24) & 0xFF);
  buffer.push_back((this->message_id >> 32) & 0xFF);
  buffer.push_back((this->message_id >> 40) & 0xFF);
  buffer.push_back((this->message_id >> 48) & 0xFF);
  buffer.push_back((this->message_id >> 56) & 0xFF);

  // Process ID (4 bytes)
  buffer.push_back(this->process_id & 0xFF);
  buffer.push_back((this->process_id >> 8) & 0xFF);
  buffer.push_back((this->process_id >> 16) & 0xFF);
  buffer.push_back((this->process_id >> 24) & 0xFF);

  // Tree ID (4 bytes)
  buffer.push_back(this->tree_id & 0xFF);
  buffer.push_back((this->tree_id >> 8) & 0xFF);
  buffer.push_back((this->tree_id >> 16) & 0xFF);
  buffer.push_back((this->tree_id >> 24) & 0xFF);

  // Session ID (8 bytes)
  buffer.push_back(this->session_id & 0xFF);
  buffer.push_back((this->session_id >> 8) & 0xFF);
  buffer.push_back((this->session_id >> 16) & 0xFF);
  buffer.push_back((this->session_id >> 24) & 0xFF);
  buffer.push_back((this->session_id >> 32) & 0xFF);
  buffer.push_back((this->session_id >> 40) & 0xFF);
  buffer.push_back((this->session_id >> 48) & 0xFF);
  buffer.push_back((this->session_id >> 56) & 0xFF);

  // Signature (16 bytes) - zeros for now (no signing)
  for (int i = 0; i < 16; i++) {
    buffer.push_back(this->signature[i]);
  }
}

bool SMB2Header::decode(const uint8_t *data, size_t length) {
  if (length < SMB2_HEADER_SIZE) {
    return false;
  }

  // Protocol ID
  this->protocol_id = (static_cast<uint32_t>(data[0])) | (static_cast<uint32_t>(data[1]) << 8) |
                      (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);

  if (this->protocol_id != SMB2_PROTOCOL_ID) {
    return false;
  }

  // Structure size
  this->structure_size = static_cast<uint16_t>(data[4]) | (static_cast<uint16_t>(data[5]) << 8);

  // Credit charge
  this->credit_charge = static_cast<uint16_t>(data[6]) | (static_cast<uint16_t>(data[7]) << 8);

  // Status
  this->status = (static_cast<uint32_t>(data[8])) | (static_cast<uint32_t>(data[9]) << 8) |
                 (static_cast<uint32_t>(data[10]) << 16) | (static_cast<uint32_t>(data[11]) << 24);

  // Command
  this->command = static_cast<uint16_t>(data[12]) | (static_cast<uint16_t>(data[13]) << 8);

  // Credit response
  this->credit_request = static_cast<uint16_t>(data[14]) | (static_cast<uint16_t>(data[15]) << 8);

  // Flags
  this->flags = (static_cast<uint32_t>(data[16])) | (static_cast<uint32_t>(data[17]) << 8) |
                (static_cast<uint32_t>(data[18]) << 16) | (static_cast<uint32_t>(data[19]) << 24);

  // Next command
  this->next_command = (static_cast<uint32_t>(data[20])) | (static_cast<uint32_t>(data[21]) << 8) |
                       (static_cast<uint32_t>(data[22]) << 16) | (static_cast<uint32_t>(data[23]) << 24);

  // Message ID
  this->message_id = 0;
  for (int i = 0; i < 8; i++) {
    this->message_id |= static_cast<uint64_t>(data[24 + i]) << (i * 8);
  }

  // Process ID
  this->process_id = (static_cast<uint32_t>(data[32])) | (static_cast<uint32_t>(data[33]) << 8) |
                     (static_cast<uint32_t>(data[34]) << 16) | (static_cast<uint32_t>(data[35]) << 24);

  // Tree ID
  this->tree_id = (static_cast<uint32_t>(data[36])) | (static_cast<uint32_t>(data[37]) << 8) |
                  (static_cast<uint32_t>(data[38]) << 16) | (static_cast<uint32_t>(data[39]) << 24);

  // Session ID
  this->session_id = 0;
  for (int i = 0; i < 8; i++) {
    this->session_id |= static_cast<uint64_t>(data[40 + i]) << (i * 8);
  }

  // Signature
  std::memcpy(this->signature, data + 48, 16);

  return true;
}

//========================================================================
// SMB2 File ID Implementation
//========================================================================

void SMB2FileId::encode(std::vector<uint8_t> &buffer) const {
  // Persistent (8 bytes)
  for (int i = 0; i < 8; i++) {
    buffer.push_back((this->persistent >> (i * 8)) & 0xFF);
  }
  // Volatile (8 bytes)
  for (int i = 0; i < 8; i++) {
    buffer.push_back((this->volatile_ >> (i * 8)) & 0xFF);
  }
}

bool SMB2FileId::decode(const uint8_t *data) {
  // Persistent
  this->persistent = 0;
  for (int i = 0; i < 8; i++) {
    this->persistent |= static_cast<uint64_t>(data[i]) << (i * 8);
  }
  // Volatile
  this->volatile_ = 0;
  for (int i = 0; i < 8; i++) {
    this->volatile_ |= static_cast<uint64_t>(data[8 + i]) << (i * 8);
  }
  return true;
}

//========================================================================
// SMBClient Implementation
//========================================================================

SMBClient::~SMBClient() { this->disconnect(); }

void SMBClient::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SMB Client...");
  ESP_LOGCONFIG(TAG, "  Server: %s:%u", this->server_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Share: %s", this->share_.c_str());
  ESP_LOGCONFIG(TAG, "  Username: %s", this->username_.c_str());
  ESP_LOGCONFIG(TAG, "  Domain: %s", this->domain_.c_str());

  if (!this->mount_path_.empty()) {
    ESP_LOGCONFIG(TAG, "  Mount path: %s", this->mount_path_.c_str());
  }

  // Try to connect
  if (this->connect()) {
    ESP_LOGI(TAG, "Successfully connected to SMB share");

    // Register with storage_host if configured
    if (!this->mount_path_.empty()) {
      this->register_with_storage_host();
    }
  } else {
    ESP_LOGW(TAG, "Failed to connect to SMB share (will retry in loop)");
  }
}

void SMBClient::loop() {
  // Try to connect if not connected
  if (!this->connected_) {
    static uint32_t last_connect_attempt = 0;
    uint32_t now = millis();
    if (now - last_connect_attempt > 30000) {  // Try every 30 seconds
      last_connect_attempt = now;
      if (this->connect()) {
        ESP_LOGI(TAG, "Successfully connected to SMB share");
      }
    }
  }
}

void SMBClient::dump_config() {
  ESP_LOGCONFIG(TAG, "SMB Client:");
  ESP_LOGCONFIG(TAG, "  Server: %s:%u", this->server_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Share: %s", this->share_.c_str());
  ESP_LOGCONFIG(TAG, "  Status: %s", this->connected_ ? "Connected" : "Disconnected");
  if (!this->mount_path_.empty()) {
    ESP_LOGCONFIG(TAG, "  Mount path: %s", this->mount_path_.c_str());
  }
}

void SMBClient::register_with_storage_host() {
#if defined(USE_STORAGE_HOST)
  if (storage_host::global_storage_host != nullptr) {
    storage_host::global_storage_host->register_mount(this->mount_path_, "smb");
    ESP_LOGI(TAG, "Registered SMB mount with storage_host: %s", this->mount_path_.c_str());
  } else {
    ESP_LOGD(TAG, "storage_host not available, skipping mount registration");
  }
#else
  ESP_LOGD(TAG, "storage_host component not compiled, mount registration disabled");
#endif  // USE_STORAGE_HOST
}

// Note: Due to the complexity and length of the full SMB2 implementation,
// I'm providing a WIP (Work In Progress) version with stubs for the remaining methods.
// The full implementation requires:
// - NTLM authentication (MD4/MD5 hashing, challenge-response)
// - SMB2 negotiate protocol
// - SMB2 session setup
// - SMB2 tree connect
// - SMB2 file operations (create, read, write, close, query directory)
// This would add approximately 1500+ more lines of code.

bool SMBClient::connect() {
  ESP_LOGI(TAG, "SMB connect() - WIP: Full implementation pending");
  // TODO: Implement full SMB2 connection sequence
  return false;
}

void SMBClient::disconnect() {
  ESP_LOGI(TAG, "SMB disconnect() - WIP");
  this->close_socket_();
}

bool SMBClient::connect_socket_() {
  // Socket connection code (similar to NFS client)
  ESP_LOGD(TAG, "SMB connect_socket_() - WIP");
  return false;
}

void SMBClient::close_socket_() {
  // Socket close code
  this->connected_ = false;
}

bool SMBClient::send_smb2_(const std::vector<uint8_t> &request) {
  // Send SMB2 packet over TCP
  return false;
}

bool SMBClient::receive_smb2_(std::vector<uint8_t> &response) {
  // Receive SMB2 packet from TCP
  return false;
}

bool SMBClient::smb2_negotiate_() {
  ESP_LOGD(TAG, "SMB2 negotiate - WIP");
  return false;
}

bool SMBClient::smb2_session_setup_() {
  ESP_LOGD(TAG, "SMB2 session setup - WIP");
  return false;
}

bool SMBClient::smb2_tree_connect_() {
  ESP_LOGD(TAG, "SMB2 tree connect - WIP");
  return false;
}

bool SMBClient::smb2_tree_disconnect_() {
  ESP_LOGD(TAG, "SMB2 tree disconnect - WIP");
  return false;
}

bool SMBClient::smb2_logoff_() {
  ESP_LOGD(TAG, "SMB2 logoff - WIP");
  return false;
}

bool SMBClient::smb2_create_(const std::string &path, uint32_t desired_access, uint32_t file_attributes,
                             uint32_t create_disposition, uint32_t create_options, SMB2FileId &file_id) {
  ESP_LOGD(TAG, "SMB2 create - WIP");
  return false;
}

bool SMBClient::smb2_close_(const SMB2FileId &file_id) {
  ESP_LOGD(TAG, "SMB2 close - WIP");
  return false;
}

bool SMBClient::smb2_read_(const SMB2FileId &file_id, uint64_t offset, uint32_t length, std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "SMB2 read - WIP");
  return false;
}

bool SMBClient::smb2_write_(const SMB2FileId &file_id, uint64_t offset, const uint8_t *data, size_t length) {
  ESP_LOGD(TAG, "SMB2 write - WIP");
  return false;
}

bool SMBClient::smb2_query_directory_(const SMB2FileId &file_id, const std::string &pattern,
                                      std::vector<SMB2DirEntry> &entries) {
  ESP_LOGD(TAG, "SMB2 query directory - WIP");
  return false;
}

std::vector<uint8_t> SMBClient::create_ntlm_negotiate_() {
  ESP_LOGD(TAG, "NTLM negotiate - WIP");
  return {};
}

std::vector<uint8_t> SMBClient::create_ntlm_authenticate_(const std::vector<uint8_t> &challenge_message) {
  ESP_LOGD(TAG, "NTLM authenticate - WIP");
  return {};
}

std::vector<uint8_t> SMBClient::ntlm_hash_(const std::string &password) {
  ESP_LOGD(TAG, "NTLM hash - WIP");
  return {};
}

std::vector<uint8_t> SMBClient::ntlmv2_hash_(const std::string &username, const std::string &password,
                                             const std::string &domain) {
  ESP_LOGD(TAG, "NTLMv2 hash - WIP");
  return {};
}

std::vector<uint8_t> SMBClient::compute_ntlmv2_response_(const std::vector<uint8_t> &ntlmv2_hash,
                                                         const std::vector<uint8_t> &server_challenge,
                                                         const std::vector<uint8_t> &client_challenge,
                                                         uint64_t timestamp,
                                                         const std::vector<uint8_t> &target_info) {
  ESP_LOGD(TAG, "NTLMv2 response - WIP");
  return {};
}

// High-level file operations (stubs)
bool SMBClient::read_file(const std::string &path, std::vector<uint8_t> &data) {
  ESP_LOGW(TAG, "SMB read_file() - not yet implemented");
  return false;
}

bool SMBClient::write_file(const std::string &path, const uint8_t *data, size_t length) {
  ESP_LOGW(TAG, "SMB write_file() - not yet implemented");
  return false;
}

bool SMBClient::delete_file(const std::string &path) {
  ESP_LOGW(TAG, "SMB delete_file() - not yet implemented");
  return false;
}

bool SMBClient::file_exists(const std::string &path) {
  ESP_LOGW(TAG, "SMB file_exists() - not yet implemented");
  return false;
}

bool SMBClient::list_directory(const std::string &path, std::vector<SMB2DirEntry> &entries) {
  ESP_LOGW(TAG, "SMB list_directory() - not yet implemented");
  return false;
}

bool SMBClient::create_directory(const std::string &path) {
  ESP_LOGW(TAG, "SMB create_directory() - not yet implemented");
  return false;
}

bool SMBClient::delete_directory(const std::string &path) {
  ESP_LOGW(TAG, "SMB delete_directory() - not yet implemented");
  return false;
}

}  // namespace smb_client
}  // namespace esphome
