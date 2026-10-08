// qnetd wire types and TLV encoding.
// Ported from corosync-qdevice qdevices/tlv.{h,c} (BSD).
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "esphome/core/helpers.h"

namespace esphome::qnetd {

// --- protocol enums (values are wire values; do not renumber) ---

enum class MsgType : uint16_t {
  MSG_TYPE_PREINIT = 0,
  MSG_TYPE_PREINIT_REPLY = 1,
  MSG_TYPE_STARTTLS = 2,
  MSG_TYPE_INIT = 3,
  MSG_TYPE_INIT_REPLY = 4,
  MSG_TYPE_SERVER_ERROR = 5,
  MSG_TYPE_SET_OPTION = 6,
  MSG_TYPE_SET_OPTION_REPLY = 7,
  MSG_TYPE_ECHO_REQUEST = 8,
  MSG_TYPE_ECHO_REPLY = 9,
  MSG_TYPE_NODE_LIST = 10,
  MSG_TYPE_NODE_LIST_REPLY = 11,
  MSG_TYPE_ASK_FOR_VOTE = 12,
  MSG_TYPE_ASK_FOR_VOTE_REPLY = 13,
  MSG_TYPE_VOTE_INFO = 14,
  MSG_TYPE_VOTE_INFO_REPLY = 15,
  MSG_TYPE_HEURISTICS_CHANGE = 16,
  MSG_TYPE_HEURISTICS_CHANGE_REPLY = 17,
};

enum class TlvOpt : uint16_t {
  TLV_OPT_MSG_SEQ_NUMBER = 0,
  TLV_OPT_CLUSTER_NAME = 1,
  TLV_OPT_TLS_SUPPORTED = 2,
  TLV_OPT_TLS_CLIENT_CERT_REQUIRED = 3,
  TLV_OPT_SUPPORTED_MESSAGES = 4,
  TLV_OPT_SUPPORTED_OPTIONS = 5,
  TLV_OPT_REPLY_ERROR_CODE = 6,
  TLV_OPT_SERVER_MAXIMUM_REQUEST_SIZE = 7,
  TLV_OPT_SERVER_MAXIMUM_REPLY_SIZE = 8,
  TLV_OPT_NODE_ID = 9,
  TLV_OPT_SUPPORTED_DECISION_ALGORITHMS = 10,
  TLV_OPT_DECISION_ALGORITHM = 11,
  TLV_OPT_HEARTBEAT_INTERVAL = 12,
  TLV_OPT_RING_ID = 13,
  TLV_OPT_CONFIG_VERSION = 14,
  TLV_OPT_DATA_CENTER_ID = 15,
  TLV_OPT_NODE_STATE = 16,
  TLV_OPT_NODE_INFO = 17,
  TLV_OPT_NODE_LIST_TYPE = 18,
  TLV_OPT_VOTE = 19,
  TLV_OPT_QUORATE = 20,
  TLV_OPT_TIE_BREAKER = 21,
  TLV_OPT_HEURISTICS = 22,
  TLV_OPT_KEEP_ACTIVE_PARTITION_TIE_BREAKER = 23,
};

enum class TlsMode : uint8_t { TLS_MODE_UNSUPPORTED = 0, TLS_MODE_SUPPORTED = 1, TLS_MODE_REQUIRED = 2 };

enum class ReplyError : uint16_t {
  REPLY_ERROR_NO_ERROR = 0,
  REPLY_ERROR_UNSUPPORTED_NEEDED_MESSAGE = 1,
  REPLY_ERROR_UNSUPPORTED_NEEDED_OPTION = 2,
  REPLY_ERROR_TLS_REQUIRED = 3,
  REPLY_ERROR_UNSUPPORTED_MESSAGE = 4,
  REPLY_ERROR_MESSAGE_TOO_LONG = 5,
  REPLY_ERROR_PREINIT_REQUIRED = 6,
  REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION = 7,
  REPLY_ERROR_UNEXPECTED_MESSAGE = 8,
  REPLY_ERROR_ERROR_DECODING_MSG = 9,
  REPLY_ERROR_INTERNAL_ERROR = 10,
  REPLY_ERROR_INIT_REQUIRED = 11,
  REPLY_ERROR_UNSUPPORTED_DECISION_ALGORITHM = 12,
  REPLY_ERROR_INVALID_HEARTBEAT_INTERVAL = 13,
  REPLY_ERROR_UNSUPPORTED_DECISION_ALGORITHM_MESSAGE = 14,
  REPLY_ERROR_TIE_BREAKER_DIFFERS_FROM_OTHER_NODES = 15,
  REPLY_ERROR_ALGORITHM_DIFFERS_FROM_OTHER_NODES = 16,
  REPLY_ERROR_DUPLICATE_NODE_ID = 17,
  REPLY_ERROR_INVALID_CONFIG_NODE_LIST = 18,
  REPLY_ERROR_INVALID_MEMBERSHIP_NODE_LIST = 19,
};

enum class Algorithm : uint16_t {
  ALGORITHM_TEST = 0,
  ALGORITHM_FFSPLIT = 1,
  ALGORITHM_TWONODELMS = 2,
  ALGORITHM_LMS = 3
};

enum class NodeState : uint8_t {
  NODE_STATE_NOT_SET = 0,
  NODE_STATE_MEMBER = 1,
  NODE_STATE_DEAD = 2,
  NODE_STATE_LEAVING = 3
};

enum class NodeListType : uint8_t {
  NODE_LIST_TYPE_INITIAL_CONFIG = 0,
  NODE_LIST_TYPE_CHANGED_CONFIG = 1,
  NODE_LIST_TYPE_MEMBERSHIP = 2,
  NODE_LIST_TYPE_QUORUM = 3,
};

enum class Vote : uint8_t {
  VOTE_UNDEFINED = 0,
  VOTE_ACK = 1,
  VOTE_NACK = 2,
  VOTE_ASK_LATER = 3,
  VOTE_WAIT_FOR_REPLY = 4,
  VOTE_NO_CHANGE = 5,
};

enum class Heuristics : uint8_t { HEURISTICS_UNDEFINED = 0, HEURISTICS_PASS = 1, HEURISTICS_FAIL = 2 };

enum class TieBreakerMode : uint8_t {
  TIE_BREAKER_MODE_LOWEST = 1,
  TIE_BREAKER_MODE_HIGHEST = 2,
  TIE_BREAKER_MODE_NODE_ID = 3
};

struct RingId {
  uint32_t node_id = 0;
  uint64_t seq = 0;
  bool operator==(const RingId &o) const { return node_id == o.node_id && seq == o.seq; }
  bool operator!=(const RingId &o) const { return !(*this == o); }
};

struct TieBreaker {
  TieBreakerMode mode = TieBreakerMode::TIE_BREAKER_MODE_LOWEST;
  uint32_t node_id = 0;  // only meaningful for mode == NODE_ID
  bool operator==(const TieBreaker &o) const {
    if (mode != o.mode)
      return false;
    return mode != TieBreakerMode::TIE_BREAKER_MODE_NODE_ID || node_id == o.node_id;
  }
};

struct NodeInfo {
  uint32_t node_id = 0;
  uint32_t data_center_id = 0;  // 0 = not set
  NodeState state = NodeState::NODE_STATE_NOT_SET;
};

// Upstream has no fixed bound; clusters larger than this do not need a qdevice.
constexpr size_t MAX_NODES_PER_LIST = 16;

// Bounded node list, copied by value where upstream clones lists.
struct NodeList {
  StaticVector<NodeInfo, MAX_NODES_PER_LIST> nodes;
  bool empty() const { return nodes.empty(); }
  size_t size() const { return nodes.size(); }
  void clear() { nodes.clear(); }
  const NodeInfo *find(uint32_t node_id) const {
    for (const auto &n : nodes) {
      if (n.node_id == node_id)
        return &n;
    }
    return nullptr;
  }
};

// --- big-endian helpers ---

inline void be_put16(std::vector<uint8_t> &b, uint16_t v) {
  b.push_back(uint8_t(v >> 8));
  b.push_back(uint8_t(v));
}
inline void be_put32(std::vector<uint8_t> &b, uint32_t v) {
  b.push_back(uint8_t(v >> 24));
  b.push_back(uint8_t(v >> 16));
  b.push_back(uint8_t(v >> 8));
  b.push_back(uint8_t(v));
}
inline void be_put64(std::vector<uint8_t> &b, uint64_t v) {
  be_put32(b, uint32_t(v >> 32));
  be_put32(b, uint32_t(v));
}
inline uint16_t be_get16(const uint8_t *p) { return uint16_t(p[0]) << 8 | p[1]; }
inline uint32_t be_get32(const uint8_t *p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
inline uint64_t be_get64(const uint8_t *p) { return uint64_t(be_get32(p)) << 32 | be_get32(p + 4); }

// --- TLV writer: appends options to a byte buffer ---

class TlvWriter {
 public:
  explicit TlvWriter(std::vector<uint8_t> &buf) : buf_(buf) {}

  void add(TlvOpt opt, const uint8_t *data, uint16_t len) {
    be_put16(buf_, uint16_t(opt));
    be_put16(buf_, len);
    buf_.insert(buf_.end(), data, data + len);
  }
  void add_u8(TlvOpt opt, uint8_t v) { add(opt, &v, 1); }
  void add_u16(TlvOpt opt, uint16_t v) {
    uint8_t t[2] = {uint8_t(v >> 8), uint8_t(v)};
    add(opt, t, 2);
  }
  void add_u32(TlvOpt opt, uint32_t v) {
    uint8_t t[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
    add(opt, t, 4);
  }
  void add_u64(TlvOpt opt, uint64_t v) {
    uint8_t t[8];
    for (int i = 0; i < 8; i++)
      t[i] = uint8_t(v >> (56 - 8 * i));
    add(opt, t, 8);
  }
  void add_string(TlvOpt opt, const char *s, size_t len) {
    add(opt, reinterpret_cast<const uint8_t *>(s), uint16_t(len));
  }
  void add_u16_array(TlvOpt opt, const uint16_t *arr, size_t n) {
    be_put16(buf_, uint16_t(opt));
    be_put16(buf_, uint16_t(n * 2));
    for (size_t i = 0; i < n; i++)
      be_put16(buf_, arr[i]);
  }
  void add_ring_id(const RingId &r) {
    uint8_t t[12];
    t[0] = uint8_t(r.node_id >> 24);
    t[1] = uint8_t(r.node_id >> 16);
    t[2] = uint8_t(r.node_id >> 8);
    t[3] = uint8_t(r.node_id);
    for (int i = 0; i < 8; i++)
      t[4 + i] = uint8_t(r.seq >> (56 - 8 * i));
    add(TlvOpt::TLV_OPT_RING_ID, t, 12);
  }
  void add_tie_breaker(const TieBreaker &tb) {
    uint8_t t[5];
    t[0] = uint8_t(tb.mode);
    uint32_t id = (tb.mode == TieBreakerMode::TIE_BREAKER_MODE_NODE_ID) ? tb.node_id : 0;
    t[1] = uint8_t(id >> 24);
    t[2] = uint8_t(id >> 16);
    t[3] = uint8_t(id >> 8);
    t[4] = uint8_t(id);
    add(TlvOpt::TLV_OPT_TIE_BREAKER, t, 5);
  }
  // node info is a nested TLV blob
  void add_node_info(const NodeInfo &ni) {
    std::vector<uint8_t> sub;
    TlvWriter w(sub);
    w.add_u32(TlvOpt::TLV_OPT_NODE_ID, ni.node_id);
    if (ni.data_center_id != 0)
      w.add_u32(TlvOpt::TLV_OPT_DATA_CENTER_ID, ni.data_center_id);
    if (ni.state != NodeState::NODE_STATE_NOT_SET)
      w.add_u8(TlvOpt::TLV_OPT_NODE_STATE, uint8_t(ni.state));
    add(TlvOpt::TLV_OPT_NODE_INFO, sub.data(), uint16_t(sub.size()));
  }

 private:
  std::vector<uint8_t> &buf_;
};

// --- TLV iterator over a raw payload ---

class TlvIterator {
 public:
  TlvIterator(const uint8_t *payload, size_t len) : p_(payload), len_(len) {}

  // Advance to next option. Returns 1 = positioned, 0 = clean end, -1 = malformed.
  int next() {
    size_t next_pos = first_ ? 0 : pos_ + 4 + cur_len_;
    first_ = false;
    if (next_pos == len_)
      return 0;
    if (next_pos + 4 > len_)
      return -1;
    pos_ = next_pos;
    cur_type_ = be_get16(p_ + pos_);
    cur_len_ = be_get16(p_ + pos_ + 2);
    if (pos_ + 4 + cur_len_ > len_)
      return -1;
    return 1;
  }
  uint16_t type() const { return cur_type_; }
  uint16_t length() const { return cur_len_; }
  const uint8_t *data() const { return p_ + pos_ + 4; }

 private:
  const uint8_t *p_;
  size_t len_;
  size_t pos_ = 0;
  uint16_t cur_type_ = 0;
  uint16_t cur_len_ = 0;
  bool first_ = true;
};

}  // namespace esphome::qnetd
