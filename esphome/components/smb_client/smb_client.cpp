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
    storage_host::global_storage_host->register_network_storage(this);
    ESP_LOGI(TAG, "Registered SMB network storage with storage_host: %s", this->mount_path_.c_str());
  } else {
    ESP_LOGD(TAG, "storage_host not available, skipping network storage registration");
  }
#else
  ESP_LOGD(TAG, "storage_host component not compiled, network storage registration disabled");
#endif  // USE_STORAGE_HOST
}

//========================================================================
// SMB2 Connection and Protocol Implementation
//========================================================================

bool SMBClient::connect() {
  ESP_LOGI(TAG, "Connecting to SMB server %s:%u", this->server_.c_str(), this->port_);

  // 1. Establish TCP connection
  if (!this->connect_socket_()) {
    ESP_LOGW(TAG, "Failed to connect to SMB server");
    return false;
  }

  // 2. SMB2 Negotiate
  if (!this->smb2_negotiate_()) {
    ESP_LOGW(TAG, "SMB2 negotiate failed");
    this->close_socket_();
    return false;
  }

  // 3. SMB2 Session Setup (NTLM authentication)
  if (!this->smb2_session_setup_()) {
    ESP_LOGW(TAG, "SMB2 session setup failed");
    this->close_socket_();
    return false;
  }

  // 4. SMB2 Tree Connect
  if (!this->smb2_tree_connect_()) {
    ESP_LOGW(TAG, "SMB2 tree connect failed");
    this->close_socket_();
    return false;
  }

  this->connected_ = true;
  ESP_LOGI(TAG, "Successfully connected to SMB share");
  return true;
}

void SMBClient::disconnect() {
  if (!this->connected_) {
    return;
  }

  ESP_LOGI(TAG, "Disconnecting from SMB server");

  // Tree disconnect
  this->smb2_tree_disconnect_();

  // Logoff
  this->smb2_logoff_();

  // Close socket
  this->close_socket_();

  this->connected_ = false;
  this->session_id_ = 0;
  this->tree_id_ = 0;
}

bool SMBClient::connect_socket_() {
#ifdef USE_ESP_IDF
  // ESP-IDF implementation
  struct addrinfo hints{}, *result = nullptr;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str = std::to_string(this->port_);
  int err = getaddrinfo(this->server_.c_str(), port_str.c_str(), &hints, &result);
  if (err != 0 || result == nullptr) {
    ESP_LOGW(TAG, "DNS lookup failed for %s: %d", this->server_.c_str(), err);
    if (result != nullptr) {
      freeaddrinfo(result);
    }
    return false;
  }

  this->socket_ = socket(result->ai_family, result->ai_socktype, 0);
  if (this->socket_ < 0) {
    ESP_LOGW(TAG, "Failed to create socket: %d", errno);
    freeaddrinfo(result);
    return false;
  }

  // Set socket timeout
  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;
  setsockopt(this->socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(this->socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  if (::connect(this->socket_, result->ai_addr, result->ai_addrlen) < 0) {
    ESP_LOGW(TAG, "Failed to connect to %s:%u: %d", this->server_.c_str(), this->port_, errno);
    close(this->socket_);
    this->socket_ = -1;
    freeaddrinfo(result);
    return false;
  }

  freeaddrinfo(result);
  ESP_LOGD(TAG, "TCP connection established");
  return true;

#else
  // Arduino WiFiClient implementation
  this->client_ = std::make_unique<WiFiClient>();
  if (!this->client_->connect(this->server_.c_str(), this->port_)) {
    ESP_LOGW(TAG, "Failed to connect to %s:%u", this->server_.c_str(), this->port_);
    this->client_ = nullptr;
    return false;
  }

  this->client_->setTimeout(10000);  // 10 second timeout
  ESP_LOGD(TAG, "TCP connection established");
  return true;
#endif
}

void SMBClient::close_socket_() {
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
  this->connected_ = false;
}

bool SMBClient::send_smb2_(const std::vector<uint8_t> &request) {
  // SMB2 uses NetBIOS session service header (4 bytes) before the SMB2 packet
  // Byte 0: Message type (0x00 = session message)
  // Bytes 1-3: Length (big-endian, 24-bit)
  std::vector<uint8_t> packet;
  packet.push_back(0x00);  // Session message
  uint32_t length = request.size();
  packet.push_back((length >> 16) & 0xFF);  // Big-endian length
  packet.push_back((length >> 8) & 0xFF);
  packet.push_back(length & 0xFF);

  // Append SMB2 packet
  packet.insert(packet.end(), request.begin(), request.end());

#ifdef USE_ESP_IDF
  size_t sent = 0;
  while (sent < packet.size()) {
    int result = send(this->socket_, packet.data() + sent, packet.size() - sent, 0);
    if (result < 0) {
      ESP_LOGW(TAG, "Failed to send SMB2 packet: %d", errno);
      return false;
    }
    sent += result;
  }
  return true;

#else
  size_t sent = this->client_->write(packet.data(), packet.size());
  if (sent != packet.size()) {
    ESP_LOGW(TAG, "Failed to send complete SMB2 packet");
    return false;
  }
  return true;
#endif
}

bool SMBClient::receive_smb2_(std::vector<uint8_t> &response) {
  response.clear();

  // Read NetBIOS session header (4 bytes)
  uint8_t header[4];

#ifdef USE_ESP_IDF
  int received = recv(this->socket_, header, 4, MSG_WAITALL);
  if (received != 4) {
    ESP_LOGW(TAG, "Failed to receive NetBIOS header: %d", errno);
    return false;
  }
#else
  if (this->client_->readBytes(header, 4) != 4) {
    ESP_LOGW(TAG, "Failed to receive NetBIOS header");
    return false;
  }
#endif

  // Parse length (big-endian, 24-bit)
  uint32_t length = (static_cast<uint32_t>(header[1]) << 16) | (static_cast<uint32_t>(header[2]) << 8) |
                    static_cast<uint32_t>(header[3]);

  if (length == 0 || length > 1024 * 1024) {  // Sanity check: max 1MB
    ESP_LOGW(TAG, "Invalid SMB2 packet length: %u", length);
    return false;
  }

  // Read SMB2 packet
  response.resize(length);

#ifdef USE_ESP_IDF
  size_t total_received = 0;
  while (total_received < length) {
    int result = recv(this->socket_, response.data() + total_received, length - total_received, 0);
    if (result <= 0) {
      ESP_LOGW(TAG, "Failed to receive SMB2 packet: %d", errno);
      return false;
    }
    total_received += result;
  }
#else
  if (this->client_->readBytes(response.data(), length) != length) {
    ESP_LOGW(TAG, "Failed to receive complete SMB2 packet");
    return false;
  }
#endif

  return true;
}

bool SMBClient::smb2_negotiate_() {
  ESP_LOGD(TAG, "SMB2 Negotiate");

  // Build negotiate request
  std::vector<uint8_t> request;

  // SMB2 header
  SMB2Header header;
  header.command = SMB2_NEGOTIATE;
  header.message_id = this->next_message_id_();
  header.encode(request);

  // Negotiate request body
  // Structure size (2 bytes): 36
  this->encode_uint16_(request, 36);

  // Dialect count (2 bytes): 1
  this->encode_uint16_(request, 1);

  // Security mode (2 bytes)
  this->encode_uint16_(request, SMB2_NEGOTIATE_SIGNING_ENABLED);

  // Reserved (2 bytes)
  this->encode_uint16_(request, 0);

  // Capabilities (4 bytes)
  this->encode_uint32_(request, 0);

  // Client GUID (16 bytes) - zeros
  for (int i = 0; i < 16; i++) {
    request.push_back(0);
  }

  // Negotiate context offset (4 bytes) - 0 for SMB 2.0.2
  this->encode_uint32_(request, 0);

  // Negotiate context count (2 bytes)
  this->encode_uint16_(request, 0);

  // Reserved (2 bytes)
  this->encode_uint16_(request, 0);

  // Dialects (2 bytes each) - we support SMB 2.0.2
  this->encode_uint16_(request, SMB_DIALECT_2_0_2);

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send negotiate request");
    return false;
  }

  // Receive response
  std::vector<uint8_t> response;
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive negotiate response");
    return false;
  }

  // Parse response header
  SMB2Header response_header;
  if (!response_header.decode(response.data(), response.size())) {
    ESP_LOGW(TAG, "Failed to decode negotiate response header");
    return false;
  }

  if (response_header.status != STATUS_SUCCESS) {
    ESP_LOGW(TAG, "Negotiate failed with status 0x%08X", response_header.status);
    return false;
  }

  // Parse negotiate response body
  if (response.size() < SMB2_HEADER_SIZE + 64) {
    ESP_LOGW(TAG, "Negotiate response too short");
    return false;
  }

  const uint8_t *body = response.data() + SMB2_HEADER_SIZE;

  // Security mode (offset 2)
  this->security_mode_ = this->decode_uint16_(body + 2);

  // Dialect revision (offset 4)
  this->negotiated_dialect_ = static_cast<SMB2Dialect>(this->decode_uint16_(body + 4));

  // Server GUID (offset 8, 16 bytes)
  this->server_guid_.assign(body + 8, body + 24);

  ESP_LOGI(TAG, "SMB2 Negotiate successful (dialect: 0x%04X)", this->negotiated_dialect_);
  return true;
}

bool SMBClient::smb2_session_setup_() {
  ESP_LOGD(TAG, "SMB2 Session Setup");

  // Step 1: Send negotiate message
  std::vector<uint8_t> ntlm_negotiate = this->create_ntlm_negotiate_();

  std::vector<uint8_t> request;
  SMB2Header header;
  header.command = SMB2_SESSION_SETUP;
  header.message_id = this->next_message_id_();
  header.encode(request);

  // Session setup request body
  // Structure size (2 bytes): 25
  this->encode_uint16_(request, 25);

  // Flags (1 byte): 0
  request.push_back(0);

  // Security mode (1 byte)
  request.push_back(SMB2_NEGOTIATE_SIGNING_ENABLED);

  // Capabilities (4 bytes)
  this->encode_uint32_(request, 0);

  // Channel (4 bytes)
  this->encode_uint32_(request, 0);

  // Security buffer offset (2 bytes)
  uint16_t security_offset = SMB2_HEADER_SIZE + 24;
  this->encode_uint16_(request, security_offset);

  // Security buffer length (2 bytes)
  this->encode_uint16_(request, ntlm_negotiate.size());

  // Previous session ID (8 bytes)
  this->encode_uint64_(request, 0);

  // Security buffer (NTLM negotiate)
  request.insert(request.end(), ntlm_negotiate.begin(), ntlm_negotiate.end());

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send session setup request");
    return false;
  }

  // Receive response (should be MORE_PROCESSING_REQUIRED)
  std::vector<uint8_t> response;
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive session setup response");
    return false;
  }

  // Parse response header
  SMB2Header response_header;
  if (!response_header.decode(response.data(), response.size())) {
    ESP_LOGW(TAG, "Failed to decode session setup response header");
    return false;
  }

  if (response_header.status != STATUS_MORE_PROCESSING_REQUIRED) {
    ESP_LOGW(TAG, "Session setup step 1 failed with status 0x%08X", response_header.status);
    return false;
  }

  // Save session ID
  this->session_id_ = response_header.session_id;

  // Parse NTLM challenge
  const uint8_t *body = response.data() + SMB2_HEADER_SIZE;
  uint16_t security_buffer_offset = this->decode_uint16_(body + 8);
  uint16_t security_buffer_length = this->decode_uint16_(body + 6);

  if (security_buffer_offset + security_buffer_length > response.size()) {
    ESP_LOGW(TAG, "Invalid security buffer in session setup response");
    return false;
  }

  std::vector<uint8_t> ntlm_challenge(response.begin() + security_buffer_offset,
                                      response.begin() + security_buffer_offset + security_buffer_length);

  // Step 2: Send authenticate message
  std::vector<uint8_t> ntlm_authenticate = this->create_ntlm_authenticate_(ntlm_challenge);
  if (ntlm_authenticate.empty()) {
    ESP_LOGW(TAG, "Failed to create NTLM authenticate message");
    return false;
  }

  request.clear();
  header.message_id = this->next_message_id_();
  header.session_id = this->session_id_;
  header.encode(request);

  // Session setup request body
  this->encode_uint16_(request, 25);
  request.push_back(0);  // Flags
  request.push_back(SMB2_NEGOTIATE_SIGNING_ENABLED);  // Security mode
  this->encode_uint32_(request, 0);  // Capabilities
  this->encode_uint32_(request, 0);  // Channel
  this->encode_uint16_(request, SMB2_HEADER_SIZE + 24);  // Security buffer offset
  this->encode_uint16_(request, ntlm_authenticate.size());  // Security buffer length
  this->encode_uint64_(request, 0);  // Previous session ID

  // Security buffer (NTLM authenticate)
  request.insert(request.end(), ntlm_authenticate.begin(), ntlm_authenticate.end());

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send session setup authenticate");
    return false;
  }

  // Receive response
  response.clear();
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive session setup authenticate response");
    return false;
  }

  // Parse response header
  if (!response_header.decode(response.data(), response.size())) {
    ESP_LOGW(TAG, "Failed to decode session setup authenticate response header");
    return false;
  }

  if (response_header.status != STATUS_SUCCESS) {
    ESP_LOGW(TAG, "Session setup authenticate failed with status 0x%08X", response_header.status);
    return false;
  }

  ESP_LOGI(TAG, "SMB2 Session Setup successful (session ID: 0x%016llX)", this->session_id_);
  return true;
}

bool SMBClient::smb2_tree_connect_() {
  ESP_LOGD(TAG, "SMB2 Tree Connect to share: %s", this->share_.c_str());

  // Build tree connect request
  std::vector<uint8_t> request;

  // SMB2 header
  SMB2Header header;
  header.command = SMB2_TREE_CONNECT;
  header.message_id = this->next_message_id_();
  header.session_id = this->session_id_;
  header.encode(request);

  // Build UNC path: \\server\share
  std::string unc_path = "\\\\" + this->server_ + "\\" + this->share_;
  std::string unc_path_utf16 = this->to_utf16le_(unc_path);

  // Tree connect request body
  // Structure size (2 bytes): 9
  this->encode_uint16_(request, 9);

  // Reserved/flags (2 bytes)
  this->encode_uint16_(request, 0);

  // Path offset (2 bytes)
  uint16_t path_offset = SMB2_HEADER_SIZE + 8;
  this->encode_uint16_(request, path_offset);

  // Path length (2 bytes)
  this->encode_uint16_(request, unc_path_utf16.size());

  // Path (UTF-16 LE)
  request.insert(request.end(), unc_path_utf16.begin(), unc_path_utf16.end());

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send tree connect request");
    return false;
  }

  // Receive response
  std::vector<uint8_t> response;
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive tree connect response");
    return false;
  }

  // Parse response header
  SMB2Header response_header;
  if (!response_header.decode(response.data(), response.size())) {
    ESP_LOGW(TAG, "Failed to decode tree connect response header");
    return false;
  }

  if (response_header.status != STATUS_SUCCESS) {
    ESP_LOGW(TAG, "Tree connect failed with status 0x%08X", response_header.status);
    return false;
  }

  // Save tree ID
  this->tree_id_ = response_header.tree_id;

  ESP_LOGI(TAG, "SMB2 Tree Connect successful (tree ID: 0x%08X)", this->tree_id_);
  return true;
}

bool SMBClient::smb2_tree_disconnect_() {
  if (this->tree_id_ == 0) {
    return true;
  }

  ESP_LOGD(TAG, "SMB2 Tree Disconnect");

  // Build tree disconnect request
  std::vector<uint8_t> request;

  // SMB2 header
  SMB2Header header;
  header.command = SMB2_TREE_DISCONNECT;
  header.message_id = this->next_message_id_();
  header.session_id = this->session_id_;
  header.tree_id = this->tree_id_;
  header.encode(request);

  // Tree disconnect request body
  // Structure size (2 bytes): 4
  this->encode_uint16_(request, 4);

  // Reserved (2 bytes)
  this->encode_uint16_(request, 0);

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send tree disconnect request");
    return false;
  }

  // Receive response
  std::vector<uint8_t> response;
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive tree disconnect response");
    return false;
  }

  this->tree_id_ = 0;
  ESP_LOGD(TAG, "SMB2 Tree Disconnect successful");
  return true;
}

bool SMBClient::smb2_logoff_() {
  if (this->session_id_ == 0) {
    return true;
  }

  ESP_LOGD(TAG, "SMB2 Logoff");

  // Build logoff request
  std::vector<uint8_t> request;

  // SMB2 header
  SMB2Header header;
  header.command = SMB2_LOGOFF;
  header.message_id = this->next_message_id_();
  header.session_id = this->session_id_;
  header.encode(request);

  // Logoff request body
  // Structure size (2 bytes): 4
  this->encode_uint16_(request, 4);

  // Reserved (2 bytes)
  this->encode_uint16_(request, 0);

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send logoff request");
    return false;
  }

  // Receive response
  std::vector<uint8_t> response;
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive logoff response");
    return false;
  }

  this->session_id_ = 0;
  ESP_LOGD(TAG, "SMB2 Logoff successful");
  return true;
}

bool SMBClient::smb2_create_(const std::string &path, uint32_t desired_access, uint32_t file_attributes,
                             uint32_t create_disposition, uint32_t create_options, SMB2FileId &file_id) {
  ESP_LOGD(TAG, "SMB2 Create: %s", path.c_str());

  // Build create request
  std::vector<uint8_t> request;

  // SMB2 header
  SMB2Header header;
  header.command = SMB2_CREATE;
  header.message_id = this->next_message_id_();
  header.session_id = this->session_id_;
  header.tree_id = this->tree_id_;
  header.encode(request);

  // Convert path to UTF-16 LE
  std::string path_utf16 = this->to_utf16le_(path);

  // Create request body
  // Structure size (2 bytes): 57
  this->encode_uint16_(request, 57);

  // Security flags (1 byte)
  request.push_back(0);

  // Requested oplock level (1 byte)
  request.push_back(0);  // No oplock

  // Impersonation level (4 bytes)
  this->encode_uint32_(request, 0x02000000);  // Impersonation

  // Create flags (8 bytes)
  this->encode_uint64_(request, 0);

  // Reserved (8 bytes)
  this->encode_uint64_(request, 0);

  // Desired access (4 bytes)
  this->encode_uint32_(request, desired_access);

  // File attributes (4 bytes)
  this->encode_uint32_(request, file_attributes);

  // Share access (4 bytes) - allow read/write
  this->encode_uint32_(request, 0x00000007);  // FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE

  // Create disposition (4 bytes)
  this->encode_uint32_(request, create_disposition);

  // Create options (4 bytes)
  this->encode_uint32_(request, create_options);

  // Name offset (2 bytes)
  uint16_t name_offset = SMB2_HEADER_SIZE + 56;
  this->encode_uint16_(request, name_offset);

  // Name length (2 bytes)
  this->encode_uint16_(request, path_utf16.size());

  // Create contexts offset (4 bytes) - 0 (no contexts)
  this->encode_uint32_(request, 0);

  // Create contexts length (4 bytes)
  this->encode_uint32_(request, 0);

  // Filename (UTF-16 LE)
  request.insert(request.end(), path_utf16.begin(), path_utf16.end());

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send create request");
    return false;
  }

  // Receive response
  std::vector<uint8_t> response;
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive create response");
    return false;
  }

  // Parse response header
  SMB2Header response_header;
  if (!response_header.decode(response.data(), response.size())) {
    ESP_LOGW(TAG, "Failed to decode create response header");
    return false;
  }

  if (response_header.status != STATUS_SUCCESS) {
    ESP_LOGW(TAG, "Create failed with status 0x%08X", response_header.status);
    return false;
  }

  // Parse create response body
  if (response.size() < SMB2_HEADER_SIZE + 88) {
    ESP_LOGW(TAG, "Create response too short");
    return false;
  }

  const uint8_t *body = response.data() + SMB2_HEADER_SIZE;

  // File ID is at offset 64 (16 bytes)
  if (!file_id.decode(body + 64)) {
    ESP_LOGW(TAG, "Failed to decode file ID");
    return false;
  }

  ESP_LOGD(TAG, "SMB2 Create successful (file ID: %016llX:%016llX)", file_id.persistent, file_id.volatile_);
  return true;
}

bool SMBClient::smb2_close_(const SMB2FileId &file_id) {
  ESP_LOGD(TAG, "SMB2 Close");

  // Build close request
  std::vector<uint8_t> request;

  // SMB2 header
  SMB2Header header;
  header.command = SMB2_CLOSE;
  header.message_id = this->next_message_id_();
  header.session_id = this->session_id_;
  header.tree_id = this->tree_id_;
  header.encode(request);

  // Close request body
  // Structure size (2 bytes): 24
  this->encode_uint16_(request, 24);

  // Flags (2 bytes)
  this->encode_uint16_(request, 0);

  // Reserved (4 bytes)
  this->encode_uint32_(request, 0);

  // File ID (16 bytes)
  file_id.encode(request);

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send close request");
    return false;
  }

  // Receive response
  std::vector<uint8_t> response;
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive close response");
    return false;
  }

  // Parse response header
  SMB2Header response_header;
  if (!response_header.decode(response.data(), response.size())) {
    ESP_LOGW(TAG, "Failed to decode close response header");
    return false;
  }

  if (response_header.status != STATUS_SUCCESS) {
    ESP_LOGW(TAG, "Close failed with status 0x%08X", response_header.status);
    return false;
  }

  ESP_LOGD(TAG, "SMB2 Close successful");
  return true;
}

bool SMBClient::smb2_read_(const SMB2FileId &file_id, uint64_t offset, uint32_t length, std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "SMB2 Read: offset=%llu, length=%u", offset, length);

  // Build read request
  std::vector<uint8_t> request;

  // SMB2 header
  SMB2Header header;
  header.command = SMB2_READ;
  header.message_id = this->next_message_id_();
  header.session_id = this->session_id_;
  header.tree_id = this->tree_id_;
  header.encode(request);

  // Read request body
  // Structure size (2 bytes): 49
  this->encode_uint16_(request, 49);

  // Padding (1 byte)
  request.push_back(0);

  // Flags (1 byte)
  request.push_back(0);

  // Length (4 bytes)
  this->encode_uint32_(request, length);

  // Offset (8 bytes)
  this->encode_uint64_(request, offset);

  // File ID (16 bytes)
  file_id.encode(request);

  // Minimum count (4 bytes)
  this->encode_uint32_(request, 0);

  // Channel (4 bytes)
  this->encode_uint32_(request, 0);

  // Remaining bytes (4 bytes)
  this->encode_uint32_(request, 0);

  // Read channel info offset (2 bytes)
  this->encode_uint16_(request, 0);

  // Read channel info length (2 bytes)
  this->encode_uint16_(request, 0);

  // Buffer (1 byte)
  request.push_back(0);

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send read request");
    return false;
  }

  // Receive response
  std::vector<uint8_t> response;
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive read response");
    return false;
  }

  // Parse response header
  SMB2Header response_header;
  if (!response_header.decode(response.data(), response.size())) {
    ESP_LOGW(TAG, "Failed to decode read response header");
    return false;
  }

  if (response_header.status != STATUS_SUCCESS) {
    if (response_header.status == STATUS_END_OF_FILE) {
      // End of file - return empty data
      data.clear();
      return true;
    }
    ESP_LOGW(TAG, "Read failed with status 0x%08X", response_header.status);
    return false;
  }

  // Parse read response body
  if (response.size() < SMB2_HEADER_SIZE + 16) {
    ESP_LOGW(TAG, "Read response too short");
    return false;
  }

  const uint8_t *body = response.data() + SMB2_HEADER_SIZE;

  // Data offset (offset 2, 2 bytes)
  uint16_t data_offset = this->decode_uint16_(body + 2);

  // Data length (offset 4, 4 bytes)
  uint32_t data_length = this->decode_uint32_(body + 4);

  // Extract data
  if (data_offset + data_length > response.size()) {
    ESP_LOGW(TAG, "Invalid data offset/length in read response");
    return false;
  }

  data.assign(response.begin() + data_offset, response.begin() + data_offset + data_length);

  ESP_LOGD(TAG, "SMB2 Read successful: read %u bytes", data_length);
  return true;
}

bool SMBClient::smb2_write_(const SMB2FileId &file_id, uint64_t offset, const uint8_t *data, size_t length) {
  ESP_LOGD(TAG, "SMB2 Write: offset=%llu, length=%u", offset, static_cast<uint32_t>(length));

  // Build write request
  std::vector<uint8_t> request;

  // SMB2 header
  SMB2Header header;
  header.command = SMB2_WRITE;
  header.message_id = this->next_message_id_();
  header.session_id = this->session_id_;
  header.tree_id = this->tree_id_;
  header.encode(request);

  // Write request body
  // Structure size (2 bytes): 49
  this->encode_uint16_(request, 49);

  // Data offset (2 bytes) - relative to start of SMB2 header
  uint16_t data_offset = SMB2_HEADER_SIZE + 48;
  this->encode_uint16_(request, data_offset);

  // Length (4 bytes)
  this->encode_uint32_(request, length);

  // Offset (8 bytes)
  this->encode_uint64_(request, offset);

  // File ID (16 bytes)
  file_id.encode(request);

  // Channel (4 bytes)
  this->encode_uint32_(request, 0);

  // Remaining bytes (4 bytes)
  this->encode_uint32_(request, 0);

  // Write channel info offset (2 bytes)
  this->encode_uint16_(request, 0);

  // Write channel info length (2 bytes)
  this->encode_uint16_(request, 0);

  // Flags (4 bytes)
  this->encode_uint32_(request, 0);

  // Data
  request.insert(request.end(), data, data + length);

  // Send request
  if (!this->send_smb2_(request)) {
    ESP_LOGW(TAG, "Failed to send write request");
    return false;
  }

  // Receive response
  std::vector<uint8_t> response;
  if (!this->receive_smb2_(response)) {
    ESP_LOGW(TAG, "Failed to receive write response");
    return false;
  }

  // Parse response header
  SMB2Header response_header;
  if (!response_header.decode(response.data(), response.size())) {
    ESP_LOGW(TAG, "Failed to decode write response header");
    return false;
  }

  if (response_header.status != STATUS_SUCCESS) {
    ESP_LOGW(TAG, "Write failed with status 0x%08X", response_header.status);
    return false;
  }

  ESP_LOGD(TAG, "SMB2 Write successful");
  return true;
}

bool SMBClient::smb2_query_directory_(const SMB2FileId &file_id, const std::string &pattern,
                                      std::vector<SMB2DirEntry> &entries) {
  ESP_LOGD(TAG, "SMB2 Query Directory: %s", pattern.c_str());

  entries.clear();
  bool first_query = true;

  while (true) {
    // Build query directory request
    std::vector<uint8_t> request;

    // SMB2 header
    SMB2Header header;
    header.command = SMB2_QUERY_DIRECTORY;
    header.message_id = this->next_message_id_();
    header.session_id = this->session_id_;
    header.tree_id = this->tree_id_;
    header.encode(request);

    // Query directory request body
    // Structure size (2 bytes): 33
    this->encode_uint16_(request, 33);

    // File info class (1 byte) - FileIdBothDirectoryInformation
    request.push_back(FileIdBothDirectoryInformation);

    // Flags (1 byte)
    uint8_t flags = 0;
    if (first_query) {
      flags |= 0x02;  // SMB2_RESTART_SCANS
      first_query = false;
    }
    request.push_back(flags);

    // File index (4 bytes)
    this->encode_uint32_(request, 0);

    // File ID (16 bytes)
    file_id.encode(request);

    // Convert pattern to UTF-16 LE
    std::string pattern_utf16 = this->to_utf16le_(pattern);

    // File name offset (2 bytes)
    uint16_t name_offset = SMB2_HEADER_SIZE + 32;
    this->encode_uint16_(request, name_offset);

    // File name length (2 bytes)
    this->encode_uint16_(request, pattern_utf16.size());

    // Output buffer length (4 bytes) - request up to 64KB
    this->encode_uint32_(request, 65536);

    // File name (UTF-16 LE)
    request.insert(request.end(), pattern_utf16.begin(), pattern_utf16.end());

    // Send request
    if (!this->send_smb2_(request)) {
      ESP_LOGW(TAG, "Failed to send query directory request");
      return false;
    }

    // Receive response
    std::vector<uint8_t> response;
    if (!this->receive_smb2_(response)) {
      ESP_LOGW(TAG, "Failed to receive query directory response");
      return false;
    }

    // Parse response header
    SMB2Header response_header;
    if (!response_header.decode(response.data(), response.size())) {
      ESP_LOGW(TAG, "Failed to decode query directory response header");
      return false;
    }

    if (response_header.status == STATUS_NO_MORE_FILES) {
      // No more files - end of directory
      break;
    }

    if (response_header.status != STATUS_SUCCESS) {
      ESP_LOGW(TAG, "Query directory failed with status 0x%08X", response_header.status);
      return false;
    }

    // Parse query directory response body
    if (response.size() < SMB2_HEADER_SIZE + 8) {
      ESP_LOGW(TAG, "Query directory response too short");
      return false;
    }

    const uint8_t *body = response.data() + SMB2_HEADER_SIZE;

    // Output buffer offset (offset 2, 2 bytes)
    uint16_t buffer_offset = this->decode_uint16_(body + 2);

    // Output buffer length (offset 4, 4 bytes)
    uint32_t buffer_length = this->decode_uint32_(body + 4);

    // Parse directory entries
    if (buffer_offset + buffer_length > response.size()) {
      ESP_LOGW(TAG, "Invalid buffer offset/length in query directory response");
      return false;
    }

    const uint8_t *dir_data = response.data() + buffer_offset;
    size_t pos = 0;

    while (pos < buffer_length) {
      // Next entry offset (4 bytes)
      uint32_t next_offset = this->decode_uint32_(dir_data + pos);

      // File index (4 bytes, offset 4)
      // File name length (4 bytes, offset 60)
      uint32_t filename_length = this->decode_uint32_(dir_data + pos + 60);

      // File attributes (4 bytes, offset 56)
      uint32_t file_attributes = this->decode_uint32_(dir_data + pos + 56);

      // File size (8 bytes, offset 48)
      uint64_t file_size = this->decode_uint64_(dir_data + pos + 48);

      // File name (UTF-16 LE, offset 104)
      std::string filename = this->from_utf16le_(dir_data + pos + 104, filename_length);

      // Skip "." and ".." entries
      if (filename != "." && filename != "..") {
        SMB2DirEntry entry;
        entry.filename = filename;
        entry.file_size = file_size;
        entry.file_attributes = file_attributes;
        entries.push_back(entry);
      }

      // Move to next entry
      if (next_offset == 0) {
        break;
      }
      pos += next_offset;
    }

    // If we got less than requested, we're done
    if (buffer_length < 65536) {
      break;
    }
  }

  ESP_LOGI(TAG, "SMB2 Query Directory successful: found %u entries", entries.size());
  return true;
}

std::vector<uint8_t> SMBClient::ntlm_hash_(const std::string &password) {
  // NTLM hash = MD4(password_utf16le)
  std::string password_utf16 = this->to_utf16le_(password);
  std::vector<uint8_t> hash(16);

#ifdef USE_ESP_IDF
  mbedtls_md4_context ctx;
  mbedtls_md4_init(&ctx);
  mbedtls_md4_starts(&ctx);
  mbedtls_md4_update(&ctx, reinterpret_cast<const uint8_t *>(password_utf16.data()), password_utf16.size());
  mbedtls_md4_finish(&ctx, hash.data());
  mbedtls_md4_free(&ctx);
#else
  // Arduino: Use simplified MD4 (not available in standard libraries)
  // For Arduino, we'll need to implement or use a library
  // For now, use a placeholder - this would need proper MD4 implementation
  ESP_LOGW(TAG, "MD4 not fully implemented for Arduino framework");
  // TODO: Add MD4 implementation for Arduino
#endif

  return hash;
}

std::vector<uint8_t> SMBClient::ntlmv2_hash_(const std::string &username, const std::string &password,
                                             const std::string &domain) {
  // NTLMv2 hash = HMAC-MD5(NTLM_hash, uppercase(username) + domain)
  std::vector<uint8_t> ntlm_hash = this->ntlm_hash_(password);

  // Create user + domain string (uppercase username)
  std::string user_upper = username;
  std::transform(user_upper.begin(), user_upper.end(), user_upper.begin(), ::toupper);
  std::string user_domain_utf16 = this->to_utf16le_(user_upper + domain);

  std::vector<uint8_t> hash(16);

#ifdef USE_ESP_IDF
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_MD5), 1);  // 1 = HMAC mode
  mbedtls_md_hmac_starts(&ctx, ntlm_hash.data(), ntlm_hash.size());
  mbedtls_md_hmac_update(&ctx, reinterpret_cast<const uint8_t *>(user_domain_utf16.data()), user_domain_utf16.size());
  mbedtls_md_hmac_finish(&ctx, hash.data());
  mbedtls_md_free(&ctx);
#else
  // Arduino: Use MD5 library
  MD5 md5;
  // HMAC-MD5 implementation
  uint8_t ipad[64], opad[64];
  std::memset(ipad, 0x36, 64);
  std::memset(opad, 0x5C, 64);

  // XOR key with pads
  for (size_t i = 0; i < ntlm_hash.size() && i < 64; i++) {
    ipad[i] ^= ntlm_hash[i];
    opad[i] ^= ntlm_hash[i];
  }

  // Inner hash: MD5(ipad || message)
  md5.begin();
  md5.add(ipad, 64);
  md5.add(reinterpret_cast<const uint8_t *>(user_domain_utf16.data()), user_domain_utf16.size());
  uint8_t inner_hash[16];
  md5.calculate();
  md5.getBytes(inner_hash);

  // Outer hash: MD5(opad || inner_hash)
  md5.begin();
  md5.add(opad, 64);
  md5.add(inner_hash, 16);
  md5.calculate();
  md5.getBytes(hash.data());
#endif

  return hash;
}

std::vector<uint8_t> SMBClient::compute_ntlmv2_response_(const std::vector<uint8_t> &ntlmv2_hash,
                                                         const std::vector<uint8_t> &server_challenge,
                                                         const std::vector<uint8_t> &client_challenge,
                                                         uint64_t timestamp,
                                                         const std::vector<uint8_t> &target_info) {
  // NTLMv2 response = HMAC-MD5(ntlmv2_hash, server_challenge + blob)
  // Blob = 0x01010000 + reserved(4) + timestamp(8) + client_challenge(8) + 0x00000000 + target_info + 0x00000000

  std::vector<uint8_t> blob;
  // Blob version and hi-version
  blob.push_back(0x01);
  blob.push_back(0x01);
  blob.push_back(0x00);
  blob.push_back(0x00);

  // Reserved (4 bytes)
  for (int i = 0; i < 4; i++) {
    blob.push_back(0x00);
  }

  // Timestamp (8 bytes, little-endian)
  this->encode_uint64_(blob, timestamp);

  // Client challenge (8 bytes)
  blob.insert(blob.end(), client_challenge.begin(), client_challenge.end());

  // Reserved (4 bytes)
  for (int i = 0; i < 4; i++) {
    blob.push_back(0x00);
  }

  // Target info
  blob.insert(blob.end(), target_info.begin(), target_info.end());

  // Terminator (4 bytes)
  for (int i = 0; i < 4; i++) {
    blob.push_back(0x00);
  }

  // Create message: server_challenge + blob
  std::vector<uint8_t> message;
  message.insert(message.end(), server_challenge.begin(), server_challenge.end());
  message.insert(message.end(), blob.begin(), blob.end());

  // Compute HMAC-MD5
  std::vector<uint8_t> response(16);

#ifdef USE_ESP_IDF
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_MD5), 1);  // 1 = HMAC mode
  mbedtls_md_hmac_starts(&ctx, ntlmv2_hash.data(), ntlmv2_hash.size());
  mbedtls_md_hmac_update(&ctx, message.data(), message.size());
  mbedtls_md_hmac_finish(&ctx, response.data());
  mbedtls_md_free(&ctx);
#else
  // Arduino: Use MD5 library for HMAC-MD5
  MD5 md5;
  uint8_t ipad[64], opad[64];
  std::memset(ipad, 0x36, 64);
  std::memset(opad, 0x5C, 64);

  for (size_t i = 0; i < ntlmv2_hash.size() && i < 64; i++) {
    ipad[i] ^= ntlmv2_hash[i];
    opad[i] ^= ntlmv2_hash[i];
  }

  md5.begin();
  md5.add(ipad, 64);
  md5.add(message.data(), message.size());
  uint8_t inner_hash[16];
  md5.calculate();
  md5.getBytes(inner_hash);

  md5.begin();
  md5.add(opad, 64);
  md5.add(inner_hash, 16);
  md5.calculate();
  md5.getBytes(response.data());
#endif

  // Return response + blob
  std::vector<uint8_t> result;
  result.insert(result.end(), response.begin(), response.end());
  result.insert(result.end(), blob.begin(), blob.end());
  return result;
}

std::vector<uint8_t> SMBClient::create_ntlm_negotiate_() {
  // NTLM Negotiate message
  std::vector<uint8_t> message;

  // Signature (8 bytes): "NTLMSSP\0"
  message.push_back('N');
  message.push_back('T');
  message.push_back('L');
  message.push_back('M');
  message.push_back('S');
  message.push_back('S');
  message.push_back('P');
  message.push_back(0x00);

  // Message type (4 bytes): 1 = negotiate
  this->encode_uint32_(message, NTLM_NEGOTIATE);

  // Flags (4 bytes)
  uint32_t flags = NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_NEGOTIATE_NTLM | NTLMSSP_REQUEST_TARGET |
                   NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY | NTLMSSP_NEGOTIATE_ALWAYS_SIGN |
                   NTLMSSP_NEGOTIATE_128;
  this->encode_uint32_(message, flags);

  // Domain (8 bytes: length, length, offset) - empty
  this->encode_uint16_(message, 0);  // Length
  this->encode_uint16_(message, 0);  // Max length
  this->encode_uint32_(message, 0);  // Offset

  // Workstation (8 bytes: length, length, offset) - empty
  this->encode_uint16_(message, 0);  // Length
  this->encode_uint16_(message, 0);  // Max length
  this->encode_uint32_(message, 0);  // Offset

  // Version (8 bytes) - optional, we'll skip for now

  ESP_LOGD(TAG, "Created NTLM negotiate message (%u bytes)", message.size());
  return message;
}

std::vector<uint8_t> SMBClient::create_ntlm_authenticate_(const std::vector<uint8_t> &challenge_message) {
  // Parse challenge message to extract server challenge and target info
  if (challenge_message.size() < 48) {
    ESP_LOGW(TAG, "Invalid NTLM challenge message (too short)");
    return {};
  }

  // Extract server challenge (8 bytes at offset 24)
  std::vector<uint8_t> server_challenge(challenge_message.begin() + 24, challenge_message.begin() + 32);

  // Extract target info from challenge message
  // Target info offset is at byte 40-43, length at byte 36-37
  uint16_t target_info_length = this->decode_uint16_(challenge_message.data() + 40);
  uint32_t target_info_offset = this->decode_uint32_(challenge_message.data() + 44);

  std::vector<uint8_t> target_info;
  if (target_info_offset > 0 && target_info_offset + target_info_length <= challenge_message.size()) {
    target_info.assign(challenge_message.begin() + target_info_offset,
                       challenge_message.begin() + target_info_offset + target_info_length);
  }

  // Generate client challenge (8 random bytes)
  std::vector<uint8_t> client_challenge(8);
  for (int i = 0; i < 8; i++) {
    client_challenge[i] = rand() & 0xFF;
  }

  // Get current timestamp (Windows FILETIME format: 100ns intervals since 1601-01-01)
  // For embedded systems, we'll use a fixed epoch + millis()
  uint64_t timestamp = 116444736000000000ULL + (static_cast<uint64_t>(millis()) * 10000ULL);

  // Compute NTLMv2 hash and response
  std::vector<uint8_t> ntlmv2_hash = this->ntlmv2_hash_(this->username_, this->password_, this->domain_);
  std::vector<uint8_t> ntlmv2_response =
      this->compute_ntlmv2_response_(ntlmv2_hash, server_challenge, client_challenge, timestamp, target_info);

  // Build authenticate message
  std::vector<uint8_t> message;

  // Signature (8 bytes)
  message.push_back('N');
  message.push_back('T');
  message.push_back('L');
  message.push_back('M');
  message.push_back('S');
  message.push_back('S');
  message.push_back('P');
  message.push_back(0x00);

  // Message type (4 bytes): 3 = authenticate
  this->encode_uint32_(message, NTLM_AUTHENTICATE);

  // LM response (8 bytes: length, length, offset) - empty for NTLMv2
  size_t lm_offset = 64;  // After fixed header
  this->encode_uint16_(message, 0);
  this->encode_uint16_(message, 0);
  this->encode_uint32_(message, lm_offset);

  // NTLM response (8 bytes: length, length, offset)
  size_t ntlm_offset = lm_offset;
  this->encode_uint16_(message, ntlmv2_response.size());
  this->encode_uint16_(message, ntlmv2_response.size());
  this->encode_uint32_(message, ntlm_offset);

  // Domain (8 bytes)
  std::string domain_utf16 = this->to_utf16le_(this->domain_);
  size_t domain_offset = ntlm_offset + ntlmv2_response.size();
  this->encode_uint16_(message, domain_utf16.size());
  this->encode_uint16_(message, domain_utf16.size());
  this->encode_uint32_(message, domain_offset);

  // Username (8 bytes)
  std::string username_utf16 = this->to_utf16le_(this->username_);
  size_t username_offset = domain_offset + domain_utf16.size();
  this->encode_uint16_(message, username_utf16.size());
  this->encode_uint16_(message, username_utf16.size());
  this->encode_uint32_(message, username_offset);

  // Workstation (8 bytes) - empty
  size_t workstation_offset = username_offset + username_utf16.size();
  this->encode_uint16_(message, 0);
  this->encode_uint16_(message, 0);
  this->encode_uint32_(message, workstation_offset);

  // Session key (8 bytes) - empty
  this->encode_uint16_(message, 0);
  this->encode_uint16_(message, 0);
  this->encode_uint32_(message, workstation_offset);

  // Flags (4 bytes)
  uint32_t flags = NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_NEGOTIATE_NTLM | NTLMSSP_REQUEST_TARGET |
                   NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY | NTLMSSP_NEGOTIATE_ALWAYS_SIGN |
                   NTLMSSP_NEGOTIATE_128;
  this->encode_uint32_(message, flags);

  // Append payload data
  message.insert(message.end(), ntlmv2_response.begin(), ntlmv2_response.end());
  message.insert(message.end(), domain_utf16.begin(), domain_utf16.end());
  message.insert(message.end(), username_utf16.begin(), username_utf16.end());

  ESP_LOGD(TAG, "Created NTLM authenticate message (%u bytes)", message.size());
  return message;
}

// High-level file operations
bool SMBClient::read_file(const std::string &path, std::vector<uint8_t> &data) {
  if (!this->connected_) {
    ESP_LOGW(TAG, "Not connected to SMB server");
    return false;
  }

  ESP_LOGI(TAG, "Reading file: %s", path.c_str());

  // Open file for reading
  SMB2FileId file_id;
  uint32_t desired_access = GENERIC_READ | FILE_READ_ATTRIBUTES;
  uint32_t file_attributes = FILE_ATTRIBUTE_NORMAL;
  uint32_t create_disposition = FILE_OPEN;
  uint32_t create_options = FILE_NON_DIRECTORY_FILE;

  if (!this->smb2_create_(path, desired_access, file_attributes, create_disposition, create_options, file_id)) {
    ESP_LOGW(TAG, "Failed to open file for reading");
    return false;
  }

  // Read file in chunks (64KB at a time)
  data.clear();
  uint64_t offset = 0;
  constexpr uint32_t chunk_size = 65536;

  while (true) {
    std::vector<uint8_t> chunk;
    if (!this->smb2_read_(file_id, offset, chunk_size, chunk)) {
      ESP_LOGW(TAG, "Failed to read file chunk at offset %llu", offset);
      this->smb2_close_(file_id);
      return false;
    }

    if (chunk.empty()) {
      // End of file
      break;
    }

    data.insert(data.end(), chunk.begin(), chunk.end());
    offset += chunk.size();

    // If we got less than chunk_size, we've reached EOF
    if (chunk.size() < chunk_size) {
      break;
    }
  }

  // Close file
  this->smb2_close_(file_id);

  ESP_LOGI(TAG, "Successfully read file: %u bytes", data.size());
  return true;
}

bool SMBClient::write_file(const std::string &path, const uint8_t *data, size_t length) {
  if (!this->connected_) {
    ESP_LOGW(TAG, "Not connected to SMB server");
    return false;
  }

  ESP_LOGI(TAG, "Writing file: %s (%u bytes)", path.c_str(), length);

  // Open/create file for writing
  SMB2FileId file_id;
  uint32_t desired_access = GENERIC_WRITE | FILE_WRITE_ATTRIBUTES;
  uint32_t file_attributes = FILE_ATTRIBUTE_NORMAL;
  uint32_t create_disposition = FILE_OVERWRITE_IF;
  uint32_t create_options = FILE_NON_DIRECTORY_FILE;

  if (!this->smb2_create_(path, desired_access, file_attributes, create_disposition, create_options, file_id)) {
    ESP_LOGW(TAG, "Failed to open file for writing");
    return false;
  }

  // Write file in chunks (64KB at a time)
  uint64_t offset = 0;
  constexpr uint32_t chunk_size = 65536;

  while (offset < length) {
    uint32_t write_size = std::min(chunk_size, static_cast<uint32_t>(length - offset));

    if (!this->smb2_write_(file_id, offset, data + offset, write_size)) {
      ESP_LOGW(TAG, "Failed to write file chunk at offset %llu", offset);
      this->smb2_close_(file_id);
      return false;
    }

    offset += write_size;
  }

  // Close file
  this->smb2_close_(file_id);

  ESP_LOGI(TAG, "Successfully wrote file: %u bytes", length);
  return true;
}

bool SMBClient::delete_file(const std::string &path) {
  if (!this->connected_) {
    ESP_LOGW(TAG, "Not connected to SMB server");
    return false;
  }

  ESP_LOGI(TAG, "Deleting file: %s", path.c_str());

  // Open file with DELETE access and FILE_DELETE_ON_CLOSE option
  SMB2FileId file_id;
  uint32_t desired_access = DELETE;
  uint32_t file_attributes = FILE_ATTRIBUTE_NORMAL;
  uint32_t create_disposition = FILE_OPEN;
  uint32_t create_options = FILE_NON_DIRECTORY_FILE | FILE_DELETE_ON_CLOSE;

  if (!this->smb2_create_(path, desired_access, file_attributes, create_disposition, create_options, file_id)) {
    ESP_LOGW(TAG, "Failed to open file for deletion");
    return false;
  }

  // Close file (will be deleted on close due to FILE_DELETE_ON_CLOSE)
  if (!this->smb2_close_(file_id)) {
    ESP_LOGW(TAG, "Failed to close file after deletion");
    return false;
  }

  ESP_LOGI(TAG, "Successfully deleted file");
  return true;
}

bool SMBClient::file_exists(const std::string &path) {
  if (!this->connected_) {
    ESP_LOGW(TAG, "Not connected to SMB server");
    return false;
  }

  // Try to open file for reading
  SMB2FileId file_id;
  uint32_t desired_access = FILE_READ_ATTRIBUTES;
  uint32_t file_attributes = FILE_ATTRIBUTE_NORMAL;
  uint32_t create_disposition = FILE_OPEN;
  uint32_t create_options = FILE_NON_DIRECTORY_FILE;

  if (!this->smb2_create_(path, desired_access, file_attributes, create_disposition, create_options, file_id)) {
    return false;
  }

  // Close file
  this->smb2_close_(file_id);
  return true;
}

bool SMBClient::list_directory(const std::string &path, std::vector<SMB2DirEntry> &entries) {
  if (!this->connected_) {
    ESP_LOGW(TAG, "Not connected to SMB server");
    return false;
  }

  ESP_LOGI(TAG, "Listing directory: %s", path.c_str());

  // Open directory
  SMB2FileId file_id;
  uint32_t desired_access = FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES;
  uint32_t file_attributes = FILE_ATTRIBUTE_DIRECTORY;
  uint32_t create_disposition = FILE_OPEN;
  uint32_t create_options = FILE_DIRECTORY_FILE;

  if (!this->smb2_create_(path, desired_access, file_attributes, create_disposition, create_options, file_id)) {
    ESP_LOGW(TAG, "Failed to open directory");
    return false;
  }

  // Query directory with wildcard pattern
  if (!this->smb2_query_directory_(file_id, "*", entries)) {
    ESP_LOGW(TAG, "Failed to query directory");
    this->smb2_close_(file_id);
    return false;
  }

  // Close directory
  this->smb2_close_(file_id);

  ESP_LOGI(TAG, "Successfully listed directory: %u entries", entries.size());
  return true;
}

bool SMBClient::create_directory(const std::string &path) {
  if (!this->connected_) {
    ESP_LOGW(TAG, "Not connected to SMB server");
    return false;
  }

  ESP_LOGI(TAG, "Creating directory: %s", path.c_str());

  // Create directory
  SMB2FileId file_id;
  uint32_t desired_access = FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES;
  uint32_t file_attributes = FILE_ATTRIBUTE_DIRECTORY;
  uint32_t create_disposition = FILE_CREATE;
  uint32_t create_options = FILE_DIRECTORY_FILE;

  if (!this->smb2_create_(path, desired_access, file_attributes, create_disposition, create_options, file_id)) {
    ESP_LOGW(TAG, "Failed to create directory");
    return false;
  }

  // Close directory
  this->smb2_close_(file_id);

  ESP_LOGI(TAG, "Successfully created directory");
  return true;
}

bool SMBClient::delete_directory(const std::string &path) {
  if (!this->connected_) {
    ESP_LOGW(TAG, "Not connected to SMB server");
    return false;
  }

  ESP_LOGI(TAG, "Deleting directory: %s", path.c_str());

  // Open directory with DELETE access and FILE_DELETE_ON_CLOSE option
  SMB2FileId file_id;
  uint32_t desired_access = DELETE;
  uint32_t file_attributes = FILE_ATTRIBUTE_DIRECTORY;
  uint32_t create_disposition = FILE_OPEN;
  uint32_t create_options = FILE_DIRECTORY_FILE | FILE_DELETE_ON_CLOSE;

  if (!this->smb2_create_(path, desired_access, file_attributes, create_disposition, create_options, file_id)) {
    ESP_LOGW(TAG, "Failed to open directory for deletion");
    return false;
  }

  // Close directory (will be deleted on close due to FILE_DELETE_ON_CLOSE)
  if (!this->smb2_close_(file_id)) {
    ESP_LOGW(TAG, "Failed to close directory after deletion");
    return false;
  }

  ESP_LOGI(TAG, "Successfully deleted directory");
  return true;
}

#if defined(USE_STORAGE_HOST)
// NetworkStorage interface override - converts SMB2DirEntry to NetworkStorage::DirEntry
bool SMBClient::list_directory(const std::string &path,
                                std::vector<storage_host::NetworkStorage::DirEntry> &entries) {
  // Call existing SMB-specific list_directory
  std::vector<SMB2DirEntry> smb_entries;
  if (!this->list_directory(path, smb_entries)) {
    return false;
  }

  // Convert SMB2DirEntry to NetworkStorage::DirEntry
  entries.clear();
  entries.reserve(smb_entries.size());
  for (const auto &smb_entry : smb_entries) {
    storage_host::NetworkStorage::DirEntry entry;
    entry.name = smb_entry.filename;
    entry.size = smb_entry.file_size;
    entry.is_directory = smb_entry.is_directory();
    entries.push_back(entry);
  }

  return true;
}
#endif

}  // namespace smb_client
}  // namespace esphome
