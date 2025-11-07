#include "tftp_server.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <cstring>
#include <algorithm>

// Include filesystem headers based on platform
#ifdef USE_ESP_IDF
#include <sys/stat.h>
#include <dirent.h>
#endif

namespace esphome {
namespace tftp_server {

//========================================================================
// TFTP Packet Implementation
//========================================================================

TFTPPacket TFTPPacket::parse(const uint8_t *buffer, size_t length) {
  TFTPPacket packet;

  if (length < 2) {
    ESP_LOGE(TAG, "TFTP packet too short: %zu bytes", length);
    packet.opcode = 0;
    return packet;
  }

  // Parse opcode (big-endian)
  packet.opcode = (buffer[0] << 8) | buffer[1];

  // Parse data
  if (length > 2) {
    packet.data.assign(buffer + 2, buffer + length);
  }

  return packet;
}

std::vector<uint8_t> TFTPPacket::serialize() const {
  std::vector<uint8_t> buffer;

  // Opcode (big-endian)
  buffer.push_back((this->opcode >> 8) & 0xFF);
  buffer.push_back(this->opcode & 0xFF);

  // Data
  buffer.insert(buffer.end(), this->data.begin(), this->data.end());

  return buffer;
}

TFTPPacket TFTPPacket::create_data(uint16_t block_number, const uint8_t *data, size_t length) {
  TFTPPacket packet;
  packet.opcode = TFTP_OPCODE_DATA;

  // Block number (big-endian)
  packet.data.push_back((block_number >> 8) & 0xFF);
  packet.data.push_back(block_number & 0xFF);

  // Data
  if (data && length > 0) {
    packet.data.insert(packet.data.end(), data, data + length);
  }

  return packet;
}

TFTPPacket TFTPPacket::create_ack(uint16_t block_number) {
  TFTPPacket packet;
  packet.opcode = TFTP_OPCODE_ACK;

  // Block number (big-endian)
  packet.data.push_back((block_number >> 8) & 0xFF);
  packet.data.push_back(block_number & 0xFF);

  return packet;
}

TFTPPacket TFTPPacket::create_error(TFTPErrorCode error_code, const std::string &error_msg) {
  TFTPPacket packet;
  packet.opcode = TFTP_OPCODE_ERROR;

  // Error code (big-endian)
  packet.data.push_back((error_code >> 8) & 0xFF);
  packet.data.push_back(error_code & 0xFF);

  // Error message (null-terminated)
  packet.data.insert(packet.data.end(), error_msg.begin(), error_msg.end());
  packet.data.push_back(0);

  return packet;
}

uint16_t TFTPPacket::get_block_number() const {
  if (this->data.size() < 2) {
    return 0;
  }
  return (this->data[0] << 8) | this->data[1];
}

std::string TFTPPacket::get_filename() const {
  // For RRQ/WRQ packets, filename is null-terminated string at start of data
  if (this->data.empty()) {
    return "";
  }

  const char *str = reinterpret_cast<const char *>(this->data.data());
  size_t max_len = this->data.size();

  // Find null terminator
  size_t len = 0;
  while (len < max_len && str[len] != '\0') {
    len++;
  }

  return std::string(str, len);
}

std::string TFTPPacket::get_mode() const {
  // For RRQ/WRQ packets, mode is second null-terminated string
  if (this->data.empty()) {
    return "";
  }

  const char *str = reinterpret_cast<const char *>(this->data.data());
  size_t max_len = this->data.size();

  // Skip filename (find first null terminator)
  size_t pos = 0;
  while (pos < max_len && str[pos] != '\0') {
    pos++;
  }
  pos++;  // Skip null terminator

  if (pos >= max_len) {
    return "";
  }

  // Find mode string
  size_t mode_start = pos;
  while (pos < max_len && str[pos] != '\0') {
    pos++;
  }

  return std::string(&str[mode_start], pos - mode_start);
}

//========================================================================
// TFTPServer Implementation
//========================================================================

TFTPServer::~TFTPServer() { this->close_socket_(); }

void TFTPServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TFTP Server...");
  ESP_LOGCONFIG(TAG, "  Port: %u", this->port_);
  ESP_LOGCONFIG(TAG, "  Root directory: %s", this->root_dir_.c_str());
  ESP_LOGCONFIG(TAG, "  Access mode: %u", this->access_mode_);
  ESP_LOGCONFIG(TAG, "  Max file size: %zu bytes", this->max_file_size_);
  ESP_LOGCONFIG(TAG, "  Max sessions: %u", this->max_sessions_);

  // Initialize socket
  if (!this->init_socket_()) {
    ESP_LOGE(TAG, "Failed to initialize UDP socket");
    this->mark_failed();
    return;
  }

  this->running_ = true;
  ESP_LOGI(TAG, "TFTP Server started on port %u", this->port_);
}

void TFTPServer::loop() {
  if (!this->running_) {
    return;
  }

  // Receive TFTP packet
  TFTPPacket packet;
  std::string client_addr;
  uint16_t client_port;

  if (this->receive_packet_(packet, client_addr, client_port)) {
    // Handle packet based on opcode
    switch (packet.opcode) {
      case TFTP_OPCODE_RRQ: {
        std::string filename = packet.get_filename();
        std::string mode = packet.get_mode();
        ESP_LOGI(TAG, "RRQ from %s:%u for file: %s (mode: %s)", client_addr.c_str(), client_port, filename.c_str(),
                 mode.c_str());
        this->handle_rrq_(filename, mode, client_addr, client_port);
        break;
      }

      case TFTP_OPCODE_WRQ: {
        std::string filename = packet.get_filename();
        std::string mode = packet.get_mode();
        ESP_LOGI(TAG, "WRQ from %s:%u for file: %s (mode: %s)", client_addr.c_str(), client_port, filename.c_str(),
                 mode.c_str());
        this->handle_wrq_(filename, mode, client_addr, client_port);
        break;
      }

      case TFTP_OPCODE_ACK: {
        uint16_t block_number = packet.get_block_number();
        ESP_LOGVV(TAG, "ACK from %s:%u for block %u", client_addr.c_str(), client_port, block_number);
        this->handle_ack_(block_number, client_addr, client_port);
        break;
      }

      case TFTP_OPCODE_DATA: {
        uint16_t block_number = packet.get_block_number();
        const uint8_t *data = packet.data.data() + 2;  // Skip block number
        size_t length = packet.data.size() - 2;
        ESP_LOGVV(TAG, "DATA from %s:%u block %u (%zu bytes)", client_addr.c_str(), client_port, block_number,
                  length);
        this->handle_data_(block_number, data, length, client_addr, client_port);
        break;
      }

      case TFTP_OPCODE_ERROR: {
        ESP_LOGW(TAG, "ERROR from %s:%u", client_addr.c_str(), client_port);
        this->remove_session_(client_addr, client_port);
        break;
      }

      default:
        ESP_LOGW(TAG, "Unknown TFTP opcode: %u from %s:%u", packet.opcode, client_addr.c_str(), client_port);
        this->send_error_(client_addr, client_port, TFTP_ERROR_ILLEGAL_OPERATION, "Illegal TFTP operation");
        break;
    }
  }

  // Cleanup expired sessions
  this->cleanup_expired_sessions_();
}

void TFTPServer::dump_config() {
  ESP_LOGCONFIG(TAG, "TFTP Server:");
  ESP_LOGCONFIG(TAG, "  Port: %u", this->port_);
  ESP_LOGCONFIG(TAG, "  Root directory: %s", this->root_dir_.c_str());
  ESP_LOGCONFIG(TAG, "  Status: %s", this->running_ ? "Running" : "Stopped");
  ESP_LOGCONFIG(TAG, "  Active sessions: %zu", this->sessions_.size());
}

//========================================================================
// Socket Operations
//========================================================================

bool TFTPServer::init_socket_() {
#ifdef USE_ESP_IDF
  // Create UDP socket
  this->socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (this->socket_ < 0) {
    ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
    return false;
  }

  // Bind to port
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(this->port_);

  if (bind(this->socket_, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    ESP_LOGE(TAG, "Failed to bind socket: errno %d", errno);
    close(this->socket_);
    this->socket_ = -1;
    return false;
  }

  // Set non-blocking mode
  int flags = fcntl(this->socket_, F_GETFL, 0);
  fcntl(this->socket_, F_SETFL, flags | O_NONBLOCK);

  ESP_LOGD(TAG, "UDP socket initialized and bound to port %u (fd=%d)", this->port_, this->socket_);
  return true;
#else
  // Arduino WiFiUDP
  this->udp_ = std::make_unique<WiFiUDP>();
  if (!this->udp_->begin(this->port_)) {
    ESP_LOGE(TAG, "Failed to initialize WiFiUDP on port %u", this->port_);
    return false;
  }

  ESP_LOGD(TAG, "WiFiUDP initialized on port %u", this->port_);
  return true;
#endif
}

void TFTPServer::close_socket_() {
#ifdef USE_ESP_IDF
  if (this->socket_ >= 0) {
    close(this->socket_);
    this->socket_ = -1;
  }
#else
  if (this->udp_) {
    this->udp_->stop();
    this->udp_ = nullptr;
  }
#endif

  // Clear all sessions
  this->sessions_.clear();
  this->running_ = false;
}

bool TFTPServer::send_packet_(const TFTPPacket &packet, const std::string &addr, uint16_t port) {
  std::vector<uint8_t> buffer = packet.serialize();

  ESP_LOGVV(TAG, "Sending TFTP packet: opcode=%u, size=%zu bytes to %s:%u", packet.opcode, buffer.size(),
            addr.c_str(), port);

#ifdef USE_ESP_IDF
  // Resolve address
  struct sockaddr_in dest_addr;
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(port);
  inet_pton(AF_INET, addr.c_str(), &dest_addr.sin_addr);

  // Send packet
  int sent = sendto(this->socket_, buffer.data(), buffer.size(), 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
  if (sent < 0) {
    ESP_LOGE(TAG, "Failed to send packet: errno %d", errno);
    return false;
  }

  return true;
#else
  // Arduino WiFiUDP
  if (!this->udp_->beginPacket(addr.c_str(), port)) {
    ESP_LOGE(TAG, "Failed to begin UDP packet");
    return false;
  }

  this->udp_->write(buffer.data(), buffer.size());

  if (!this->udp_->endPacket()) {
    ESP_LOGE(TAG, "Failed to send UDP packet");
    return false;
  }

  return true;
#endif
}

bool TFTPServer::receive_packet_(TFTPPacket &packet, std::string &addr, uint16_t &port) {
#ifdef USE_ESP_IDF
  uint8_t buffer[TFTP_MAX_PACKET_SIZE];
  struct sockaddr_in src_addr;
  socklen_t src_addr_len = sizeof(src_addr);

  // Receive packet
  int received = recvfrom(this->socket_, buffer, sizeof(buffer), 0, (struct sockaddr *) &src_addr, &src_addr_len);
  if (received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false;  // No data available
    }
    ESP_LOGW(TAG, "Failed to receive packet: errno %d", errno);
    return false;
  }

  // Parse packet
  packet = TFTPPacket::parse(buffer, received);

  // Get source address and port
  char addr_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &src_addr.sin_addr, addr_str, sizeof(addr_str));
  addr = addr_str;
  port = ntohs(src_addr.sin_port);

  return true;
#else
  // Arduino WiFiUDP
  int packet_size = this->udp_->parsePacket();
  if (packet_size <= 0) {
    return false;
  }

  uint8_t buffer[TFTP_MAX_PACKET_SIZE];
  int received = this->udp_->read(buffer, sizeof(buffer));

  if (received > 0) {
    packet = TFTPPacket::parse(buffer, received);
    addr = this->udp_->remoteIP().toString().c_str();
    port = this->udp_->remotePort();
    return true;
  }

  return false;
#endif
}

//========================================================================
// Request Handlers
//========================================================================

void TFTPServer::handle_rrq_(const std::string &filename, const std::string &mode, const std::string &client_addr,
                             uint16_t client_port) {
  // Check if read access is allowed
  if (this->access_mode_ == ACCESS_WRITE_ONLY) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_ACCESS_VIOLATION, "Read access denied");
    return;
  }

  // Validate filename
  if (!this->validate_filename_(filename)) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_ACCESS_VIOLATION, "Invalid filename");
    return;
  }

  // Get full path
  std::string full_path = this->get_full_path_(filename);

  // Check if file exists and can be read
  if (!this->file_exists_(full_path)) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_FILE_NOT_FOUND, "File not found");
    return;
  }

  if (!this->can_read_(full_path)) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_ACCESS_VIOLATION, "Cannot read file");
    return;
  }

  // Check if we have room for another session
  if (this->sessions_.size() >= this->max_sessions_) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_NOT_DEFINED, "Too many active sessions");
    return;
  }

  // Create session
  TFTPMode tftp_mode = (mode == "netascii") ? TFTP_MODE_NETASCII : TFTP_MODE_OCTET;
  TFTPSession *session = this->create_session_(client_addr, client_port, filename, tftp_mode, false);
  if (!session) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_NOT_DEFINED, "Failed to create session");
    return;
  }

  // Read file into session buffer
  if (!this->read_file_(full_path, session->file_data)) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_NOT_DEFINED, "Failed to read file");
    this->remove_session_(client_addr, client_port);
    return;
  }

  ESP_LOGI(TAG, "Starting read transfer: %s (%zu bytes)", filename.c_str(), session->file_data.size());

  // Send first data block
  session->current_block = 1;
  this->send_data_block_(session);
}

void TFTPServer::handle_wrq_(const std::string &filename, const std::string &mode, const std::string &client_addr,
                             uint16_t client_port) {
  // Check if write access is allowed
  if (this->access_mode_ == ACCESS_READ_ONLY) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_ACCESS_VIOLATION, "Write access denied");
    return;
  }

  // Validate filename
  if (!this->validate_filename_(filename)) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_ACCESS_VIOLATION, "Invalid filename");
    return;
  }

  // Get full path
  std::string full_path = this->get_full_path_(filename);

  // Check if we can write
  if (!this->can_write_(full_path)) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_ACCESS_VIOLATION, "Cannot write file");
    return;
  }

  // Check if we have room for another session
  if (this->sessions_.size() >= this->max_sessions_) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_NOT_DEFINED, "Too many active sessions");
    return;
  }

  // Create session
  TFTPMode tftp_mode = (mode == "netascii") ? TFTP_MODE_NETASCII : TFTP_MODE_OCTET;
  TFTPSession *session = this->create_session_(client_addr, client_port, filename, tftp_mode, true);
  if (!session) {
    this->send_error_(client_addr, client_port, TFTP_ERROR_NOT_DEFINED, "Failed to create session");
    return;
  }

  ESP_LOGI(TAG, "Starting write transfer: %s", filename.c_str());

  // Send ACK for block 0 to indicate ready to receive
  TFTPPacket ack = TFTPPacket::create_ack(0);
  this->send_packet_(ack, client_addr, client_port);
}

void TFTPServer::handle_ack_(uint16_t block_number, const std::string &client_addr, uint16_t client_port) {
  TFTPSession *session = this->get_session_(client_addr, client_port);
  if (!session) {
    ESP_LOGW(TAG, "ACK for unknown session from %s:%u", client_addr.c_str(), client_port);
    return;
  }

  // Update last activity
  session->last_activity = millis();

  // Validate block number
  if (block_number != session->current_block) {
    ESP_LOGW(TAG, "Unexpected ACK block %u, expected %u", block_number, session->current_block);
    return;
  }

  // Check if transfer is complete
  size_t offset = (session->current_block - 1) * TFTP_DATA_SIZE;
  size_t remaining = (offset < session->file_data.size()) ? (session->file_data.size() - offset) : 0;

  if (remaining <= TFTP_DATA_SIZE && remaining < TFTP_DATA_SIZE) {
    // Last block was sent and acknowledged
    ESP_LOGI(TAG, "Read transfer complete: %s (%zu bytes)", session->filename.c_str(), session->file_data.size());
    this->remove_session_(client_addr, client_port);
    return;
  }

  // Send next block
  session->current_block++;
  this->send_data_block_(session);
}

void TFTPServer::handle_data_(uint16_t block_number, const uint8_t *data, size_t length,
                              const std::string &client_addr, uint16_t client_port) {
  TFTPSession *session = this->get_session_(client_addr, client_port);
  if (!session) {
    ESP_LOGW(TAG, "DATA for unknown session from %s:%u", client_addr.c_str(), client_port);
    this->send_error_(client_addr, client_port, TFTP_ERROR_UNKNOWN_TRANSFER_ID, "Unknown transfer ID");
    return;
  }

  // Update last activity
  session->last_activity = millis();

  // Validate block number
  uint16_t expected_block = session->current_block + 1;
  if (block_number != expected_block) {
    ESP_LOGW(TAG, "Unexpected DATA block %u, expected %u", block_number, expected_block);
    // Re-send ACK for current block
    TFTPPacket ack = TFTPPacket::create_ack(session->current_block);
    this->send_packet_(ack, client_addr, client_port);
    return;
  }

  // Check file size limit
  if (session->file_data.size() + length > this->max_file_size_) {
    ESP_LOGE(TAG, "File size exceeded limit: %zu + %zu > %zu", session->file_data.size(), length,
             this->max_file_size_);
    this->send_error_(client_addr, client_port, TFTP_ERROR_DISK_FULL, "File size limit exceeded");
    this->remove_session_(client_addr, client_port);
    return;
  }

  // Append data to session buffer
  session->file_data.insert(session->file_data.end(), data, data + length);
  session->current_block = block_number;

  // Send ACK
  TFTPPacket ack = TFTPPacket::create_ack(block_number);
  this->send_packet_(ack, client_addr, client_port);

  // Check if this is the last block (less than 512 bytes)
  if (length < TFTP_DATA_SIZE) {
    // Write file
    std::string full_path = this->get_full_path_(session->filename);
    if (this->write_file_(full_path, session->file_data.data(), session->file_data.size())) {
      ESP_LOGI(TAG, "Write transfer complete: %s (%zu bytes)", session->filename.c_str(), session->file_data.size());
    } else {
      ESP_LOGE(TAG, "Failed to write file: %s", session->filename.c_str());
      this->send_error_(client_addr, client_port, TFTP_ERROR_NOT_DEFINED, "Failed to write file");
    }
    this->remove_session_(client_addr, client_port);
  }
}

void TFTPServer::handle_error_(TFTPErrorCode error_code, const std::string &error_msg,
                               const std::string &client_addr, uint16_t client_port) {
  ESP_LOGW(TAG, "Client error from %s:%u: code=%u msg=%s", client_addr.c_str(), client_port, error_code,
           error_msg.c_str());
  this->remove_session_(client_addr, client_port);
}

//========================================================================
// Session Management
//========================================================================

TFTPSession *TFTPServer::get_session_(const std::string &client_addr, uint16_t client_port) {
  std::string key = client_addr + ":" + std::to_string(client_port);
  auto it = this->sessions_.find(key);
  return (it != this->sessions_.end()) ? it->second.get() : nullptr;
}

TFTPSession *TFTPServer::create_session_(const std::string &client_addr, uint16_t client_port,
                                         const std::string &filename, TFTPMode mode, bool is_write) {
  std::string key = client_addr + ":" + std::to_string(client_port);

  // Remove existing session if any
  this->sessions_.erase(key);

  // Create new session
  auto session = std::make_unique<TFTPSession>();
  session->client_addr = client_addr;
  session->client_port = client_port;
  session->filename = filename;
  session->mode = mode;
  session->is_write = is_write;
  session->current_block = 0;
  session->bytes_transferred = 0;
  session->last_activity = millis();

  TFTPSession *session_ptr = session.get();
  this->sessions_[key] = std::move(session);

  ESP_LOGD(TAG, "Created session for %s:%u (file: %s, write: %d)", client_addr.c_str(), client_port,
           filename.c_str(), is_write);

  return session_ptr;
}

void TFTPServer::remove_session_(const std::string &client_addr, uint16_t client_port) {
  std::string key = client_addr + ":" + std::to_string(client_port);
  auto it = this->sessions_.find(key);
  if (it != this->sessions_.end()) {
    ESP_LOGD(TAG, "Removed session for %s:%u", client_addr.c_str(), client_port);
    this->sessions_.erase(it);
  }
}

void TFTPServer::cleanup_expired_sessions_() {
  std::vector<std::string> expired_keys;

  // Find expired sessions
  for (auto &entry : this->sessions_) {
    if (entry.second->is_expired(TFTP_TIMEOUT_MS * 3)) {  // 3x timeout for session expiry
      expired_keys.push_back(entry.first);
    }
  }

  // Remove expired sessions
  for (const auto &key : expired_keys) {
    ESP_LOGW(TAG, "Session timeout: %s", key.c_str());
    this->sessions_.erase(key);
  }
}

//========================================================================
// File Operations
//========================================================================

bool TFTPServer::validate_filename_(const std::string &filename) {
  // Reject empty filenames
  if (filename.empty()) {
    return false;
  }

  // Reject path traversal attempts
  if (filename.find("..") != std::string::npos) {
    return false;
  }

  // Reject absolute paths (must be relative to root_dir)
  if (!filename.empty() && filename[0] == '/') {
    // Allow if it starts with root_dir
    if (filename.find(this->root_dir_) != 0) {
      return false;
    }
  }

  return true;
}

std::string TFTPServer::get_full_path_(const std::string &filename) {
  std::string path = this->root_dir_;

  // Ensure root_dir ends with /
  if (!path.empty() && path.back() != '/') {
    path += '/';
  }

  // Add filename (remove leading / if present)
  if (!filename.empty() && filename[0] == '/') {
    path += filename.substr(1);
  } else {
    path += filename;
  }

  return path;
}

bool TFTPServer::file_exists_(const std::string &path) {
#ifdef USE_ESP_IDF
  struct stat st;
  return (stat(path.c_str(), &st) == 0) && S_ISREG(st.st_mode);
#else
  // Arduino - simplified check
  // TODO: Implement proper filesystem check for Arduino
  return false;
#endif
}

bool TFTPServer::can_read_(const std::string &path) {
  // Simple check - always allow read if access mode permits
  return (this->access_mode_ == ACCESS_READ_ONLY || this->access_mode_ == ACCESS_READ_WRITE);
}

bool TFTPServer::can_write_(const std::string &path) {
  // Simple check - always allow write if access mode permits
  return (this->access_mode_ == ACCESS_WRITE_ONLY || this->access_mode_ == ACCESS_READ_WRITE);
}

bool TFTPServer::read_file_(const std::string &path, std::vector<uint8_t> &data) {
#ifdef USE_ESP_IDF
  FILE *file = fopen(path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", path.c_str());
    return false;
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Check size limit
  if (file_size > this->max_file_size_) {
    ESP_LOGE(TAG, "File too large: %zu > %zu", file_size, this->max_file_size_);
    fclose(file);
    return false;
  }

  // Read file
  data.resize(file_size);
  size_t read = fread(data.data(), 1, file_size, file);
  fclose(file);

  if (read != file_size) {
    ESP_LOGE(TAG, "Failed to read entire file: %zu / %zu bytes", read, file_size);
    return false;
  }

  return true;
#else
  // Arduino - TODO: Implement filesystem read
  ESP_LOGE(TAG, "File read not implemented for Arduino");
  return false;
#endif
}

bool TFTPServer::write_file_(const std::string &path, const uint8_t *data, size_t length) {
#ifdef USE_ESP_IDF
  FILE *file = fopen(path.c_str(), "wb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", path.c_str());
    return false;
  }

  size_t written = fwrite(data, 1, length, file);
  fclose(file);

  if (written != length) {
    ESP_LOGE(TAG, "Failed to write entire file: %zu / %zu bytes", written, length);
    return false;
  }

  return true;
#else
  // Arduino - TODO: Implement filesystem write
  ESP_LOGE(TAG, "File write not implemented for Arduino");
  return false;
#endif
}

//========================================================================
// Transfer Operations
//========================================================================

void TFTPServer::send_data_block_(TFTPSession *session) {
  if (!session || session->is_write) {
    return;
  }

  // Calculate block offset and size
  size_t offset = (session->current_block - 1) * TFTP_DATA_SIZE;
  size_t remaining = (offset < session->file_data.size()) ? (session->file_data.size() - offset) : 0;
  size_t block_size = std::min(remaining, TFTP_DATA_SIZE);

  // Create DATA packet
  const uint8_t *block_data = (block_size > 0) ? &session->file_data[offset] : nullptr;
  TFTPPacket data_packet = TFTPPacket::create_data(session->current_block, block_data, block_size);

  // Send packet
  this->send_packet_(data_packet, session->client_addr, session->client_port);

  ESP_LOGVV(TAG, "Sent block %u (%zu bytes)", session->current_block, block_size);
}

void TFTPServer::send_error_(const std::string &client_addr, uint16_t client_port, TFTPErrorCode error_code,
                             const std::string &error_msg) {
  TFTPPacket error_packet = TFTPPacket::create_error(error_code, error_msg);
  this->send_packet_(error_packet, client_addr, client_port);
  ESP_LOGW(TAG, "Sent error to %s:%u: %s", client_addr.c_str(), client_port, error_msg.c_str());
}

}  // namespace tftp_server
}  // namespace esphome
