#include "qnetd_server.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace esphome::qnetd {

static const char *const TAG = "qnetd";

void QnetdServer::notify_() { this->state_changed_ = true; }

// ---------------------------------------------------------------- lifecycle

int QnetdServer::on_connect(uint64_t now_ms, const char *peer) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (this->sessions_[i].phase == Phase::PHASE_FREE) {
      this->sessions_[i].reset();
      this->sessions_[i].phase = Phase::PHASE_WAIT_PREINIT;
      snprintf(this->sessions_[i].peer, sizeof(this->sessions_[i].peer), "%s", peer);
      this->sessions_[i].deadline_ms = now_ms + PREACTIVE_TIMEOUT_MS;
      ESP_LOGI(TAG, "client %s connected (slot %d)", peer, i);
      this->notify_();
      return i;
    }
  }
  ESP_LOGW(TAG, "client %s rejected: no free slots", peer);
  return -1;
}

void QnetdServer::on_disconnected(int slot, uint64_t now_ms) {
  if (slot < 0 || slot >= MAX_CLIENTS || this->sessions_[slot].phase == Phase::PHASE_FREE)
    return;
  bool was_processing = this->processing_;
  this->processing_ = true;
  ESP_LOGI(TAG, "client %s (slot %d) disconnected", this->sessions_[slot].peer, slot);
  this->leave_cluster_(slot);
  this->sessions_[slot].reset();
  this->processing_ = was_processing;
  if (!was_processing)
    this->drain_closes_(now_ms);
  this->notify_();
}

void QnetdServer::disconnect_client_(int slot, uint64_t now_ms, const char *why) {
  Session &s = this->sessions_[slot];
  if (s.phase == Phase::PHASE_FREE || s.schedule_disconnect)
    return;
  ESP_LOGW(TAG, "disconnecting client %s (slot %d): %s", s.peer, slot, why);
  s.schedule_disconnect = true;
  this->pending_close_.push_back(slot);
  if (!this->processing_)
    this->drain_closes_(now_ms);
}

void QnetdServer::drain_closes_(uint64_t now_ms) {
  this->processing_ = true;
  while (!this->pending_close_.empty()) {
    int slot = this->pending_close_[this->pending_close_.size() - 1];
    this->pending_close_.resize(this->pending_close_.size() - 1);
    if (this->sessions_[slot].phase == Phase::PHASE_FREE)
      continue;
    this->leave_cluster_(slot);
    this->sessions_[slot].reset();
    this->transport_->close_connection(slot);
  }
  this->processing_ = false;
  this->notify_();
}

void QnetdServer::reschedule_dpd_(Session &s, uint64_t now_ms) {
  if (s.phase == Phase::PHASE_ACTIVE && s.heartbeat_ms > 0)
    s.deadline_ms = now_ms + uint64_t(DPD_COEFFICIENT * s.heartbeat_ms);
  else if (s.phase != Phase::PHASE_FREE && s.phase != Phase::PHASE_ACTIVE)
    s.deadline_ms = now_ms + PREACTIVE_TIMEOUT_MS;
}

void QnetdServer::tick(uint64_t now_ms) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    Session &s = this->sessions_[i];
    if (s.phase == Phase::PHASE_FREE || s.deadline_ms == 0 || s.schedule_disconnect)
      continue;
    if (now_ms >= s.deadline_ms)
      this->disconnect_client_(i, now_ms, "dead peer detection timeout");
  }
}

// ---------------------------------------------------------------- framing

void QnetdServer::on_data(int slot, const uint8_t *data, size_t len, uint64_t now_ms) {
  if (slot < 0 || slot >= MAX_CLIENTS || this->sessions_[slot].phase == Phase::PHASE_FREE)
    return;
  Session &s = this->sessions_[slot];
  if (s.schedule_disconnect)
    return;
  s.rx.insert(s.rx.end(), data, data + len);
  this->reschedule_dpd_(s, now_ms);

  this->processing_ = true;
  while (this->sessions_[slot].phase != Phase::PHASE_FREE && !this->sessions_[slot].schedule_disconnect) {
    std::vector<uint8_t> &rx = this->sessions_[slot].rx;
    if (rx.size() < MSG_HEADER_LEN)
      break;
    uint16_t type = be_get16(rx.data());
    uint32_t plen = be_get32(rx.data() + 2);
    if (plen > MAX_RX_FRAME) {
      MsgDecoded dummy;
      this->send_err_(slot, dummy, ReplyError::REPLY_ERROR_MESSAGE_TOO_LONG);
      this->disconnect_client_(slot, now_ms, "frame too long");
      break;
    }
    if (rx.size() < MSG_HEADER_LEN + plen)
      break;
    this->handle_frame_(slot, MsgType(type), rx.data(), MSG_HEADER_LEN + plen, rx.data() + MSG_HEADER_LEN, plen,
                        now_ms);
    // handle_frame may have scheduled a disconnect but rx is still valid
    rx.erase(rx.begin(), rx.begin() + MSG_HEADER_LEN + plen);
  }
  this->processing_ = false;
  this->drain_closes_(now_ms);
}

void QnetdServer::handle_frame_(int slot, MsgType type, const uint8_t *frame, size_t frame_len, const uint8_t *payload,
                                size_t payload_len, uint64_t now_ms) {
  MsgDecoded m;
  if (!msg_decode(type, payload, payload_len, m)) {
    MsgDecoded dummy;
    this->send_err_(slot, dummy, ReplyError::REPLY_ERROR_ERROR_DECODING_MSG);
    this->disconnect_client_(slot, now_ms, "malformed message");
    return;
  }
  Session &s = this->sessions_[slot];
  ESP_LOGD(TAG, "slot %d: %s", slot, msg_type_str(type));

  // phase gating, mirroring upstream handler preambles
  if (type != MsgType::MSG_TYPE_PREINIT && s.phase == Phase::PHASE_WAIT_PREINIT) {
    this->send_err_(slot, m, ReplyError::REPLY_ERROR_PREINIT_REQUIRED);
    return;
  }
  switch (type) {
    case MsgType::MSG_TYPE_PREINIT:
      this->handle_preinit_(slot, m);
      return;
    case MsgType::MSG_TYPE_INIT:
      this->handle_init_(slot, m, now_ms);
      return;
    default:
      break;
  }
  if (s.phase != Phase::PHASE_ACTIVE) {
    this->send_err_(slot, m, ReplyError::REPLY_ERROR_INIT_REQUIRED);
    return;
  }
  switch (type) {
    case MsgType::MSG_TYPE_SET_OPTION:
      this->handle_set_option_(slot, m, now_ms);
      break;
    case MsgType::MSG_TYPE_ECHO_REQUEST:
      this->send_(slot, build_echo_reply(frame, frame_len));
      break;
    case MsgType::MSG_TYPE_NODE_LIST:
      this->handle_node_list_(slot, m);
      break;
    case MsgType::MSG_TYPE_VOTE_INFO_REPLY:
      this->handle_vote_info_reply_(slot, m);
      break;
    case MsgType::MSG_TYPE_HEURISTICS_CHANGE:
      this->handle_heuristics_change_(slot, m);
      break;
    case MsgType::MSG_TYPE_ASK_FOR_VOTE:
      // ffsplit does not support ask_for_vote (upstream error 14)
      this->send_err_(slot, m, ReplyError::REPLY_ERROR_UNSUPPORTED_DECISION_ALGORITHM_MESSAGE);
      break;
    case MsgType::MSG_TYPE_STARTTLS:
      this->send_err_(slot, m, ReplyError::REPLY_ERROR_UNSUPPORTED_MESSAGE);
      break;
    default:
      // server-to-client types arriving from a client
      this->send_err_(slot, m, ReplyError::REPLY_ERROR_UNEXPECTED_MESSAGE);
      break;
  }
}

// ---------------------------------------------------------------- handshake

void QnetdServer::handle_preinit_(int slot, const MsgDecoded &m) {
  Session &s = this->sessions_[slot];
  if (m.cluster_name.empty()) {
    this->send_err_(slot, m, ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION);
    return;
  }
  if (s.phase != Phase::PHASE_WAIT_PREINIT) {
    this->send_err_(slot, m, ReplyError::REPLY_ERROR_UNEXPECTED_MESSAGE);
    return;
  }
  s.cluster_name = m.cluster_name;
  s.phase = Phase::PHASE_WAIT_INIT;
  this->send_(slot, build_preinit_reply(m.seq_number_set, m.seq_number, TlsMode::TLS_MODE_UNSUPPORTED, 0));
}

void QnetdServer::handle_init_(int slot, const MsgDecoded &m, uint64_t now_ms) {
  Session &s = this->sessions_[slot];
  ReplyError code = ReplyError::REPLY_ERROR_NO_ERROR;

  if (s.phase == Phase::PHASE_ACTIVE)
    code = ReplyError::REPLY_ERROR_UNEXPECTED_MESSAGE;

  if (code == ReplyError::REPLY_ERROR_NO_ERROR && !m.node_id_set)
    code = ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION;
  else
    s.node_id = m.node_id;

  if (code == ReplyError::REPLY_ERROR_NO_ERROR && !m.ring_id_set)
    code = ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION;
  else
    s.last_ring_id = m.ring_id;

  if (code == ReplyError::REPLY_ERROR_NO_ERROR) {
    if (!m.heartbeat_interval_set) {
      code = ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION;
    } else if (m.heartbeat_interval < HEARTBEAT_MIN_MS || m.heartbeat_interval > HEARTBEAT_MAX_MS) {
      code = ReplyError::REPLY_ERROR_INVALID_HEARTBEAT_INTERVAL;
    } else {
      s.heartbeat_ms = m.heartbeat_interval;
    }
  }

  if (code == ReplyError::REPLY_ERROR_NO_ERROR) {
    if (!m.tie_breaker_set)
      code = ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION;
    else
      s.tie_breaker = m.tie_breaker;
  }

  if (code == ReplyError::REPLY_ERROR_NO_ERROR) {
    if (!m.decision_algorithm_set)
      code = ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION;
    else if (m.decision_algorithm != Algorithm::ALGORITHM_FFSPLIT)
      code = ReplyError::REPLY_ERROR_UNSUPPORTED_DECISION_ALGORITHM;
    else
      s.algorithm = m.decision_algorithm;
  }

  // consistency with existing members of the same cluster (upstream
  // init_check_new_client)
  if (code == ReplyError::REPLY_ERROR_NO_ERROR) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
      const Session &o = this->sessions_[i];
      if (i == slot || o.phase != Phase::PHASE_ACTIVE || o.cluster_name != s.cluster_name)
        continue;
      if (!(s.tie_breaker == o.tie_breaker)) {
        code = ReplyError::REPLY_ERROR_TIE_BREAKER_DIFFERS_FROM_OTHER_NODES;
        break;
      }
      if (s.algorithm != o.algorithm) {
        code = ReplyError::REPLY_ERROR_ALGORITHM_DIFFERS_FROM_OTHER_NODES;
        break;
      }
      if (s.node_id == o.node_id) {
        code = ReplyError::REPLY_ERROR_DUPLICATE_NODE_ID;
        break;
      }
    }
  }

  if (code == ReplyError::REPLY_ERROR_NO_ERROR) {
    int c = this->find_or_create_cluster_(s.cluster_name);
    if (c < 0) {
      code = ReplyError::REPLY_ERROR_INTERNAL_ERROR;
    } else {
      s.cluster = c;
      if (this->clusters_[c].members == 0) {
        this->clusters_[c].state = FfClusterState::FF_CLUSTER_STATE_WAITING_FOR_CHANGE;
        this->clusters_[c].quorate_partition.clear();
      }
      this->clusters_[c].members++;
      s.phase = Phase::PHASE_ACTIVE;
      this->reschedule_dpd_(s, now_ms);
      ESP_LOGI(TAG, "cluster \"%s\": node %" PRIu32 " joined (%s, hb %" PRIu32 " ms)", s.cluster_name.c_str(),
               s.node_id, s.peer, s.heartbeat_ms);
    }
  }

  this->send_(slot, build_init_reply(m.seq_number_set, m.seq_number, code, m.supported_messages_present,
                                     m.supported_options_present, MAX_RX_FRAME, MAX_RX_FRAME));
  this->notify_();
}

void QnetdServer::handle_set_option_(int slot, const MsgDecoded &m, uint64_t now_ms) {
  Session &s = this->sessions_[slot];
  if (m.heartbeat_interval_set) {
    if (m.heartbeat_interval < HEARTBEAT_MIN_MS || m.heartbeat_interval > HEARTBEAT_MAX_MS) {
      this->send_err_(slot, m, ReplyError::REPLY_ERROR_INVALID_HEARTBEAT_INTERVAL);
      return;
    }
    s.heartbeat_ms = m.heartbeat_interval;
    this->reschedule_dpd_(s, now_ms);
  }
  if (m.keep_active_partition_tie_breaker_set)
    s.keep_active_partition_tb = m.keep_active_partition_tie_breaker != 0;
  this->send_(slot,
              build_set_option_reply(m.seq_number_set, m.seq_number, m.heartbeat_interval_set, s.heartbeat_ms,
                                     m.keep_active_partition_tie_breaker_set, s.keep_active_partition_tb ? 1 : 0));
}

// ---------------------------------------------------------------- node lists

void QnetdServer::handle_node_list_(int slot, const MsgDecoded &m) {
  Session &s = this->sessions_[slot];
  if (!m.node_list_type_set || !m.seq_number_set) {
    this->send_err_(slot, m, ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION);
    return;
  }
  Vote result = Vote::VOTE_NO_CHANGE;
  ReplyError code = ReplyError::REPLY_ERROR_NO_ERROR;

  switch (m.node_list_type) {
    case NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG:
    case NodeListType::NODE_LIST_TYPE_CHANGED_CONFIG: {
      // upstream ffsplit config_node_list_received
      if (m.nodes.empty() || m.nodes.find(s.node_id) == nullptr) {
        this->send_err_(slot, m, ReplyError::REPLY_ERROR_INVALID_CONFIG_NODE_LIST);
        return;
      }
      bool initial = m.node_list_type == NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG;
      if (initial || s.membership_list.empty()) {
        result = Vote::VOTE_ASK_LATER;
      } else {
        TriggerView tv{slot, false, &s.last_ring_id, &m.nodes, &s.membership_list, s.last_heuristics};
        code = this->ffsplit_do_(tv, result);
      }
      break;
    }
    case NodeListType::NODE_LIST_TYPE_MEMBERSHIP: {
      if (!m.ring_id_set) {
        this->send_err_(slot, m, ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION);
        return;
      }
      if (m.nodes.empty() || m.nodes.find(s.node_id) == nullptr) {
        this->send_err_(slot, m, ReplyError::REPLY_ERROR_INVALID_MEMBERSHIP_NODE_LIST);
        return;
      }
      if (s.config_list.empty()) {
        result = Vote::VOTE_ASK_LATER;
      } else {
        TriggerView tv{slot, false, &m.ring_id, &s.config_list, &m.nodes, m.heuristics};
        code = this->ffsplit_do_(tv, result);
      }
      break;
    }
    case NodeListType::NODE_LIST_TYPE_QUORUM:
      if (!m.quorate_set) {
        this->send_err_(slot, m, ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION);
        return;
      }
      result = Vote::VOTE_NO_CHANGE;  // informative only
      break;
  }

  if (code != ReplyError::REPLY_ERROR_NO_ERROR) {
    this->send_err_(slot, m, code);
    return;
  }

  // store lists AFTER the algorithm ran (it compares new vs stored views)
  switch (m.node_list_type) {
    case NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG:
    case NodeListType::NODE_LIST_TYPE_CHANGED_CONFIG:
      s.config_list = m.nodes;
      s.config_version_set = m.config_version_set;
      s.config_version = m.config_version;
      break;
    case NodeListType::NODE_LIST_TYPE_MEMBERSHIP:
      s.membership_list = m.nodes;
      s.last_ring_id = m.ring_id;
      s.last_heuristics = m.heuristics;
      break;
    case NodeListType::NODE_LIST_TYPE_QUORUM:
      break;  // informative only, nothing to keep
  }

  s.last_sent_vote = result;
  if (result == Vote::VOTE_ACK || result == Vote::VOTE_NACK)
    s.last_ack_nack = result;

  this->send_(slot, build_node_list_reply(m.seq_number, m.node_list_type, s.last_ring_id, result));
  this->notify_();
}

void QnetdServer::handle_vote_info_reply_(int slot, const MsgDecoded &m) {
  // port of ffsplit vote_info_reply_received
  Session &s = this->sessions_[slot];
  if (!m.seq_number_set) {
    this->send_err_(slot, m, ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION);
    return;
  }
  if (m.seq_number != s.vote_info_seq) {
    ESP_LOGD(TAG, "stale vote-info reply from node %" PRIu32 " (seq %" PRIu32 ", expected %" PRIu32 ")", s.node_id,
             m.seq_number, s.vote_info_seq);
    return;
  }
  s.ff_state = FfClientState::FF_CLIENT_STATE_WAITING_FOR_CHANGE;
  Cluster &cl = this->clusters_[s.cluster];
  if (cl.state == FfClusterState::FF_CLUSTER_STATE_SENDING_NACKS) {
    if (this->ffsplit_count_state_(s.cluster, FfClientState::FF_CLIENT_STATE_SENDING_NACK) == 0) {
      ESP_LOGD(TAG, "cluster \"%s\": all NACKs acknowledged", cl.name.c_str());
      cl.state = FfClusterState::FF_CLUSTER_STATE_SENDING_ACKS;
      TriggerView tv{slot, false, &s.last_ring_id, &s.config_list, &s.membership_list, s.last_heuristics};
      if (this->ffsplit_send_votes_(tv, true) == 0)
        cl.state = FfClusterState::FF_CLUSTER_STATE_WAITING_FOR_CHANGE;
    }
  } else if (cl.state == FfClusterState::FF_CLUSTER_STATE_SENDING_ACKS) {
    if (this->ffsplit_count_state_(s.cluster, FfClientState::FF_CLIENT_STATE_SENDING_ACK) == 0) {
      ESP_LOGD(TAG, "cluster \"%s\": all ACKs acknowledged", cl.name.c_str());
      cl.state = FfClusterState::FF_CLUSTER_STATE_WAITING_FOR_CHANGE;
    }
  }
  this->notify_();
}

void QnetdServer::handle_heuristics_change_(int slot, const MsgDecoded &m) {
  Session &s = this->sessions_[slot];
  if (!m.seq_number_set || m.heuristics == Heuristics::HEURISTICS_UNDEFINED) {
    this->send_err_(slot, m, ReplyError::REPLY_ERROR_DOESNT_CONTAIN_REQUIRED_OPTION);
    return;
  }
  Vote result = Vote::VOTE_NO_CHANGE;
  ReplyError code = ReplyError::REPLY_ERROR_NO_ERROR;
  if (s.config_list.empty() || s.membership_list.empty()) {
    result = Vote::VOTE_ASK_LATER;
  } else {
    TriggerView tv{slot, false, &s.last_ring_id, &s.config_list, &s.membership_list, m.heuristics};
    code = this->ffsplit_do_(tv, result);
  }
  if (code != ReplyError::REPLY_ERROR_NO_ERROR) {
    this->send_err_(slot, m, code);
    return;
  }
  s.last_heuristics = m.heuristics;
  s.last_sent_vote = result;
  if (result == Vote::VOTE_ACK || result == Vote::VOTE_NACK)
    s.last_ack_nack = result;
  this->send_(slot, build_heuristics_change_reply(m.seq_number_set, m.seq_number, result));
  this->notify_();
}

// ---------------------------------------------------------------- clusters

int QnetdServer::find_or_create_cluster_(const std::string &name) {
  for (int i = 0; i < MAX_CLUSTERS; i++)
    if (this->clusters_[i].used && this->clusters_[i].name == name)
      return i;
  for (int i = 0; i < MAX_CLUSTERS; i++) {
    if (!this->clusters_[i].used) {
      this->clusters_[i] = Cluster();
      this->clusters_[i].used = true;
      this->clusters_[i].name = name;
      return i;
    }
  }
  return -1;
}

void QnetdServer::leave_cluster_(int slot) {
  Session &s = this->sessions_[slot];
  if (s.phase != Phase::PHASE_ACTIVE || s.cluster < 0)
    return;
  // upstream ffsplit client_disconnect: re-run the algorithm with the
  // leaving client excluded, so the surviving partition can be promoted
  Vote result;
  TriggerView tv{slot, true, &s.last_ring_id, &s.config_list, &s.membership_list, s.last_heuristics};
  this->ffsplit_do_(tv, result);
  Cluster &cl = this->clusters_[s.cluster];
  cl.members--;
  if (cl.members <= 0)
    cl = Cluster();
  s.cluster = -1;
}

// ---------------------------------------------------------------- ffsplit

const NodeList *QnetdServer::eff_config_(const Session &s, const TriggerView &tv) const {
  return (!tv.leaving && &s == &this->sessions_[tv.slot]) ? tv.config : &s.config_list;
}
const NodeList *QnetdServer::eff_membership_(const Session &s, const TriggerView &tv) const {
  return (!tv.leaving && &s == &this->sessions_[tv.slot]) ? tv.membership : &s.membership_list;
}
const RingId *QnetdServer::eff_ring_(const Session &s, const TriggerView &tv) const {
  return (!tv.leaving && &s == &this->sessions_[tv.slot]) ? tv.ring_id : &s.last_ring_id;
}
Heuristics QnetdServer::eff_heuristics_(const Session &s, const TriggerView &tv) const {
  return (!tv.leaving && &s == &this->sessions_[tv.slot]) ? tv.heuristics : s.last_heuristics;
}

bool QnetdServer::ffsplit_is_stable_(const TriggerView &tv) const {
  int cluster = this->sessions_[tv.slot].cluster;
  // 1. all active clients share the same config node-id set (pairwise)
  for (int i = 0; i < MAX_CLIENTS; i++) {
    const Session &c1 = this->sessions_[i];
    if (c1.phase != Phase::PHASE_ACTIVE || c1.cluster != cluster || this->skip_in_cluster_walk_(c1, tv))
      continue;
    for (int j = 0; j < MAX_CLIENTS; j++) {
      const Session &c2 = this->sessions_[j];
      if (i == j || c2.phase != Phase::PHASE_ACTIVE || c2.cluster != cluster || this->skip_in_cluster_walk_(c2, tv))
        continue;
      const NodeList *l1 = this->eff_config_(c1, tv);
      const NodeList *l2 = this->eff_config_(c2, tv);
      for (const auto &n : l1->nodes)
        if (l2->find(n.node_id) == nullptr)
          return false;
    }
  }
  // 2. clients within one partition share ring id and membership set
  for (int i = 0; i < MAX_CLIENTS; i++) {
    const Session &c1 = this->sessions_[i];
    if (c1.phase != Phase::PHASE_ACTIVE || c1.cluster != cluster || this->skip_in_cluster_walk_(c1, tv))
      continue;
    const NodeList *mem1 = this->eff_membership_(c1, tv);
    const RingId *ring1 = this->eff_ring_(c1, tv);
    for (const auto &n : mem1->nodes) {
      const Session *c2 = nullptr;
      for (int j = 0; j < MAX_CLIENTS; j++) {
        const Session &cand = this->sessions_[j];
        if (cand.phase == Phase::PHASE_ACTIVE && cand.cluster == cluster && cand.node_id == n.node_id &&
            !this->skip_in_cluster_walk_(cand, tv)) {
          c2 = &cand;
          break;
        }
      }
      if (c2 == nullptr)
        continue;  // that member is not connected to us
      const NodeList *mem2 = this->eff_membership_(*c2, tv);
      const RingId *ring2 = this->eff_ring_(*c2, tv);
      if (*ring1 != *ring2)
        return false;
      for (const auto &n3 : mem1->nodes)
        if (mem2->find(n3.node_id) == nullptr)
          return false;
    }
  }
  return true;
}

bool QnetdServer::ffsplit_is_preferred_partition_(const Session *c, const NodeList *cfg, const NodeList *mem) const {
  uint32_t preferred = 0;
  switch (c->tie_breaker.mode) {
    case TieBreakerMode::TIE_BREAKER_MODE_LOWEST: {
      preferred = cfg->nodes[0].node_id;
      for (const auto &n : cfg->nodes)
        if (n.node_id < preferred)
          preferred = n.node_id;
      break;
    }
    case TieBreakerMode::TIE_BREAKER_MODE_HIGHEST: {
      preferred = cfg->nodes[0].node_id;
      for (const auto &n : cfg->nodes)
        if (n.node_id > preferred)
          preferred = n.node_id;
      break;
    }
    case TieBreakerMode::TIE_BREAKER_MODE_NODE_ID:
      preferred = c->tie_breaker.node_id;
      break;
  }
  return mem->find(preferred) != nullptr;
}

void QnetdServer::ffsplit_partition_stats_(const Session *c, const NodeList *mem, Heuristics h, size_t &clients,
                                           size_t &pass, size_t &fail) const {
  clients = pass = fail = 0;
  if (c == nullptr || mem == nullptr)
    return;
  int cluster = c->cluster;
  for (const auto &n : mem->nodes) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
      const Session &s = this->sessions_[i];
      if (s.phase != Phase::PHASE_ACTIVE || s.cluster != cluster || s.node_id != n.node_id)
        continue;
      clients++;
      Heuristics eff = (&s == c) ? h : s.last_heuristics;
      if (eff == Heuristics::HEURISTICS_PASS)
        pass++;
      else if (eff == Heuristics::HEURISTICS_FAIL)
        fail++;
      break;
    }
  }
}

bool QnetdServer::ffsplit_partition_better_(const Session *c1, const NodeList *cfg1, const NodeList *mem1,
                                            Heuristics h1, const Session *c2, const NodeList *mem2, Heuristics h2,
                                            const NodeList &prev_quorate, bool keep_active_tb) const {
  if (cfg1->size() % 2 != 0) {
    // odd clusters never split 50:50: strict majority or nothing
    return mem1->size() > cfg1->size() / 2;
  }
  if (mem1->size() > cfg1->size() / 2)
    return true;
  if (mem1->size() < cfg1->size() / 2)
    return false;

  // exact 50:50 split
  size_t n1, p1, f1, n2, p2, f2;
  this->ffsplit_partition_stats_(c1, mem1, h1, n1, p1, f1);
  this->ffsplit_partition_stats_(c2, mem2, h2, n2, p2, f2);
  // fail <= active, so this cannot go negative
  int64_t score1 = int64_t(n1) + (int64_t(p1) - int64_t(f1));
  int64_t score2 = int64_t(n2) + (int64_t(p2) - int64_t(f2));
  if (score1 != score2)
    return score1 > score2;
  if (n1 != n2)
    return n1 > n2;

  if (keep_active_tb && c2 != nullptr) {
    bool in1 = prev_quorate.find(c1->node_id) != nullptr;
    bool in2 = prev_quorate.find(c2->node_id) != nullptr;
    if (in1 != in2)
      return in1;
  }
  return this->ffsplit_is_preferred_partition_(c1, cfg1, mem1);
}

const NodeList *QnetdServer::ffsplit_select_partition_(const TriggerView &tv) const {
  int cluster = this->sessions_[tv.slot].cluster;
  const Cluster &cl = this->clusters_[cluster];

  bool keep_active_tb = true;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    const Session &s = this->sessions_[i];
    if (s.phase == Phase::PHASE_ACTIVE && s.cluster == cluster && !s.keep_active_partition_tb) {
      keep_active_tb = false;
      break;
    }
  }

  const Session *best = nullptr;
  const NodeList *best_mem = nullptr;
  Heuristics best_h = Heuristics::HEURISTICS_UNDEFINED;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    const Session &s = this->sessions_[i];
    if (s.phase != Phase::PHASE_ACTIVE || s.cluster != cluster || this->skip_in_cluster_walk_(s, tv))
      continue;
    const NodeList *cfg = this->eff_config_(s, tv);
    const NodeList *mem = this->eff_membership_(s, tv);
    Heuristics h = this->eff_heuristics_(s, tv);
    if (cfg->empty() || mem->empty())
      continue;
    if (this->ffsplit_partition_better_(&s, cfg, mem, h, best, best_mem, best_h, cl.quorate_partition,
                                        keep_active_tb)) {
      best = &s;
      best_mem = mem;
      best_h = h;
    }
  }
  return best_mem;
}

void QnetdServer::ffsplit_update_states_(const TriggerView &tv, const NodeList *winner) {
  int cluster = this->sessions_[tv.slot].cluster;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    Session &s = this->sessions_[i];
    if (s.phase != Phase::PHASE_ACTIVE || s.cluster != cluster)
      continue;
    if (tv.leaving && s.node_id == this->sessions_[tv.slot].node_id) {
      s.ff_state = FfClientState::FF_CLIENT_STATE_WAITING_FOR_CHANGE;
      continue;
    }
    if (winner == nullptr || winner->find(s.node_id) == nullptr)
      s.ff_state = FfClientState::FF_CLIENT_STATE_SENDING_NACK;
    else
      s.ff_state = FfClientState::FF_CLIENT_STATE_SENDING_ACK;
  }
}

size_t QnetdServer::ffsplit_send_votes_(const TriggerView &tv, bool send_acks) {
  int cluster = this->sessions_[tv.slot].cluster;
  size_t sent = 0;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    Session &s = this->sessions_[i];
    if (s.phase != Phase::PHASE_ACTIVE || s.cluster != cluster || this->skip_in_cluster_walk_(s, tv))
      continue;
    Vote v = Vote::VOTE_UNDEFINED;
    if (send_acks && s.ff_state == FfClientState::FF_CLIENT_STATE_SENDING_ACK)
      v = Vote::VOTE_ACK;
    if (!send_acks && s.ff_state == FfClientState::FF_CLIENT_STATE_SENDING_NACK)
      v = Vote::VOTE_NACK;
    if (v == Vote::VOTE_UNDEFINED)
      continue;
    const RingId *ring = this->eff_ring_(s, tv);
    s.vote_info_seq++;
    sent++;
    s.last_sent_vote = v;
    s.last_ack_nack = v;
    ESP_LOGI(TAG, "cluster \"%s\": vote-info %s -> node %" PRIu32 " (ring %" PRIu32 "/%" PRIu64 ")",
             this->clusters_[cluster].name.c_str(), vote_str(v), s.node_id, ring->node_id, ring->seq);
    this->send_(i, build_vote_info(s.vote_info_seq, *ring, v));
  }
  return sent;
}

size_t QnetdServer::ffsplit_count_state_(int cluster, FfClientState st) const {
  size_t n = 0;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    const Session &s = this->sessions_[i];
    if (s.phase == Phase::PHASE_ACTIVE && s.cluster == cluster && s.ff_state == st)
      n++;
  }
  return n;
}

ReplyError QnetdServer::ffsplit_do_(const TriggerView &tv, Vote &result) {
  int cluster = this->sessions_[tv.slot].cluster;
  Cluster &cl = this->clusters_[cluster];

  cl.state = FfClusterState::FF_CLUSTER_STATE_WAITING_FOR_STABLE_MEMBERSHIP;
  if (!this->ffsplit_is_stable_(tv)) {
    ESP_LOGD(TAG, "cluster \"%s\": membership not yet stable", cl.name.c_str());
    result = Vote::VOTE_WAIT_FOR_REPLY;
    return ReplyError::REPLY_ERROR_NO_ERROR;
  }

  const NodeList *winner = this->ffsplit_select_partition_(tv);
  this->decisions_++;
  if (winner == nullptr) {
    ESP_LOGW(TAG, "cluster \"%s\": no partition can be quorate", cl.name.c_str());
  } else {
    ESP_LOGI(TAG, "cluster \"%s\": quorate partition selected (%u nodes)", cl.name.c_str(), (unsigned) winner->size());
  }

  // note: winner may point at a client's stored list or the trigger's
  // incoming list, never at cl.quorate_partition, so this copy is safe
  NodeList new_quorate = winner ? *winner : NodeList();

  this->ffsplit_update_states_(tv, winner);
  cl.quorate_partition = std::move(new_quorate);

  cl.state = FfClusterState::FF_CLUSTER_STATE_SENDING_NACKS;
  if (this->ffsplit_send_votes_(tv, false) == 0) {
    cl.state = FfClusterState::FF_CLUSTER_STATE_SENDING_ACKS;
    if (this->ffsplit_send_votes_(tv, true) == 0)
      cl.state = FfClusterState::FF_CLUSTER_STATE_WAITING_FOR_CHANGE;
  }
  result = Vote::VOTE_NO_CHANGE;
  return ReplyError::REPLY_ERROR_NO_ERROR;
}

// ---------------------------------------------------------------- status

int QnetdServer::connected_clients() const {
  int n = 0;
  for (const auto &s : this->sessions_)
    if (s.phase == Phase::PHASE_ACTIVE)
      n++;
  return n;
}

bool QnetdServer::any_ack() const {
  for (const auto &s : this->sessions_)
    if (s.phase == Phase::PHASE_ACTIVE && s.last_ack_nack == Vote::VOTE_ACK)
      return true;
  return false;
}

std::string QnetdServer::status_string() const {
  std::string out;
  for (int c = 0; c < MAX_CLUSTERS; c++) {
    if (!this->clusters_[c].used)
      continue;
    if (!out.empty())
      out += " | ";
    out += this->clusters_[c].name + ":";
    for (const auto &s : this->sessions_) {
      if (s.phase != Phase::PHASE_ACTIVE || s.cluster != c)
        continue;
      char buf[48];
      snprintf(buf, sizeof(buf), " %" PRIu32 "=%s", s.node_id, vote_str(s.last_ack_nack));
      out += buf;
    }
  }
  if (out.empty())
    out = "idle";
  return out;
}

}  // namespace esphome::qnetd
