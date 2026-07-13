#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/components/storage/storage.h"

#if defined(USE_ESP_IDF) || defined(USE_ESP32)
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#elif defined(USE_ESP8266) || defined(USE_LIBRETINY)
#include <WiFiClient.h>
#endif

#include <string>
#include <vector>
#include <memory>

namespace esphome::nfs_client {

static const char *const TAG = "nfs_client";

//========================================================================
// RPC Protocol Constants (RFC 1831)
//========================================================================

enum RPCMessageType : uint32_t {
  RPC_CALL = 0,
  RPC_REPLY = 1,
};

enum RPCReplyStatus : uint32_t {
  RPC_MSG_ACCEPTED = 0,
  RPC_MSG_DENIED = 1,
};

enum RPCAcceptStatus : uint32_t {
  RPC_SUCCESS = 0,
  RPC_PROG_UNAVAIL = 1,
  RPC_PROG_MISMATCH = 2,
  RPC_PROC_UNAVAIL = 3,
  RPC_GARBAGE_ARGS = 4,
  RPC_SYSTEM_ERR = 5,
};

enum RPCAuthFlavor : uint32_t {
  RPC_AUTH_NULL = 0,
  RPC_AUTH_UNIX = 1,
  RPC_AUTH_SHORT = 2,
  RPC_AUTH_DES = 3,
};

//========================================================================
// NFS Protocol Constants (RFC 1813)
//========================================================================

static constexpr uint32_t NFS_PROGRAM = 100003;
static constexpr uint32_t NFS_VERSION_3 = 3;

static constexpr uint32_t MOUNT_PROGRAM = 100005;
static constexpr uint32_t MOUNT_VERSION_3 = 3;

static constexpr uint32_t PMAP_PROGRAM = 100000;
static constexpr uint32_t PMAP_VERSION = 2;
static constexpr uint16_t PMAP_PORT = 111;

static constexpr uint16_t NFS_DEFAULT_PORT = 2049;
static constexpr uint16_t MOUNT_DEFAULT_PORT = 2049;

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

enum MOUNTv3Procedure : uint32_t {
  MOUNTPROC3_NULL = 0,
  MOUNTPROC3_MNT = 1,
  MOUNTPROC3_DUMP = 2,
  MOUNTPROC3_UMNT = 3,
  MOUNTPROC3_UMNTALL = 4,
  MOUNTPROC3_EXPORT = 5,
};

enum PMAPv2Procedure : uint32_t {
  PMAPPROC_NULL = 0,
  PMAPPROC_SET = 1,
  PMAPPROC_UNSET = 2,
  PMAPPROC_GETPORT = 3,
  PMAPPROC_DUMP = 4,
  PMAPPROC_CALLIT = 5,
};

enum NFSFileType : uint32_t {
  NF3REG = 1,
  NF3DIR = 2,
  NF3BLK = 3,
  NF3CHR = 4,
  NF3LNK = 5,
  NF3SOCK = 6,
  NF3FIFO = 7,
};

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

static constexpr size_t NFS_FHSIZE3 = 64;
static constexpr size_t NFS_MAXNAMLEN = 255;
static constexpr size_t NFS_MAXPATHLEN = 1024;

//========================================================================
// XDR Buffer (RFC 1832)
//========================================================================

class XDRBuffer {
 public:
  XDRBuffer() = default;
  explicit XDRBuffer(size_t capacity) { this->data_.reserve(capacity); }
  explicit XDRBuffer(const std::vector<uint8_t> &data) : data_(data), position_(0) {}

  void encode_uint32(uint32_t value);
  void encode_uint64(uint64_t value);
  void encode_bytes(const uint8_t *data, size_t length);
  void encode_string(const std::string &str);
  void encode_opaque(const uint8_t *data, size_t length);
  void encode_bool(bool value) { this->encode_uint32(value ? 1 : 0); }

  bool decode_uint32(uint32_t &value);
  bool decode_uint64(uint64_t &value);
  bool decode_bytes(uint8_t *data, size_t length);
  bool decode_string(std::string &str);
  bool decode_opaque(std::vector<uint8_t> &data);
  bool decode_opaque_to_buffer(uint8_t *buffer, size_t max_len, size_t &actual_len);
  bool decode_bool(bool &value);

  const std::vector<uint8_t> &data() const { return this->data_; }
  size_t size() const { return this->data_.size(); }
  size_t position() const { return this->position_; }
  void reset() { this->position_ = 0; }
  void skip(size_t bytes) { this->position_ += bytes; }
  void clear() {
    this->data_.clear();
    this->position_ = 0;
  }
  static size_t align_4(size_t size) { return (size + 3) & ~3; }

 protected:
  std::vector<uint8_t> data_;
  size_t position_{0};
};

//========================================================================
// NFS Structures
//========================================================================

struct NFSFileHandle {
  std::vector<uint8_t> data;

  NFSFileHandle() = default;
  explicit NFSFileHandle(const std::vector<uint8_t> &fh_data) : data(fh_data) {}

  bool is_valid() const { return !this->data.empty() && this->data.size() <= NFS_FHSIZE3; }

  void encode(XDRBuffer &xdr) const;
  bool decode(XDRBuffer &xdr);
};

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

  NFSFileAttr()
      : type(NF3REG),
        mode(0),
        nlink(0),
        uid(0),
        gid(0),
        size(0),
        used(0),
        fsid(0),
        fileid(0),
        atime_sec(0),
        atime_nsec(0),
        mtime_sec(0),
        mtime_nsec(0),
        ctime_sec(0),
        ctime_nsec(0) {}

  bool decode(XDRBuffer &xdr);
};

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

class RPCClient {
 public:
  RPCClient() = default;

  void build_call(XDRBuffer &xdr, uint32_t xid, uint32_t program, uint32_t version, uint32_t procedure,
                  uint32_t uid = 0, uint32_t gid = 0);
  bool parse_reply(XDRBuffer &xdr, uint32_t expected_xid, RPCAcceptStatus &status);
  static uint32_t generate_xid() { return millis(); }

 protected:
  void encode_auth_unix_(XDRBuffer &xdr, uint32_t uid, uint32_t gid);
  void encode_auth_null_(XDRBuffer &xdr);
};

//========================================================================
// NFS Client Component
//========================================================================

class NFSClient final : public storage::NetworkStorage, public storage::MountableStorage {
 public:
  NFSClient() = default;
  ~NFSClient() override;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  //========================================================================
  // Configuration
  //========================================================================

  void set_server(const char *server) { this->server_ = server; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_export(const char *export_path) { this->export_path_ = export_path; }
  // Feeds the inherited PathStorage mount path — resolve_path()/consumers read it from there.
  // (A private shadow member here previously left PathStorage's copy null, making this device
  // invisible to path routing despite registering fine.)
  void set_mount_path(const char *mount_path) { this->set_mount_path_(mount_path); }
  void set_uid(uint32_t uid) { this->uid_ = uid; }
  void set_gid(uint32_t gid) { this->gid_ = gid; }
  // Fire one mount attempt on each rising edge of network connectivity (see loop()).
  void set_auto_connect(bool auto_connect) { this->auto_connect_ = auto_connect; }

  bool is_mounted() const { return this->mounted_; }

  //========================================================================
  // MountableStorage interface
  //========================================================================

  // No-RTTI downcast hook — see PathStorage::as_mountable().
  storage::MountableStorage *as_mountable() override { return this; }

  // mount() requests one asynchronous mount attempt; loop() carries it out step by step
  // (never blocking the caller). unmount() tears the mount down synchronously, following
  // the SD-card safe-eject pattern: unregister (drains async worker traffic per the registry
  // contract), UMNT + close, re-register as registered-but-unmounted.
  storage::StorageError mount() override;
  storage::StorageError unmount() override;

  //========================================================================
  // NetworkStorage interface
  //========================================================================

  storage::StorageError get_info(storage::StorageInfo *info) override;
  // connect()/disconnect() are the NetworkStorage names for the same two operations —
  // they delegate to mount()/unmount() above (one implementation, two interface names).
  storage::StorageError connect() override;
  storage::StorageError disconnect() override;
  storage::StorageError read_chunk(const char *path, uint8_t *buf, uint64_t offset, size_t len,
                                   size_t *bytes_transferred) override;
  storage::StorageError write_chunk(const char *path, const uint8_t *buf, uint64_t offset, size_t len,
                                    size_t *bytes_transferred) override;
  storage::StorageError stat(const char *path, storage::FileStat *stat) override;
  storage::StorageError list_dir(const char *path, bool (*callback)(const storage::FileStat *entry, void *ctx),
                                 void *ctx) override;
  storage::StorageError mkdir(const char *path) override;
  storage::StorageError rmdir(const char *path) override;
  storage::StorageError remove(const char *path) override;
  storage::StorageError rename(const char *old_path, const char *new_path) override;

  //========================================================================
  // NFS-specific operations (used internally and optionally by consumers)
  //========================================================================

  bool get_file_attributes(const std::string &path, NFSFileAttr &attr);
  bool get_space_info(uint64_t &total_bytes, uint64_t &free_bytes);

 protected:
  //========================================================================
  // Configuration
  //========================================================================

  std::string server_;
  uint16_t port_{NFS_DEFAULT_PORT};
  std::string export_path_;
  uint32_t uid_{0};
  uint32_t gid_{0};

  //========================================================================
  // Connection State
  //========================================================================

#if defined(USE_ESP_IDF) || defined(USE_ESP32)
  int socket_{-1};
#elif defined(USE_ESP8266) || defined(USE_LIBRETINY)
  std::unique_ptr<WiFiClient> client_;
#endif

  bool connected_{false};
  bool mounted_{false};
  NFSFileHandle root_fh_;

  //========================================================================
  // Async Mount Management
  //========================================================================

  enum class MountState : uint8_t {
    IDLE,
    CONNECTING_PMAP,
    QUERYING_PMAP_MOUNT,
    CONNECTING_MOUNT,
    MOUNTING,
    QUERYING_PMAP_NFS,
    MOUNTED,
    FAILED,
  };

  MountState mount_state_{MountState::IDLE};
  // Set by mount()/connect() or the auto-connect edge below; consumed by loop() to start one
  // mount attempt. No periodic retry exists anymore — FAILED is terminal until the next
  // request (users schedule retries themselves via interval:/automations + storage.mount).
  bool mount_requested_{false};
  // Fire one mount attempt on each rising edge of network connectivity (default on). The
  // boot pass counts as an edge when the network is already up by then.
  bool auto_connect_{true};
  bool network_was_connected_{false};

#if defined(USE_ESP_IDF) || defined(USE_ESP32)
  struct sockaddr_in server_addr_ {};
  bool server_addr_resolved_{false};
#endif

  uint16_t mount_port_{0};
  bool mount_port_discovered_{false};
  uint16_t nfs_port_{0};
  bool nfs_port_discovered_{false};

  //========================================================================
  // RPC Client
  //========================================================================

  RPCClient rpc_;
  std::unique_ptr<uint8_t[]> rpc_response_buffer_;

  //========================================================================
  // File Handle Cache (for chunked reads)
  //========================================================================

  std::string cached_path_;
  NFSFileHandle cached_fh_;
  NFSFileAttr cached_attr_;
  uint32_t cache_timestamp_{0};

  //========================================================================
  // Internal Operations
  //========================================================================

#if defined(USE_ESP_IDF) || defined(USE_ESP32)
  bool resolve_hostname_();
#endif
  bool connect_tcp_();
  void close_connection_();
  void unmount_();
  bool send_rpc_(const XDRBuffer &request, XDRBuffer &response);
  bool query_portmapper_(uint32_t program, uint32_t version, uint16_t &port);

  bool mount_export_(const std::string &export_path, NFSFileHandle &fh);
  bool unmount_export_(const std::string &export_path);

  bool nfs_lookup_(const NFSFileHandle &dir_fh, const std::string &name, NFSFileHandle &fh, NFSFileAttr &attr);
  bool nfs_getattr_(const NFSFileHandle &fh, NFSFileAttr &attr);
  bool nfs_read_(const NFSFileHandle &fh, uint64_t offset, uint32_t count, std::vector<uint8_t> &data);
  bool nfs_write_(const NFSFileHandle &fh, uint64_t offset, const uint8_t *data, size_t length);
  bool nfs_create_(const NFSFileHandle &dir_fh, const std::string &name, uint32_t mode, NFSFileHandle &fh);
  bool nfs_remove_(const NFSFileHandle &dir_fh, const std::string &name);
  bool nfs_mkdir_(const NFSFileHandle &dir_fh, const std::string &name, uint32_t mode, NFSFileHandle &fh);
  bool nfs_rmdir_(const NFSFileHandle &dir_fh, const std::string &name);
  bool nfs_readdir_(const NFSFileHandle &dir_fh, std::vector<NFSDirEntry> &entries);
  bool nfs_rename_(const NFSFileHandle &old_dir_fh, const std::string &old_name, const NFSFileHandle &new_dir_fh,
                   const std::string &new_name);

  bool resolve_path_(const std::string &path, NFSFileHandle &fh, NFSFileAttr &attr);
  bool resolve_parent_path_(const std::string &path, NFSFileHandle &parent_fh, std::string &filename);
  std::vector<std::string> split_path_(const std::string &path);
};

}  // namespace esphome::nfs_client
