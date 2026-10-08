// Ported from corosync-qdevice qdevices/msg.c, Copyright (c) 2015-2020
// Red Hat, Inc., BSD 3-Clause; see LICENSE.txt in this directory.
#include "qnetd_msg.h"

namespace esphome::qnetd {

const uint16_t SUPPORTED_MESSAGES[18] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
const uint16_t SUPPORTED_OPTIONS[24] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                                        12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
const uint16_t SUPPORTED_ALGORITHMS[1] = {uint16_t(Algorithm::ALGORITHM_FFSPLIT)};

static bool decode_node_info(const uint8_t *data, size_t len, NodeInfo &out) {
  TlvIterator it(data, len);
  bool have_id = false;
  int r;
  while ((r = it.next()) > 0) {
    switch (TlvOpt(it.type())) {
      case TlvOpt::TLV_OPT_NODE_ID:
        if (it.length() != 4)
          return false;
        out.node_id = be_get32(it.data());
        have_id = true;
        break;
      case TlvOpt::TLV_OPT_DATA_CENTER_ID:
        if (it.length() != 4)
          return false;
        out.data_center_id = be_get32(it.data());
        break;
      case TlvOpt::TLV_OPT_NODE_STATE:
        if (it.length() != 1)
          return false;
        out.state = NodeState(it.data()[0]);
        break;
      default:
        break;  // unknown nested options ignored
    }
  }
  return r == 0 && have_id;
}

bool msg_decode(MsgType type, const uint8_t *payload, size_t len, MsgDecoded &out) {
  out = MsgDecoded();
  out.type = type;
  TlvIterator it(payload, len);
  int r;
  while ((r = it.next()) > 0) {
    const uint8_t *d = it.data();
    uint16_t l = it.length();
    switch (TlvOpt(it.type())) {
      case TlvOpt::TLV_OPT_MSG_SEQ_NUMBER:
        if (l != 4)
          return false;
        out.seq_number = be_get32(d);
        out.seq_number_set = true;
        break;
      case TlvOpt::TLV_OPT_CLUSTER_NAME:
        out.cluster_name.assign(reinterpret_cast<const char *>(d), l);
        break;
      case TlvOpt::TLV_OPT_TLS_SUPPORTED:
        if (l != 1 || d[0] > 2)
          return false;
        out.tls_supported = TlsMode(d[0]);
        out.tls_supported_set = true;
        break;
      case TlvOpt::TLV_OPT_TLS_CLIENT_CERT_REQUIRED:
        if (l != 1)
          return false;
        out.tls_client_cert_required = d[0];
        out.tls_client_cert_required_set = true;
        break;
      case TlvOpt::TLV_OPT_SUPPORTED_MESSAGES:
        if (l % 2 != 0)
          return false;
        out.supported_messages_present = true;
        break;
      case TlvOpt::TLV_OPT_SUPPORTED_OPTIONS:
        if (l % 2 != 0)
          return false;
        out.supported_options_present = true;
        break;
      case TlvOpt::TLV_OPT_NODE_ID:
        if (l != 4)
          return false;
        out.node_id = be_get32(d);
        out.node_id_set = true;
        break;
      case TlvOpt::TLV_OPT_DECISION_ALGORITHM:
        if (l != 2)
          return false;
        out.decision_algorithm = Algorithm(be_get16(d));
        out.decision_algorithm_set = true;
        break;
      case TlvOpt::TLV_OPT_HEARTBEAT_INTERVAL:
        if (l != 4)
          return false;
        out.heartbeat_interval = be_get32(d);
        out.heartbeat_interval_set = true;
        break;
      case TlvOpt::TLV_OPT_RING_ID:
        if (l != 12)
          return false;
        out.ring_id.node_id = be_get32(d);
        out.ring_id.seq = be_get64(d + 4);
        out.ring_id_set = true;
        break;
      case TlvOpt::TLV_OPT_CONFIG_VERSION:
        if (l != 8)
          return false;
        out.config_version = be_get64(d);
        out.config_version_set = true;
        break;
      case TlvOpt::TLV_OPT_NODE_INFO: {
        NodeInfo ni;
        if (!decode_node_info(d, l, ni))
          return false;
        if (out.nodes.size() >= MAX_NODES_PER_LIST)
          return false;
        out.nodes.nodes.push_back(ni);
        break;
      }
      case TlvOpt::TLV_OPT_NODE_LIST_TYPE:
        if (l != 1 || d[0] > 3)
          return false;
        out.node_list_type = NodeListType(d[0]);
        out.node_list_type_set = true;
        break;
      case TlvOpt::TLV_OPT_VOTE:
        if (l != 1 || d[0] > 5)
          return false;
        out.vote = Vote(d[0]);
        out.vote_set = true;
        break;
      case TlvOpt::TLV_OPT_QUORATE:
        if (l != 1 || d[0] > 1)
          return false;
        out.quorate = d[0];
        out.quorate_set = true;
        break;
      case TlvOpt::TLV_OPT_TIE_BREAKER:
        if (l != 5 || d[0] < 1 || d[0] > 3)
          return false;
        out.tie_breaker.mode = TieBreakerMode(d[0]);
        out.tie_breaker.node_id = be_get32(d + 1);
        out.tie_breaker_set = true;
        break;
      case TlvOpt::TLV_OPT_HEURISTICS:
        if (l != 1 || d[0] > 2)
          return false;
        out.heuristics = Heuristics(d[0]);
        break;
      case TlvOpt::TLV_OPT_KEEP_ACTIVE_PARTITION_TIE_BREAKER:
        if (l != 1)
          return false;
        out.keep_active_partition_tie_breaker = d[0];
        out.keep_active_partition_tie_breaker_set = true;
        break;
      default:
        break;  // upstream: "protocol ignores unknown options"
    }
  }
  return r == 0;
}

// --- builders ---

static void begin_frame(Frame &f, MsgType type) {
  f.clear();
  be_put16(f, uint16_t(type));
  be_put32(f, 0);  // patched by end_frame
}

static void end_frame(Frame &f) {
  uint32_t len = uint32_t(f.size() - MSG_HEADER_LEN);
  f[2] = uint8_t(len >> 24);
  f[3] = uint8_t(len >> 16);
  f[4] = uint8_t(len >> 8);
  f[5] = uint8_t(len);
}

void build_preinit_reply(Frame &out, bool seq_set, uint32_t seq, TlsMode tls, uint8_t cert_required) {
  begin_frame(out, MsgType::MSG_TYPE_PREINIT_REPLY);
  TlvWriter w(out);
  if (seq_set)
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, seq);
  w.add_u8(TlvOpt::TLV_OPT_TLS_SUPPORTED, uint8_t(tls));
  w.add_u8(TlvOpt::TLV_OPT_TLS_CLIENT_CERT_REQUIRED, cert_required);
  end_frame(out);
}

void build_server_error(Frame &out, bool seq_set, uint32_t seq, ReplyError code) {
  begin_frame(out, MsgType::MSG_TYPE_SERVER_ERROR);
  TlvWriter w(out);
  if (seq_set)
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, seq);
  w.add_u16(TlvOpt::TLV_OPT_REPLY_ERROR_CODE, uint16_t(code));
  end_frame(out);
}

void build_init_reply(Frame &out, bool seq_set, uint32_t seq, ReplyError code, bool include_supported_messages,
                      bool include_supported_options, uint32_t max_request_size, uint32_t max_reply_size) {
  begin_frame(out, MsgType::MSG_TYPE_INIT_REPLY);
  TlvWriter w(out);
  if (seq_set)
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, seq);
  w.add_u16(TlvOpt::TLV_OPT_REPLY_ERROR_CODE, uint16_t(code));
  if (include_supported_messages)
    w.add_u16_array(TlvOpt::TLV_OPT_SUPPORTED_MESSAGES, SUPPORTED_MESSAGES, 18);
  if (include_supported_options)
    w.add_u16_array(TlvOpt::TLV_OPT_SUPPORTED_OPTIONS, SUPPORTED_OPTIONS, 24);
  w.add_u32(TlvOpt::TLV_OPT_SERVER_MAXIMUM_REQUEST_SIZE, max_request_size);
  w.add_u32(TlvOpt::TLV_OPT_SERVER_MAXIMUM_REPLY_SIZE, max_reply_size);
  w.add_u16_array(TlvOpt::TLV_OPT_SUPPORTED_DECISION_ALGORITHMS, SUPPORTED_ALGORITHMS, 1);
  end_frame(out);
}

void build_set_option_reply(Frame &out, bool seq_set, uint32_t seq, bool hb_set, uint32_t hb, bool kaptb_set,
                            uint8_t kaptb) {
  begin_frame(out, MsgType::MSG_TYPE_SET_OPTION_REPLY);
  TlvWriter w(out);
  if (seq_set)
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, seq);
  if (hb_set)
    w.add_u32(TlvOpt::TLV_OPT_HEARTBEAT_INTERVAL, hb);
  if (kaptb_set)
    w.add_u8(TlvOpt::TLV_OPT_KEEP_ACTIVE_PARTITION_TIE_BREAKER, kaptb);
  end_frame(out);
}

void build_node_list_reply(Frame &out, uint32_t seq, NodeListType type, const RingId &ring, Vote vote) {
  begin_frame(out, MsgType::MSG_TYPE_NODE_LIST_REPLY);
  TlvWriter w(out);
  w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, seq);
  w.add_u8(TlvOpt::TLV_OPT_NODE_LIST_TYPE, uint8_t(type));
  w.add_ring_id(ring);
  w.add_u8(TlvOpt::TLV_OPT_VOTE, uint8_t(vote));
  end_frame(out);
}

void build_vote_info(Frame &out, uint32_t seq, const RingId &ring, Vote vote) {
  begin_frame(out, MsgType::MSG_TYPE_VOTE_INFO);
  TlvWriter w(out);
  w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, seq);
  w.add_u8(TlvOpt::TLV_OPT_VOTE, uint8_t(vote));
  w.add_ring_id(ring);
  end_frame(out);
}

void build_heuristics_change_reply(Frame &out, bool seq_set, uint32_t seq, Vote vote) {
  begin_frame(out, MsgType::MSG_TYPE_HEURISTICS_CHANGE_REPLY);
  TlvWriter w(out);
  if (seq_set)
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, seq);
  w.add_u8(TlvOpt::TLV_OPT_VOTE, uint8_t(vote));
  end_frame(out);
}

void patch_frame_type(uint8_t *frame, MsgType type) {
  frame[0] = uint8_t(uint16_t(type) >> 8);
  frame[1] = uint8_t(uint16_t(type));
}

const char *vote_str(Vote v) {
  switch (v) {
    case Vote::VOTE_UNDEFINED:
      return "undef";
    case Vote::VOTE_ACK:
      return "ACK";
    case Vote::VOTE_NACK:
      return "NACK";
    case Vote::VOTE_ASK_LATER:
      return "ask-later";
    case Vote::VOTE_WAIT_FOR_REPLY:
      return "wait";
    case Vote::VOTE_NO_CHANGE:
      return "no-change";
  }
  return "?";
}

const char *msg_type_str(MsgType t) {
  switch (t) {
    case MsgType::MSG_TYPE_PREINIT:
      return "preinit";
    case MsgType::MSG_TYPE_PREINIT_REPLY:
      return "preinit-reply";
    case MsgType::MSG_TYPE_STARTTLS:
      return "starttls";
    case MsgType::MSG_TYPE_INIT:
      return "init";
    case MsgType::MSG_TYPE_INIT_REPLY:
      return "init-reply";
    case MsgType::MSG_TYPE_SERVER_ERROR:
      return "server-error";
    case MsgType::MSG_TYPE_SET_OPTION:
      return "set-option";
    case MsgType::MSG_TYPE_SET_OPTION_REPLY:
      return "set-option-reply";
    case MsgType::MSG_TYPE_ECHO_REQUEST:
      return "echo-request";
    case MsgType::MSG_TYPE_ECHO_REPLY:
      return "echo-reply";
    case MsgType::MSG_TYPE_NODE_LIST:
      return "node-list";
    case MsgType::MSG_TYPE_NODE_LIST_REPLY:
      return "node-list-reply";
    case MsgType::MSG_TYPE_ASK_FOR_VOTE:
      return "ask-for-vote";
    case MsgType::MSG_TYPE_ASK_FOR_VOTE_REPLY:
      return "ask-for-vote-reply";
    case MsgType::MSG_TYPE_VOTE_INFO:
      return "vote-info";
    case MsgType::MSG_TYPE_VOTE_INFO_REPLY:
      return "vote-info-reply";
    case MsgType::MSG_TYPE_HEURISTICS_CHANGE:
      return "heuristics-change";
    case MsgType::MSG_TYPE_HEURISTICS_CHANGE_REPLY:
      return "heuristics-change-reply";
  }
  return "?";
}

}  // namespace esphome::qnetd
