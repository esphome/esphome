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
#include <memory>
#include <map>

namespace esphome {
namespace nfs_client {

static const char *const TAG = "nfs_client";

//========================================================================
// RPC Protocol Constants (RFC 1831)
//========================================================================

/// RPC message types
enum RPCMessageType : uint32_t {
  RPC_CALL = 0,
  RPC_REPLY = 1,
};

/// RPC reply status
enum RPCReplyStatus : uint32_t {
  RPC_MSG_ACCEPTED = 0,
  RPC_MSG_DENIED = 1,
};

/// RPC accept status
enum RPCAcceptStatus : uint32_t {
  RPC_SUCCESS = 0,
  RPC_PROG_UNAVAIL = 1,
  RPC_PROG_MISMATCH = 2,
  RPC_PROC_UNAVAIL = 3,
  RPC_GARBAGE_ARGS = 4,
  RPC_SYSTEM_ERR = 5,
};

/// RPC authentication flavor
enum RPCAuthFlavor : uint32_t {
  RPC_AUTH_NULL = 0,
  RPC_AUTH_UNIX = 1,
  RPC_AUTH_SHORT = 2,
  RPC_AUTH_DES = 3,
};

//========================================================================
// NFS Protocol Constants (RFC 1813)
//========================================================================

/// NFS program number
static constexpr uint32_t NFS_PROGRAM = 100003;
static constexpr uint32_t NFS_VERSION_3 = 3;

/// MOUNT program number
static constexpr uint32_t MOUNT_PROGRAM = 100005;
static constexpr uint32_t MOUNT_VERSION_3 = 3;

/// NFS ports (use portmapper/rpcbind in production)
static constexpr uint16_t NFS_DEFAULT_PORT = 2049;
static constexpr uint16_t MOUNT_DEFAULT_PORT = 2049;

/// NFS v3 procedure numbers
enum NFSv3Procedure : uint32_t {
  NFSPROC3_NULL = 0,
  NFSPROC3_GETATTR = 1,
  NFSPROC3_SETATTR = 2,
  NFSPROC3_LOOKUP = 3,
  NFSPROC3_ACCESS = 4,
  NFSPROC3_READLINK = 5,
  NFSPROC3_READ = 6,
  NFSPROC3_WRITE = 7,
  NFSPROC3_CREATE = 8,
  NFSPROC3_MKDIR = 9,
  NFSPROC3_SYMLINK = 10,
  NFSPROC3_MKNOD = 11,
  NFSPROC3_REMOVE = 12,
  NFSPROC3_RMDIR = 13,
  NFSPROC3_RENAME = 14,
  NFSPROC3_LINK = 15,
  NFSPROC3_READDIR = 16,
  NFSPROC3_READDIRPLUS = 17,
  NFSPROC3_FSSTAT = 18,
  NFSPROC3_FSINFO = 19,
  NFSPROC3_PATHCONF = 20,
  NFSPROC3_COMMIT = 21,
};

/// MOUNT v3 procedure numbers
enum MOUNTv3Procedure : uint32_t {
  MOUNTPROC3_NULL = 0,
  MOUNTPROC3_MNT = 1,
  MOUNTPROC3_DUMP = 2,
  MOUNTPROC3_UMNT = 3,
  MOUNTPROC3_UMNTALL = 4,
  MOUNTPROC3_EXPORT = 5,
};

/// NFS file types
enum NFSFileType : uint32_t {
  NF3REG = 1,   ///< Regular file
  NF3DIR = 2,   ///< Directory
  NF3BLK = 3,   ///< Block device
  NF3CHR = 4,   ///< Character device
  NF3LNK = 5,   ///< Symbolic link
  NF3SOCK = 6,  ///< Socket
  NF3FIFO = 7,  ///< FIFO
};

/// NFS status codes
enum NFSStatus : uint32_t {
  NFS3_OK = 0,
  NFS3ERR_PERM = 1,
  NFS3ERR_NOENT = 2,
  NFS3ERR_IO = 5,
  NFS3ERR_NXIO = 6,
  NFS3ERR_ACCES = 13,
  NFS3ERR_EXIST = 17,
  NFS3ERR_XDEV = 18,
  NFS3ERR_NODEV = 19,
  NFS3ERR_NOTDIR = 20,
  NFS3ERR_ISDIR = 21,
  NFS3ERR_INVAL = 22,
  NFS3ERR_FBIG = 27,
  NFS3ERR_NOSPC = 28,
  NFS3ERR_ROFS = 30,
  NFS3ERR_MLINK = 31,
  NFS3ERR_NAMETOOLONG = 63,
  NFS3ERR_NOTEMPTY = 66,
  NFS3ERR_DQUOT = 69,
  NFS3ERR_STALE = 70,
  NFS3ERR_REMOTE = 71,
  NFS3ERR_BADHANDLE = 10001,
  NFS3ERR_NOT_SYNC = 10002,
  NFS3ERR_BAD_COOKIE = 10003,
  NFS3ERR_NOTSUPP = 10004,
  NFS3ERR_TOOSMALL = 10005,
  NFS3ERR_SERVERFAULT = 10006,
  NFS3ERR_BADTYPE = 10007,
  NFS3ERR_JUKEBOX = 10008,
};

/// Maximum file handle size
static constexpr size_t NFS_FHSIZE3 = 64;

/// Maximum path component size
static constexpr size_t NFS_MAXNAMLEN = 255;

/// Maximum path size
static constexpr size_t NFS_MAXPATHLEN = 1024;

//========================================================================
// XDR Buffer (RFC 1832)
//========================================================================

/**
 * @brief XDR (External Data Representation) buffer for encoding/decoding
 *
 * Handles XDR encoding/decoding for RPC and NFS protocols.
 * All data is big-endian and 4-byte aligned.
 */
class XDRBuffer {
 public:
  XDRBuffer() = default;
  explicit XDRBuffer(size_t capacity) { this->data_.reserve(capacity); }
  explicit XDRBuffer(const std::vector<uint8_t> &data) : data_(data), position_(0) {}

  // Encoding (writing)
  void encode_uint32(uint32_t value);
  void encode_uint64(uint64_t value);
  void encode_bytes(const uint8_t *data, size_t length);
  void encode_string(const std::string &str);
  void encode_opaque(const uint8_t *data, size_t length);
  void encode_bool(bool value) { this->encode_uint32(value ? 1 : 0); }

  // Decoding (reading)
  bool decode_uint32(uint32_t &value);
  bool decode_uint64(uint64_t &value);
  bool decode_bytes(uint8_t *data, size_t length);
  bool decode_string(std::string &str);
  bool decode_opaque(std::vector<uint8_t> &data);
  bool decode_bool(bool &value);

  // Buffer management
  const std::vector<uint8_t> &data() const { return this->data_; }
  size_t size() const { return this->data_.size(); }
  size_t position() const { return this->position_; }
  void reset() { this->position_ = 0; }
  void clear() {
    this->data_.clear();
    this->position_ = 0;
  }

 protected:
  std::vector<uint8_t> data_;
  size_t position_{0};

  // XDR alignment helper
  static size_t align_4(size_t size) { return (size + 3) & ~3; }
};

//========================================================================
// NFS Structures
//========================================================================

/**
 * @brief NFS file handle
 */
struct NFSFileHandle {
  std::vector<uint8_t> data;

  NFSFileHandle() = default;
  explicit NFSFileHandle(const std::vector<uint8_t> &fh_data) : data(fh_data) {}

  bool is_valid() const { return !this->data.empty() && this->data.size() <= NFS_FHSIZE3; }

  void encode(XDRBuffer &xdr) const;
  bool decode(XDRBuffer &xdr);
};

/**
 * @brief NFS file attributes
 */
struct NFSFileAttr {
  NFSFileType type;
  uint32_t mode;
  uint32_t nlink;
  uint32_t uid;
  uint32_t gid;
  uint64_t size;
  uint64_t used;
  uint64_t fsid;
  uint64_t fileid;
  uint64_t atime_sec;
  uint32_t atime_nsec;
  uint64_t mtime_sec;
  uint32_t mtime_nsec;
  uint64_t ctime_sec;
  uint32_t ctime_nsec;

  NFSFileAttr() : type(NF3REG), mode(0), nlink(0), uid(0), gid(0), size(0), used(0), fsid(0), fileid(0),
                  atime_sec(0), atime_nsec(0), mtime_sec(0), mtime_nsec(0), ctime_sec(0), ctime_nsec(0) {}

  bool decode(XDRBuffer &xdr);
};

/**
 * @brief NFS directory entry
 */
struct NFSDirEntry {
  uint64_t fileid;
  std::string name;
  uint64_t cookie;
  NFSFileAttr attr;
  bool has_attr;

  NFSDirEntry() : fileid(0), cookie(0), has_attr(false) {}
};

//========================================================================
// RPC Layer
//========================================================================

/**
 * @brief RPC call/reply handler
 */
class RPCClient {
 public:
  RPCClient() = default;

  // Build RPC call
  void build_call(XDRBuffer &xdr, uint32_t xid, uint32_t program, uint32_t version, uint32_t procedure,
                  uint32_t uid = 0, uint32_t gid = 0);

  // Parse RPC reply
  bool parse_reply(XDRBuffer &xdr, uint32_t expected_xid, RPCAcceptStatus &status);

  // Generate XID
  static uint32_t generate_xid() { return millis(); }

 protected:
  void encode_auth_unix_(XDRBuffer &xdr, uint32_t uid, uint32_t gid);
  void encode_auth_null_(XDRBuffer &xdr);
};

//========================================================================
// NFS Client Component
//========================================================================

/**
 * @brief NFS v3 Client for mounting and accessing NFS shares
 *
 * Implements NFS v3 protocol (RFC 1813) with MOUNT protocol support.
 * Provides read and write access to remote NFS shares.
 *
 * Features:
 * - NFS v3 protocol (RFC 1813)
 * - MOUNT protocol for getting root file handle
 * - RPC/XDR encoding/decoding (RFC 1831/1832)
 * - AUTH_UNIX authentication
 * - File operations: read, write, create, delete
 * - Directory operations: list, create, delete
 * - Storage host integration
 *
 * Example configuration:
 * @code
 * nfs_client:
 *   - id: my_nfs
 *     server: 192.168.1.100
 *     export: /volume1/data
 *     mount_path: /nfs/nas  # For storage_host
 *     uid: 1000  # Optional, default 0
 *     gid: 1000  # Optional, default 0
 * @endcode
 */
class NFSClient : public Component {
 public:
  NFSClient() = default;
  ~NFSClient() override;

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
  void set_export(const std::string &export_path) { this->export_path_ = export_path; }
  void set_mount_path(const std::string &mount_path) { this->mount_path_ = mount_path; }
  void set_uid(uint32_t uid) { this->uid_ = uid; }
  void set_gid(uint32_t gid) { this->gid_ = gid; }

  const std::string &get_mount_path() const { return this->mount_path_; }

  //========================================================================
  // Storage Host Integration (soft dependency)
  //========================================================================

  void register_with_storage_host();

  //========================================================================
  // NFS Operations
  //========================================================================

  bool is_mounted() const { return this->mounted_; }
  bool mount();
  void unmount();

  // File operations
  bool read_file(const std::string &path, std::vector<uint8_t> &data);
  bool write_file(const std::string &path, const uint8_t *data, size_t length);
  bool delete_file(const std::string &path);
  bool file_exists(const std::string &path);

  // Directory operations
  bool list_directory(const std::string &path, std::vector<NFSDirEntry> &entries);
  bool create_directory(const std::string &path);
  bool delete_directory(const std::string &path);

  // File info
  bool get_file_attributes(const std::string &path, NFSFileAttr &attr);

 protected:
  //========================================================================
  // Configuration
  //========================================================================

  std::string server_;
  uint16_t port_{NFS_DEFAULT_PORT};
  std::string export_path_;
  std::string mount_path_;
  uint32_t uid_{0};
  uint32_t gid_{0};

  //========================================================================
  // Connection State
  //========================================================================

#ifdef USE_ESP_IDF
  int socket_{-1};
#else
  std::unique_ptr<WiFiClient> client_;
#endif

  bool connected_{false};
  bool mounted_{false};
  NFSFileHandle root_fh_;

  //========================================================================
  // RPC Client
  //========================================================================

  RPCClient rpc_;

  //========================================================================
  // Internal Operations
  //========================================================================

  bool connect_();
  void disconnect_();
  bool send_rpc_(const XDRBuffer &request, XDRBuffer &response);

  //========================================================================
  // MOUNT Protocol
  //========================================================================

  bool mount_export_(const std::string &export_path, NFSFileHandle &fh);
  bool unmount_export_(const std::string &export_path);

  //========================================================================
  // NFS Protocol Operations
  //========================================================================

  bool nfs_lookup_(const NFSFileHandle &dir_fh, const std::string &name, NFSFileHandle &fh, NFSFileAttr &attr);
  bool nfs_getattr_(const NFSFileHandle &fh, NFSFileAttr &attr);
  bool nfs_read_(const NFSFileHandle &fh, uint64_t offset, uint32_t count, std::vector<uint8_t> &data);
  bool nfs_write_(const NFSFileHandle &fh, uint64_t offset, const uint8_t *data, size_t length);
  bool nfs_create_(const NFSFileHandle &dir_fh, const std::string &name, uint32_t mode, NFSFileHandle &fh);
  bool nfs_remove_(const NFSFileHandle &dir_fh, const std::string &name);
  bool nfs_mkdir_(const NFSFileHandle &dir_fh, const std::string &name, uint32_t mode, NFSFileHandle &fh);
  bool nfs_rmdir_(const NFSFileHandle &dir_fh, const std::string &name);
  bool nfs_readdir_(const NFSFileHandle &dir_fh, std::vector<NFSDirEntry> &entries);

  //========================================================================
  // Path Resolution
  //========================================================================

  bool resolve_path_(const std::string &path, NFSFileHandle &fh, NFSFileAttr &attr);
  bool resolve_parent_path_(const std::string &path, NFSFileHandle &parent_fh, std::string &filename);
  std::vector<std::string> split_path_(const std::string &path);
};

}  // namespace nfs_client
}  // namespace esphome
