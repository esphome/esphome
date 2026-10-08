// Frame decode/encode, mirroring corosync-qdevice qdevices/msg.{h,c}.
#pragma once
#include <string>
#include "qnetd_tlv.h"

namespace esphome::qnetd {

constexpr size_t MSG_HEADER_LEN = 6;  // u16 type + u32 payload length

// All 18 message types, advertised in INIT_REPLY.
extern const uint16_t SUPPORTED_MESSAGES[18];
// All 24 option types, advertised in INIT_REPLY.
extern const uint16_t SUPPORTED_OPTIONS[24];
// We implement ffsplit only.
extern const uint16_t SUPPORTED_ALGORITHMS[1];

struct MsgDecoded {
  MsgType type = MsgType::MSG_TYPE_PREINIT;

  bool seq_number_set = false;
  uint32_t seq_number = 0;
  std::string cluster_name;  // empty = not present (wire name is never empty)
  bool tls_supported_set = false;
  TlsMode tls_supported = TlsMode::TLS_MODE_UNSUPPORTED;
  bool tls_client_cert_required_set = false;
  uint8_t tls_client_cert_required = 0;
  bool supported_messages_present = false;
  bool supported_options_present = false;
  bool node_id_set = false;
  uint32_t node_id = 0;
  bool decision_algorithm_set = false;
  Algorithm decision_algorithm = Algorithm::ALGORITHM_TEST;
  bool heartbeat_interval_set = false;
  uint32_t heartbeat_interval = 0;
  bool ring_id_set = false;
  RingId ring_id;
  bool config_version_set = false;
  uint64_t config_version = 0;
  NodeList nodes;
  bool node_list_type_set = false;
  NodeListType node_list_type = NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG;
  bool vote_set = false;
  Vote vote = Vote::VOTE_UNDEFINED;
  bool quorate_set = false;
  uint8_t quorate = 0;
  bool tie_breaker_set = false;
  TieBreaker tie_breaker;
  Heuristics heuristics = Heuristics::HEURISTICS_UNDEFINED;  // valid even when absent
  bool keep_active_partition_tie_breaker_set = false;
  uint8_t keep_active_partition_tie_breaker = 0;
};

// Decode payload of a frame (header already parsed). Unknown options are
// skipped, matching upstream. Returns true on success.
bool msg_decode(MsgType type, const uint8_t *payload, size_t len, MsgDecoded &out);

// --- frame builders (server -> client) ---
// Each returns a complete frame incl. header.

using Frame = std::vector<uint8_t>;

Frame build_preinit_reply(bool seq_set, uint32_t seq, TlsMode tls, uint8_t cert_required);
Frame build_server_error(bool seq_set, uint32_t seq, ReplyError code);
Frame build_init_reply(bool seq_set, uint32_t seq, ReplyError code, bool include_supported_messages,
                       bool include_supported_options, uint32_t max_request_size, uint32_t max_reply_size);
Frame build_set_option_reply(bool seq_set, uint32_t seq, bool hb_set, uint32_t hb, bool kaptb_set, uint8_t kaptb);
// Echo reply = byte copy of the request frame with the type rewritten.
Frame build_echo_reply(const uint8_t *request_frame, size_t frame_len);
Frame build_node_list_reply(uint32_t seq, NodeListType type, const RingId &ring, Vote vote);
Frame build_vote_info(uint32_t seq, const RingId &ring, Vote vote);
Frame build_heuristics_change_reply(bool seq_set, uint32_t seq, Vote vote);

const char *vote_str(Vote v);
const char *msg_type_str(MsgType t);

}  // namespace esphome::qnetd
