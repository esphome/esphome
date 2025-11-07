#include "nfs_client.h"
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
namespace nfs_client {

//========================================================================
// XDR Buffer Implementation
//========================================================================

void XDRBuffer::encode_uint32(uint32_t value) {
  // Big-endian encoding
  this->data_.push_back((value >> 24) & 0xFF);
  this->data_.push_back((value >> 16) & 0xFF);
  this->data_.push_back((value >> 8) & 0xFF);
  this->data_.push_back(value & 0xFF);
}

void XDRBuffer::encode_uint64(uint64_t value) {
  this->encode_uint32(static_cast<uint32_t>(value >> 32));
  this->encode_uint32(static_cast<uint32_t>(value & 0xFFFFFFFF));
}

void XDRBuffer::encode_bytes(const uint8_t *data, size_t length) {
  this->data_.insert(this->data_.end(), data, data + length);
}

void XDRBuffer::encode_string(const std::string &str) {
  // Length + data + padding
  this->encode_uint32(str.length());
  this->encode_bytes(reinterpret_cast<const uint8_t *>(str.data()), str.length());

  // Add padding to 4-byte boundary
  size_t padding = align_4(str.length()) - str.length();
  for (size_t i = 0; i < padding; i++) {
    this->data_.push_back(0);
  }
}

void XDRBuffer::encode_opaque(const uint8_t *data, size_t length) {
  // Same as string but for binary data
  this->encode_uint32(length);
  this->encode_bytes(data, length);

  // Add padding to 4-byte boundary
  size_t padding = align_4(length) - length;
  for (size_t i = 0; i < padding; i++) {
    this->data_.push_back(0);
  }
}

bool XDRBuffer::decode_uint32(uint32_t &value) {
  if (this->position_ + 4 > this->data_.size()) {
    return false;
  }

  value = (static_cast<uint32_t>(this->data_[this->position_]) << 24) |
          (static_cast<uint32_t>(this->data_[this->position_ + 1]) << 16) |
          (static_cast<uint32_t>(this->data_[this->position_ + 2]) << 8) |
          static_cast<uint32_t>(this->data_[this->position_ + 3]);

  this->position_ += 4;
  return true;
}

bool XDRBuffer::decode_uint64(uint64_t &value) {
  uint32_t high, low;
  if (!this->decode_uint32(high) || !this->decode_uint32(low)) {
    return false;
  }
  value = (static_cast<uint64_t>(high) << 32) | low;
  return true;
}

bool XDRBuffer::decode_bytes(uint8_t *data, size_t length) {
  if (this->position_ + length > this->data_.size()) {
    return false;
  }

  std::copy(this->data_.begin() + this->position_, this->data_.begin() + this->position_ + length, data);
  this->position_ += length;
  return true;
}

bool XDRBuffer::decode_string(std::string &str) {
  uint32_t length;
  if (!this->decode_uint32(length)) {
    return false;
  }

  if (length > NFS_MAXPATHLEN) {
    return false;
  }

  if (this->position_ + length > this->data_.size()) {
    return false;
  }

  str.assign(reinterpret_cast<const char *>(&this->data_[this->position_]), length);
  this->position_ += align_4(length);

  return true;
}

bool XDRBuffer::decode_opaque(std::vector<uint8_t> &data) {
  uint32_t length;
  if (!this->decode_uint32(length)) {
    return false;
  }

  if (length > NFS_FHSIZE3) {
    return false;
  }

  if (this->position_ + length > this->data_.size()) {
    return false;
  }

  data.assign(this->data_.begin() + this->position_, this->data_.begin() + this->position_ + length);
  this->position_ += align_4(length);

  return true;
}

bool XDRBuffer::decode_bool(bool &value) {
  uint32_t val;
  if (!this->decode_uint32(val)) {
    return false;
  }
  value = (val != 0);
  return true;
}

//========================================================================
// NFS Structures Implementation
//========================================================================

void NFSFileHandle::encode(XDRBuffer &xdr) const {
  xdr.encode_opaque(this->data.data(), this->data.size());
}

bool NFSFileHandle::decode(XDRBuffer &xdr) {
  return xdr.decode_opaque(this->data);
}

bool NFSFileAttr::decode(XDRBuffer &xdr) {
  uint32_t type_val;
  if (!xdr.decode_uint32(type_val))
    return false;
  this->type = static_cast<NFSFileType>(type_val);

  return xdr.decode_uint32(this->mode) && xdr.decode_uint32(this->nlink) && xdr.decode_uint32(this->uid) &&
         xdr.decode_uint32(this->gid) && xdr.decode_uint64(this->size) && xdr.decode_uint64(this->used) &&
         xdr.decode_uint64(this->fsid) && xdr.decode_uint64(this->fileid) && xdr.decode_uint64(this->atime_sec) &&
         xdr.decode_uint32(this->atime_nsec) && xdr.decode_uint64(this->mtime_sec) &&
         xdr.decode_uint32(this->mtime_nsec) && xdr.decode_uint64(this->ctime_sec) &&
         xdr.decode_uint32(this->ctime_nsec);
}

//========================================================================
// RPC Client Implementation
//========================================================================

void RPCClient::build_call(XDRBuffer &xdr, uint32_t xid, uint32_t program, uint32_t version, uint32_t procedure,
                            uint32_t uid, uint32_t gid) {
  // RPC call header
  xdr.encode_uint32(xid);              // XID
  xdr.encode_uint32(RPC_CALL);         // Message type
  xdr.encode_uint32(2);                // RPC version
  xdr.encode_uint32(program);          // Program
  xdr.encode_uint32(version);          // Version
  xdr.encode_uint32(procedure);        // Procedure

  // Authentication
  if (uid != 0 || gid != 0) {
    this->encode_auth_unix_(xdr, uid, gid);
  } else {
    this->encode_auth_null_(xdr);
  }

  // Verifier (NULL)
  this->encode_auth_null_(xdr);
}

bool RPCClient::parse_reply(XDRBuffer &xdr, uint32_t expected_xid, RPCAcceptStatus &status) {
  uint32_t xid, msg_type, reply_status;

  if (!xdr.decode_uint32(xid) || xid != expected_xid) {
    ESP_LOGE(TAG, "XID mismatch: expected %u, got %u", expected_xid, xid);
    return false;
  }

  if (!xdr.decode_uint32(msg_type) || msg_type != RPC_REPLY) {
    ESP_LOGE(TAG, "Not an RPC reply");
    return false;
  }

  if (!xdr.decode_uint32(reply_status)) {
    return false;
  }

  if (reply_status != RPC_MSG_ACCEPTED) {
    ESP_LOGE(TAG, "RPC message denied");
    return false;
  }

  // Skip verifier (flavor + length + data)
  uint32_t flavor, length;
  if (!xdr.decode_uint32(flavor) || !xdr.decode_uint32(length)) {
    return false;
  }
  xdr.position() + XDRBuffer::align_4(length);  // Skip verifier data

  // Accept status
  uint32_t accept_status;
  if (!xdr.decode_uint32(accept_status)) {
    return false;
  }

  status = static_cast<RPCAcceptStatus>(accept_status);
  return (status == RPC_SUCCESS);
}

void RPCClient::encode_auth_unix_(XDRBuffer &xdr, uint32_t uid, uint32_t gid) {
  // AUTH_UNIX credential
  xdr.encode_uint32(RPC_AUTH_UNIX);  // Flavor

  // Build AUTH_UNIX structure
  XDRBuffer auth_data;
  auth_data.encode_uint32(static_cast<uint32_t>(millis() / 1000));  // Timestamp
  auth_data.encode_string("esphome");                                // Machine name
  auth_data.encode_uint32(uid);                                      // UID
  auth_data.encode_uint32(gid);                                      // GID
  auth_data.encode_uint32(0);                                        // No auxiliary GIDs

  // Encode AUTH_UNIX data as opaque
  xdr.encode_opaque(auth_data.data().data(), auth_data.size());
}

void RPCClient::encode_auth_null_(XDRBuffer &xdr) {
  xdr.encode_uint32(RPC_AUTH_NULL);  // Flavor
  xdr.encode_uint32(0);              // Length
}

//========================================================================
// NFSClient Implementation
//========================================================================

NFSClient::~NFSClient() { this->disconnect_(); }

void NFSClient::setup() {
  ESP_LOGCONFIG(TAG, "Setting up NFS Client...");
  ESP_LOGCONFIG(TAG, "  Server: %s:%u", this->server_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Export: %s", this->export_path_.c_str());
  ESP_LOGCONFIG(TAG, "  UID: %u, GID: %u", this->uid_, this->gid_);

  if (!this->mount_path_.empty()) {
    ESP_LOGCONFIG(TAG, "  Mount path: %s", this->mount_path_.c_str());
  }

  // Try to mount
  if (this->mount()) {
    ESP_LOGI(TAG, "Successfully mounted NFS export");

    // Register with storage_host if configured
    if (!this->mount_path_.empty()) {
      this->register_with_storage_host();
    }
  } else {
    ESP_LOGW(TAG, "Failed to mount NFS export (will retry in loop)");
  }
}

void NFSClient::loop() {
  // Try to mount if not mounted
  if (!this->mounted_) {
    static uint32_t last_mount_attempt = 0;
    uint32_t now = millis();
    if (now - last_mount_attempt > 30000) {  // Try every 30 seconds
      last_mount_attempt = now;
      if (this->mount()) {
        ESP_LOGI(TAG, "Successfully mounted NFS export");
      }
    }
  }
}

void NFSClient::dump_config() {
  ESP_LOGCONFIG(TAG, "NFS Client:");
  ESP_LOGCONFIG(TAG, "  Server: %s:%u", this->server_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Export: %s", this->export_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Status: %s", this->mounted_ ? "Mounted" : "Not mounted");
  if (!this->mount_path_.empty()) {
    ESP_LOGCONFIG(TAG, "  Mount path: %s", this->mount_path_.c_str());
  }
}

void NFSClient::register_with_storage_host() {
#if defined(USE_STORAGE_HOST)
  if (storage_host::global_storage_host != nullptr) {
    storage_host::global_storage_host->register_network_storage(this);
    ESP_LOGI(TAG, "Registered NFS network storage with storage_host: %s", this->mount_path_.c_str());
  } else {
    ESP_LOGD(TAG, "storage_host not available, skipping network storage registration");
  }
#else
  ESP_LOGD(TAG, "storage_host component not compiled, network storage registration disabled");
#endif  // USE_STORAGE_HOST
}

//========================================================================
// Connection Management
//========================================================================

bool NFSClient::connect_() {
  if (this->connected_) {
    return true;
  }

  ESP_LOGD(TAG, "Connecting to NFS server %s:%u...", this->server_.c_str(), this->port_);

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

  struct hostent *host = gethostbyname(this->server_.c_str());
  if (host == nullptr) {
    ESP_LOGE(TAG, "Failed to resolve host: %s", this->server_.c_str());
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

  // Set TCP_NODELAY
  int nodelay = 1;
  setsockopt(this->socket_, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

  this->connected_ = true;
  return true;
#else
  // Arduino WiFiClient
  this->client_ = std::make_unique<WiFiClient>();
  if (!this->client_->connect(this->server_.c_str(), this->port_)) {
    ESP_LOGE(TAG, "Failed to connect");
    this->client_ = nullptr;
    return false;
  }

  this->client_->setNoDelay(true);
  this->connected_ = true;
  return true;
#endif
}

void NFSClient::disconnect_() {
  if (this->mounted_) {
    this->unmount();
  }

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

bool NFSClient::send_rpc_(const XDRBuffer &request, XDRBuffer &response) {
  if (!this->connected_ && !this->connect_()) {
    return false;
  }

  // RPC over TCP uses record marking (RFC 1831 section 11)
  // Format: [4-byte length with high bit set for last fragment] [data]
  uint32_t length = request.size() | 0x80000000;  // Set high bit for last fragment

#ifdef USE_ESP_IDF
  // Send length
  uint8_t length_buf[4];
  length_buf[0] = (length >> 24) & 0xFF;
  length_buf[1] = (length >> 16) & 0xFF;
  length_buf[2] = (length >> 8) & 0xFF;
  length_buf[3] = length & 0xFF;

  if (send(this->socket_, length_buf, 4, 0) != 4) {
    ESP_LOGE(TAG, "Failed to send RPC length");
    return false;
  }

  // Send data
  if (send(this->socket_, request.data().data(), request.size(), 0) != static_cast<int>(request.size())) {
    ESP_LOGE(TAG, "Failed to send RPC data");
    return false;
  }

  // Receive response length
  uint8_t response_length_buf[4];
  if (recv(this->socket_, response_length_buf, 4, MSG_WAITALL) != 4) {
    ESP_LOGE(TAG, "Failed to receive RPC response length");
    return false;
  }

  uint32_t response_length = (static_cast<uint32_t>(response_length_buf[0]) << 24) |
                             (static_cast<uint32_t>(response_length_buf[1]) << 16) |
                             (static_cast<uint32_t>(response_length_buf[2]) << 8) |
                             static_cast<uint32_t>(response_length_buf[3]);

  response_length &= 0x7FFFFFFF;  // Clear high bit

  if (response_length > 65536) {  // Sanity check
    ESP_LOGE(TAG, "Response too large: %u bytes", response_length);
    return false;
  }

  // Receive response data
  std::vector<uint8_t> response_data(response_length);
  if (recv(this->socket_, response_data.data(), response_length, MSG_WAITALL) !=
      static_cast<int>(response_length)) {
    ESP_LOGE(TAG, "Failed to receive RPC response data");
    return false;
  }

  response = XDRBuffer(response_data);
  return true;
#else
  // Arduino WiFiClient
  // Send length
  uint8_t length_buf[4];
  length_buf[0] = (length >> 24) & 0xFF;
  length_buf[1] = (length >> 16) & 0xFF;
  length_buf[2] = (length >> 8) & 0xFF;
  length_buf[3] = length & 0xFF;

  this->client_->write(length_buf, 4);
  this->client_->write(request.data().data(), request.size());

  // Wait for response
  uint32_t start = millis();
  while (this->client_->available() < 4 && millis() - start < 5000) {
    delay(10);
  }

  if (this->client_->available() < 4) {
    ESP_LOGE(TAG, "Timeout waiting for response");
    return false;
  }

  // Read response length
  uint8_t response_length_buf[4];
  this->client_->read(response_length_buf, 4);

  uint32_t response_length = (static_cast<uint32_t>(response_length_buf[0]) << 24) |
                             (static_cast<uint32_t>(response_length_buf[1]) << 16) |
                             (static_cast<uint32_t>(response_length_buf[2]) << 8) |
                             static_cast<uint32_t>(response_length_buf[3]);

  response_length &= 0x7FFFFFFF;

  // Wait for full response
  start = millis();
  while (this->client_->available() < static_cast<int>(response_length) && millis() - start < 5000) {
    delay(10);
  }

  if (this->client_->available() < static_cast<int>(response_length)) {
    ESP_LOGE(TAG, "Timeout waiting for full response");
    return false;
  }

  // Read response data
  std::vector<uint8_t> response_data(response_length);
  this->client_->read(response_data.data(), response_length);

  response = XDRBuffer(response_data);
  return true;
#endif
}

//========================================================================
// MOUNT Protocol Implementation
//========================================================================

bool NFSClient::mount_export_(const std::string &export_path, NFSFileHandle &fh) {
  ESP_LOGD(TAG, "Mounting export: %s", export_path.c_str());

  // Build MOUNT MNT call
  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, MOUNT_PROGRAM, MOUNT_VERSION_3, MOUNTPROC3_MNT, this->uid_, this->gid_);

  // MOUNT MNT arguments: dirpath
  request.encode_string(export_path);

  // Send RPC call
  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    ESP_LOGE(TAG, "Failed to send MOUNT MNT");
    return false;
  }

  // Parse RPC reply
  RPCAcceptStatus status;
  if (!this->rpc_.parse_reply(response, xid, status)) {
    ESP_LOGE(TAG, "Failed to parse MOUNT reply");
    return false;
  }

  // Parse MOUNT reply: status + file handle
  uint32_t mount_status;
  if (!response.decode_uint32(mount_status)) {
    ESP_LOGE(TAG, "Failed to decode MOUNT status");
    return false;
  }

  if (mount_status != 0) {
    ESP_LOGE(TAG, "MOUNT failed with status: %u", mount_status);
    return false;
  }

  // Decode file handle
  if (!fh.decode(response)) {
    ESP_LOGE(TAG, "Failed to decode file handle");
    return false;
  }

  ESP_LOGD(TAG, "MOUNT successful, file handle size: %zu", fh.data.size());
  return true;
}

bool NFSClient::unmount_export_(const std::string &export_path) {
  ESP_LOGD(TAG, "Unmounting export: %s", export_path.c_str());

  // Build MOUNT UMNT call
  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, MOUNT_PROGRAM, MOUNT_VERSION_3, MOUNTPROC3_UMNT, this->uid_, this->gid_);

  // MOUNT UMNT arguments: dirpath
  request.encode_string(export_path);

  // Send RPC call
  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    ESP_LOGW(TAG, "Failed to send MOUNT UMNT");
    return false;
  }

  // Parse RPC reply
  RPCAcceptStatus status;
  if (!this->rpc_.parse_reply(response, xid, status)) {
    ESP_LOGW(TAG, "Failed to parse UMNT reply");
    return false;
  }

  return true;
}

//========================================================================
// NFS Mount/Unmount
//========================================================================

bool NFSClient::mount() {
  if (this->mounted_) {
    return true;
  }

  if (!this->connect_()) {
    return false;
  }

  // Mount the export
  if (!this->mount_export_(this->export_path_, this->root_fh_)) {
    return false;
  }

  this->mounted_ = true;
  ESP_LOGI(TAG, "NFS mount successful: %s", this->export_path_.c_str());
  return true;
}

void NFSClient::unmount() {
  if (!this->mounted_) {
    return;
  }

  this->unmount_export_(this->export_path_);
  this->root_fh_.data.clear();
  this->mounted_ = false;

  ESP_LOGI(TAG, "NFS unmounted");
}

//========================================================================
// Path Resolution
//========================================================================

std::vector<std::string> NFSClient::split_path_(const std::string &path) {
  std::vector<std::string> components;
  std::string current;

  for (char c : path) {
    if (c == '/') {
      if (!current.empty()) {
        components.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }

  if (!current.empty()) {
    components.push_back(current);
  }

  return components;
}

bool NFSClient::resolve_path_(const std::string &path, NFSFileHandle &fh, NFSFileAttr &attr) {
  if (!this->mounted_) {
    return false;
  }

  // Start from root
  fh = this->root_fh_;

  // Empty path = root
  if (path.empty() || path == "/") {
    return this->nfs_getattr_(fh, attr);
  }

  // Split path into components
  std::vector<std::string> components = this->split_path_(path);

  // Resolve each component
  for (const auto &component : components) {
    NFSFileHandle next_fh;
    if (!this->nfs_lookup_(fh, component, next_fh, attr)) {
      return false;
    }
    fh = next_fh;
  }

  return true;
}

bool NFSClient::resolve_parent_path_(const std::string &path, NFSFileHandle &parent_fh, std::string &filename) {
  if (!this->mounted_) {
    return false;
  }

  // Split path
  std::vector<std::string> components = this->split_path_(path);
  if (components.empty()) {
    return false;
  }

  // Last component is the filename
  filename = components.back();
  components.pop_back();

  // Resolve parent directory
  parent_fh = this->root_fh_;
  NFSFileAttr attr;

  for (const auto &component : components) {
    NFSFileHandle next_fh;
    if (!this->nfs_lookup_(parent_fh, component, next_fh, attr)) {
      return false;
    }
    parent_fh = next_fh;
  }

  return true;
}

//========================================================================
// NFS Protocol Operations
//========================================================================

bool NFSClient::nfs_lookup_(const NFSFileHandle &dir_fh, const std::string &name, NFSFileHandle &fh,
                            NFSFileAttr &attr) {
  ESP_LOGVV(TAG, "NFS LOOKUP: %s", name.c_str());

  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, NFS_PROGRAM, NFS_VERSION_3, NFSPROC3_LOOKUP, this->uid_, this->gid_);

  // LOOKUP arguments: dir file handle + name
  dir_fh.encode(request);
  request.encode_string(name);

  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    return false;
  }

  RPCAcceptStatus rpc_status;
  if (!this->rpc_.parse_reply(response, xid, rpc_status)) {
    return false;
  }

  // Parse NFS status
  uint32_t nfs_status;
  if (!response.decode_uint32(nfs_status) || nfs_status != NFS3_OK) {
    ESP_LOGW(TAG, "LOOKUP failed: status=%u", nfs_status);
    return false;
  }

  // Decode file handle
  if (!fh.decode(response)) {
    return false;
  }

  // Decode attributes (optional)
  bool has_attr;
  if (response.decode_bool(has_attr) && has_attr) {
    attr.decode(response);
  }

  return true;
}

bool NFSClient::nfs_getattr_(const NFSFileHandle &fh, NFSFileAttr &attr) {
  ESP_LOGVV(TAG, "NFS GETATTR");

  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, NFS_PROGRAM, NFS_VERSION_3, NFSPROC3_GETATTR, this->uid_, this->gid_);

  // GETATTR arguments: file handle
  fh.encode(request);

  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    return false;
  }

  RPCAcceptStatus rpc_status;
  if (!this->rpc_.parse_reply(response, xid, rpc_status)) {
    return false;
  }

  // Parse NFS status
  uint32_t nfs_status;
  if (!response.decode_uint32(nfs_status) || nfs_status != NFS3_OK) {
    ESP_LOGW(TAG, "GETATTR failed: status=%u", nfs_status);
    return false;
  }

  // Decode attributes
  return attr.decode(response);
}

bool NFSClient::nfs_read_(const NFSFileHandle &fh, uint64_t offset, uint32_t count, std::vector<uint8_t> &data) {
  ESP_LOGVV(TAG, "NFS READ: offset=%llu, count=%u", offset, count);

  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, NFS_PROGRAM, NFS_VERSION_3, NFSPROC3_READ, this->uid_, this->gid_);

  // READ arguments: file handle + offset + count
  fh.encode(request);
  request.encode_uint64(offset);
  request.encode_uint32(count);

  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    return false;
  }

  RPCAcceptStatus rpc_status;
  if (!this->rpc_.parse_reply(response, xid, rpc_status)) {
    return false;
  }

  // Parse NFS status
  uint32_t nfs_status;
  if (!response.decode_uint32(nfs_status) || nfs_status != NFS3_OK) {
    ESP_LOGW(TAG, "READ failed: status=%u", nfs_status);
    return false;
  }

  // Skip post-op attributes
  bool has_attr;
  NFSFileAttr attr;
  if (response.decode_bool(has_attr) && has_attr) {
    attr.decode(response);
  }

  // Decode count + EOF flag
  uint32_t bytes_read;
  bool eof;
  if (!response.decode_uint32(bytes_read) || !response.decode_bool(eof)) {
    return false;
  }

  // Decode data
  if (!response.decode_opaque(data)) {
    return false;
  }

  ESP_LOGVV(TAG, "NFS READ: read %u bytes, EOF=%d", bytes_read, eof);
  return true;
}

bool NFSClient::nfs_write_(const NFSFileHandle &fh, uint64_t offset, const uint8_t *data, size_t length) {
  ESP_LOGVV(TAG, "NFS WRITE: offset=%llu, length=%zu", offset, length);

  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, NFS_PROGRAM, NFS_VERSION_3, NFSPROC3_WRITE, this->uid_, this->gid_);

  // WRITE arguments: file handle + offset + count + stable + data
  fh.encode(request);
  request.encode_uint64(offset);
  request.encode_uint32(length);
  request.encode_uint32(2);  // FILE_SYNC (write immediately to stable storage)
  request.encode_opaque(data, length);

  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    return false;
  }

  RPCAcceptStatus rpc_status;
  if (!this->rpc_.parse_reply(response, xid, rpc_status)) {
    return false;
  }

  // Parse NFS status
  uint32_t nfs_status;
  if (!response.decode_uint32(nfs_status) || nfs_status != NFS3_OK) {
    ESP_LOGW(TAG, "WRITE failed: status=%u", nfs_status);
    return false;
  }

  // Skip pre-op and post-op attributes
  bool has_wcc;
  if (response.decode_bool(has_wcc) && has_wcc) {
    // Skip WCC data
  }

  // Decode bytes written
  uint32_t bytes_written;
  if (!response.decode_uint32(bytes_written)) {
    return false;
  }

  ESP_LOGVV(TAG, "NFS WRITE: wrote %u bytes", bytes_written);
  return (bytes_written == length);
}

bool NFSClient::nfs_create_(const NFSFileHandle &dir_fh, const std::string &name, uint32_t mode, NFSFileHandle &fh) {
  ESP_LOGD(TAG, "NFS CREATE: %s (mode 0%o)", name.c_str(), mode);

  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, NFS_PROGRAM, NFS_VERSION_3, NFSPROC3_CREATE, this->uid_, this->gid_);

  // CREATE arguments: dir fh + name + how + attributes
  dir_fh.encode(request);
  request.encode_string(name);
  request.encode_uint32(0);  // UNCHECKED

  // Set attributes
  request.encode_bool(true);   // mode
  request.encode_uint32(mode);
  request.encode_bool(true);   // uid
  request.encode_uint32(this->uid_);
  request.encode_bool(true);   // gid
  request.encode_uint32(this->gid_);
  request.encode_bool(false);  // size
  request.encode_uint32(0);    // atime: don't set
  request.encode_uint32(0);    // mtime: don't set

  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    return false;
  }

  RPCAcceptStatus rpc_status;
  if (!this->rpc_.parse_reply(response, xid, rpc_status)) {
    return false;
  }

  // Parse NFS status
  uint32_t nfs_status;
  if (!response.decode_uint32(nfs_status) || nfs_status != NFS3_OK) {
    ESP_LOGW(TAG, "CREATE failed: status=%u", nfs_status);
    return false;
  }

  // Decode file handle
  bool has_fh;
  if (!response.decode_bool(has_fh) || !has_fh) {
    return false;
  }

  return fh.decode(response);
}

bool NFSClient::nfs_remove_(const NFSFileHandle &dir_fh, const std::string &name) {
  ESP_LOGD(TAG, "NFS REMOVE: %s", name.c_str());

  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, NFS_PROGRAM, NFS_VERSION_3, NFSPROC3_REMOVE, this->uid_, this->gid_);

  // REMOVE arguments: dir fh + name
  dir_fh.encode(request);
  request.encode_string(name);

  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    return false;
  }

  RPCAcceptStatus rpc_status;
  if (!this->rpc_.parse_reply(response, xid, rpc_status)) {
    return false;
  }

  // Parse NFS status
  uint32_t nfs_status;
  if (!response.decode_uint32(nfs_status) || nfs_status != NFS3_OK) {
    ESP_LOGW(TAG, "REMOVE failed: status=%u", nfs_status);
    return false;
  }

  return true;
}

bool NFSClient::nfs_mkdir_(const NFSFileHandle &dir_fh, const std::string &name, uint32_t mode, NFSFileHandle &fh) {
  ESP_LOGD(TAG, "NFS MKDIR: %s (mode 0%o)", name.c_str(), mode);

  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, NFS_PROGRAM, NFS_VERSION_3, NFSPROC3_MKDIR, this->uid_, this->gid_);

  // MKDIR arguments: dir fh + name + attributes
  dir_fh.encode(request);
  request.encode_string(name);

  // Set attributes
  request.encode_bool(true);   // mode
  request.encode_uint32(mode | 0040000);  // Add directory bit
  request.encode_bool(true);   // uid
  request.encode_uint32(this->uid_);
  request.encode_bool(true);   // gid
  request.encode_uint32(this->gid_);
  request.encode_bool(false);  // size
  request.encode_uint32(0);    // atime
  request.encode_uint32(0);    // mtime

  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    return false;
  }

  RPCAcceptStatus rpc_status;
  if (!this->rpc_.parse_reply(response, xid, rpc_status)) {
    return false;
  }

  // Parse NFS status
  uint32_t nfs_status;
  if (!response.decode_uint32(nfs_status) || nfs_status != NFS3_OK) {
    ESP_LOGW(TAG, "MKDIR failed: status=%u", nfs_status);
    return false;
  }

  // Decode file handle
  bool has_fh;
  if (!response.decode_bool(has_fh) || !has_fh) {
    return false;
  }

  return fh.decode(response);
}

bool NFSClient::nfs_rmdir_(const NFSFileHandle &dir_fh, const std::string &name) {
  ESP_LOGD(TAG, "NFS RMDIR: %s", name.c_str());

  uint32_t xid = RPCClient::generate_xid();
  XDRBuffer request;
  this->rpc_.build_call(request, xid, NFS_PROGRAM, NFS_VERSION_3, NFSPROC3_RMDIR, this->uid_, this->gid_);

  // RMDIR arguments: dir fh + name
  dir_fh.encode(request);
  request.encode_string(name);

  XDRBuffer response;
  if (!this->send_rpc_(request, response)) {
    return false;
  }

  RPCAcceptStatus rpc_status;
  if (!this->rpc_.parse_reply(response, xid, rpc_status)) {
    return false;
  }

  // Parse NFS status
  uint32_t nfs_status;
  if (!response.decode_uint32(nfs_status) || nfs_status != NFS3_OK) {
    ESP_LOGW(TAG, "RMDIR failed: status=%u", nfs_status);
    return false;
  }

  return true;
}

bool NFSClient::nfs_readdir_(const NFSFileHandle &dir_fh, std::vector<NFSDirEntry> &entries) {
  ESP_LOGVV(TAG, "NFS READDIR");

  entries.clear();
  uint64_t cookie = 0;
  std::vector<uint8_t> cookieverf(8, 0);

  // READDIR may need multiple calls to get all entries
  while (true) {
    uint32_t xid = RPCClient::generate_xid();
    XDRBuffer request;
    this->rpc_.build_call(request, xid, NFS_PROGRAM, NFS_VERSION_3, NFSPROC3_READDIR, this->uid_, this->gid_);

    // READDIR arguments: dir fh + cookie + cookieverf + count
    dir_fh.encode(request);
    request.encode_uint64(cookie);
    request.encode_opaque(cookieverf.data(), cookieverf.size());
    request.encode_uint32(8192);  // Max bytes to return

    XDRBuffer response;
    if (!this->send_rpc_(request, response)) {
      return false;
    }

    RPCAcceptStatus rpc_status;
    if (!this->rpc_.parse_reply(response, xid, rpc_status)) {
      return false;
    }

    // Parse NFS status
    uint32_t nfs_status;
    if (!response.decode_uint32(nfs_status) || nfs_status != NFS3_OK) {
      ESP_LOGW(TAG, "READDIR failed: status=%u", nfs_status);
      return false;
    }

    // Skip post-op attributes
    bool has_attr;
    NFSFileAttr attr;
    if (response.decode_bool(has_attr) && has_attr) {
      attr.decode(response);
    }

    // Decode cookieverf
    response.decode_opaque(cookieverf);

    // Decode entries
    bool has_entry;
    while (response.decode_bool(has_entry) && has_entry) {
      NFSDirEntry entry;
      if (!response.decode_uint64(entry.fileid)) {
        break;
      }
      if (!response.decode_string(entry.name)) {
        break;
      }
      if (!response.decode_uint64(entry.cookie)) {
        break;
      }

      // Skip "." and ".."
      if (entry.name != "." && entry.name != "..") {
        entries.push_back(entry);
      }

      cookie = entry.cookie;
    }

    // Check if EOF
    bool eof;
    if (!response.decode_bool(eof)) {
      return false;
    }

    if (eof) {
      break;
    }
  }

  return true;
}

//========================================================================
// High-Level File Operations
//========================================================================

bool NFSClient::read_file(const std::string &path, std::vector<uint8_t> &data) {
  NFSFileHandle fh;
  NFSFileAttr attr;

  // Resolve path to file handle
  if (!this->resolve_path_(path, fh, attr)) {
    ESP_LOGW(TAG, "Failed to resolve path: %s", path.c_str());
    return false;
  }

  // Check if it's a regular file
  if (attr.type != NF3REG) {
    ESP_LOGW(TAG, "Not a regular file: %s", path.c_str());
    return false;
  }

  // Read file in chunks
  data.clear();
  data.reserve(attr.size);

  uint64_t offset = 0;
  uint32_t chunk_size = 8192;

  while (offset < attr.size) {
    std::vector<uint8_t> chunk;
    if (!this->nfs_read_(fh, offset, chunk_size, chunk)) {
      ESP_LOGE(TAG, "Failed to read file at offset %llu", offset);
      return false;
    }

    if (chunk.empty()) {
      break;  // EOF
    }

    data.insert(data.end(), chunk.begin(), chunk.end());
    offset += chunk.size();
  }

  ESP_LOGI(TAG, "Read file: %s (%zu bytes)", path.c_str(), data.size());
  return true;
}

bool NFSClient::write_file(const std::string &path, const uint8_t *data, size_t length) {
  NFSFileHandle parent_fh;
  std::string filename;

  // Resolve parent directory
  if (!this->resolve_parent_path_(path, parent_fh, filename)) {
    ESP_LOGW(TAG, "Failed to resolve parent path: %s", path.c_str());
    return false;
  }

  // Create file
  NFSFileHandle fh;
  if (!this->nfs_create_(parent_fh, filename, 0644, fh)) {
    ESP_LOGW(TAG, "Failed to create file: %s", path.c_str());
    return false;
  }

  // Write data in chunks
  uint64_t offset = 0;
  uint32_t chunk_size = 8192;

  while (offset < length) {
    uint32_t to_write = std::min(chunk_size, static_cast<uint32_t>(length - offset));
    if (!this->nfs_write_(fh, offset, data + offset, to_write)) {
      ESP_LOGE(TAG, "Failed to write file at offset %llu", offset);
      return false;
    }
    offset += to_write;
  }

  ESP_LOGI(TAG, "Wrote file: %s (%zu bytes)", path.c_str(), length);
  return true;
}

bool NFSClient::delete_file(const std::string &path) {
  NFSFileHandle parent_fh;
  std::string filename;

  if (!this->resolve_parent_path_(path, parent_fh, filename)) {
    return false;
  }

  return this->nfs_remove_(parent_fh, filename);
}

bool NFSClient::file_exists(const std::string &path) {
  NFSFileHandle fh;
  NFSFileAttr attr;
  return this->resolve_path_(path, fh, attr);
}

bool NFSClient::list_directory(const std::string &path, std::vector<NFSDirEntry> &entries) {
  NFSFileHandle fh;
  NFSFileAttr attr;

  if (!this->resolve_path_(path, fh, attr)) {
    return false;
  }

  if (attr.type != NF3DIR) {
    ESP_LOGW(TAG, "Not a directory: %s", path.c_str());
    return false;
  }

  return this->nfs_readdir_(fh, entries);
}

bool NFSClient::create_directory(const std::string &path) {
  NFSFileHandle parent_fh;
  std::string dirname;

  if (!this->resolve_parent_path_(path, parent_fh, dirname)) {
    return false;
  }

  NFSFileHandle fh;
  return this->nfs_mkdir_(parent_fh, dirname, 0755, fh);
}

bool NFSClient::delete_directory(const std::string &path) {
  NFSFileHandle parent_fh;
  std::string dirname;

  if (!this->resolve_parent_path_(path, parent_fh, dirname)) {
    return false;
  }

  return this->nfs_rmdir_(parent_fh, dirname);
}

bool NFSClient::get_file_attributes(const std::string &path, NFSFileAttr &attr) {
  NFSFileHandle fh;
  return this->resolve_path_(path, fh, attr);
}

#if defined(USE_STORAGE_HOST)
// NetworkStorage interface override - converts NFSDirEntry to NetworkStorage::DirEntry
bool NFSClient::list_directory(const std::string &path, std::vector<storage_host::NetworkStorage::DirEntry> &entries) {
  // Call existing NFS-specific list_directory
  std::vector<NFSDirEntry> nfs_entries;
  if (!this->list_directory(path, nfs_entries)) {
    return false;
  }

  // Convert NFSDirEntry to NetworkStorage::DirEntry
  entries.clear();
  entries.reserve(nfs_entries.size());
  for (const auto &nfs_entry : nfs_entries) {
    storage_host::NetworkStorage::DirEntry entry;
    entry.name = nfs_entry.name;
    entry.size = nfs_entry.size;
    entry.is_directory = nfs_entry.is_directory;
    entries.push_back(entry);
  }

  return true;
}
#endif

}  // namespace nfs_client
}  // namespace esphome
