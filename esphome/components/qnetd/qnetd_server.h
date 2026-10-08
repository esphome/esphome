// qnetd server core: client sessions, message dispatch, dead-peer detection
// and the ffsplit decision algorithm. Portable: no sockets, no clock; the
// embedder drives on_connect/on_data/on_disconnected/tick and provides a
// QnetdTransport. Ported from corosync-qdevice qdevices/qnetd-*.c,
// Copyright (c) 2015-2020 Red Hat, Inc., BSD 3-Clause; see LICENSE.txt in
// this directory.
#pragma once
#include <array>
#include <string>
#include <vector>

#include "esphome/core/helpers.h"
#include "qnetd_msg.h"

namespace esphome::qnetd {

// Two nodes of the arbitrated cluster plus slack for reconnect races.
constexpr int MAX_CLIENTS = 4;
constexpr int MAX_CLUSTERS = 2;
constexpr uint32_t HEARTBEAT_MIN_MS = 1000;  // upstream qnetd defaults
constexpr uint32_t HEARTBEAT_MAX_MS = 2 * 60 * 1000;
constexpr double DPD_COEFFICIENT = 1.5;           // upstream default
constexpr uint32_t PREACTIVE_TIMEOUT_MS = 30000;  // ours: cap a stuck handshake
constexpr uint32_t MAX_RX_FRAME = 32768;          // the minimum the client accepts
constexpr size_t PEER_NAME_LEN = 46;              // INET6_ADDRSTRLEN

// Byte sink for the server; implemented by the component on top of sockets.
class QnetdTransport {
 public:
  virtual ~QnetdTransport() = default;
  virtual void send_frame(int slot, const uint8_t *data, size_t len) = 0;
  // Close the connection. Must not call back into the server from inside
  // this call; the server has already cleaned the slot up.
  virtual void close_connection(int slot) = 0;
};

class QnetdServer {
 public:
  explicit QnetdServer(QnetdTransport *transport) : transport_(transport) {}

  // Returns a slot id >= 0, or -1 when full (caller should close the socket).
  int on_connect(uint64_t now_ms, const char *peer);
  // Feed received bytes. May emit frames and request closes via the transport.
  void on_data(int slot, const uint8_t *data, size_t len, uint64_t now_ms);
  // The transport noticed the connection is gone (EOF/reset).
  void on_disconnected(int slot, uint64_t now_ms);
  // Drive timers (dead peer detection). Call at least a few times per second.
  void tick(uint64_t now_ms);

  // --- introspection (for entities / status) ---
  int connected_clients() const;
  bool any_ack() const;
  std::string status_string() const;
  uint32_t decisions() const { return this->decisions_; }
  // True once after anything observable changed (connections, votes).
  bool consume_state_change() {
    bool changed = this->state_changed_;
    this->state_changed_ = false;
    return changed;
  }

 private:
  enum class Phase : uint8_t { PHASE_FREE, PHASE_WAIT_PREINIT, PHASE_WAIT_INIT, PHASE_ACTIVE };
  enum class FfClientState : uint8_t {
    FF_CLIENT_STATE_WAITING_FOR_CHANGE,
    FF_CLIENT_STATE_SENDING_NACK,
    FF_CLIENT_STATE_SENDING_ACK,
  };
  enum class FfClusterState : uint8_t {
    FF_CLUSTER_STATE_WAITING_FOR_CHANGE,
    FF_CLUSTER_STATE_WAITING_FOR_STABLE_MEMBERSHIP,
    FF_CLUSTER_STATE_SENDING_NACKS,
    FF_CLUSTER_STATE_SENDING_ACKS,
  };

  struct Cluster {
    bool used = false;
    std::string name;
    int members = 0;
    FfClusterState state = FfClusterState::FF_CLUSTER_STATE_WAITING_FOR_CHANGE;
    NodeList quorate_partition;
  };

  struct Session {
    Phase phase = Phase::PHASE_FREE;
    char peer[PEER_NAME_LEN] = "";
    std::string cluster_name;
    int cluster = -1;  // index into clusters_, valid in PHASE_ACTIVE
    uint32_t node_id = 0;
    RingId last_ring_id;
    uint32_t heartbeat_ms = 0;
    TieBreaker tie_breaker;
    Algorithm algorithm = Algorithm::ALGORITHM_FFSPLIT;
    bool keep_active_partition_tb = true;  // upstream default enabled
    NodeList config_list;
    NodeList membership_list;
    Heuristics last_heuristics = Heuristics::HEURISTICS_UNDEFINED;
    Vote last_ack_nack = Vote::VOTE_UNDEFINED;
    FfClientState ff_state = FfClientState::FF_CLIENT_STATE_WAITING_FOR_CHANGE;
    uint32_t vote_info_seq = 0;
    uint64_t deadline_ms = 0;
    bool schedule_disconnect = false;
    std::vector<uint8_t> rx;  // grows to the largest frame seen, up to MAX_RX_FRAME

    void reset() { *this = Session(); }
  };

  // --- plumbing ---
  // Sends the frame built into tx_.
  void send_(int slot) { this->transport_->send_frame(slot, this->tx_.data(), this->tx_.size()); }
  void send_err_(int slot, const MsgDecoded &m, ReplyError code) {
    build_server_error(this->tx_, m.seq_number_set, m.seq_number, code);
    this->send_(slot);
  }
  void drain_closes_(uint64_t now_ms);
  void disconnect_client_(int slot, uint64_t now_ms, const char *why);
  void reschedule_dpd_(Session &s, uint64_t now_ms);
  void notify_();

  // --- message handlers ---
  void handle_frame_(int slot, MsgType type, uint8_t *frame, size_t frame_len, const uint8_t *payload,
                     size_t payload_len, uint64_t now_ms);
  void handle_preinit_(int slot, const MsgDecoded &m);
  void handle_init_(int slot, const MsgDecoded &m, uint64_t now_ms);
  void handle_set_option_(int slot, const MsgDecoded &m, uint64_t now_ms);
  void handle_node_list_(int slot, const MsgDecoded &m);
  void handle_vote_info_reply_(int slot, const MsgDecoded &m);
  void handle_heuristics_change_(int slot, const MsgDecoded &m);

  // --- cluster helpers ---
  int find_or_create_cluster_(const std::string &name);
  void leave_cluster_(int slot);

  // --- ffsplit (port of qnetd-algo-ffsplit.c) ---
  // View of the triggering client's *incoming* lists; peers use stored state.
  struct TriggerView {
    int slot;
    bool leaving;
    const RingId *ring_id;
    const NodeList *config;
    const NodeList *membership;
    Heuristics heuristics;
  };
  ReplyError ffsplit_do_(const TriggerView &tv, Vote &result);
  bool ffsplit_is_stable_(const TriggerView &tv) const;
  const NodeList *ffsplit_select_partition_(const TriggerView &tv) const;
  bool ffsplit_partition_better_(const Session *c1, const NodeList *cfg1, const NodeList *mem1, Heuristics h1,
                                 const Session *c2, const NodeList *mem2, Heuristics h2, const NodeList &prev_quorate,
                                 bool keep_active_tb) const;
  bool ffsplit_is_preferred_partition_(const Session *c, const NodeList *cfg, const NodeList *mem) const;
  void ffsplit_partition_stats_(const Session *c, const NodeList *mem, Heuristics h, size_t &clients, size_t &pass,
                                size_t &fail) const;
  void ffsplit_update_states_(const TriggerView &tv, const NodeList *winner);
  size_t ffsplit_send_votes_(const TriggerView &tv, bool send_acks);
  size_t ffsplit_count_state_(int cluster, FfClientState st) const;
  // effective per-client view (trigger override)
  const NodeList *eff_config_(const Session &s, const TriggerView &tv) const;
  const NodeList *eff_membership_(const Session &s, const TriggerView &tv) const;
  const RingId *eff_ring_(const Session &s, const TriggerView &tv) const;
  Heuristics eff_heuristics_(const Session &s, const TriggerView &tv) const;
  bool skip_in_cluster_walk_(const Session &s, const TriggerView &tv) const {
    return tv.leaving && s.node_id == this->sessions_[tv.slot].node_id;
  }

  QnetdTransport *transport_;
  Frame tx_;  // reused for every outgoing frame
  std::array<Session, MAX_CLIENTS> sessions_;
  std::array<Cluster, MAX_CLUSTERS> clusters_;
  StaticVector<int, MAX_CLIENTS> pending_close_;
  bool processing_{false};
  bool state_changed_{false};
  uint32_t decisions_{0};
};

}  // namespace esphome::qnetd
