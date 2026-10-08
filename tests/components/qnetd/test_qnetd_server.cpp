// Protocol and scenario tests for the qnetd core. A minimal qdevice-side
// client builds the same frames corosync-qdevice sends, so the server is
// exercised over real wire bytes without sockets.
#include <gtest/gtest.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "esphome/components/qnetd/qnetd_server.h"

namespace esphome::qnetd {
namespace {

class MockTransport : public QnetdTransport {
 public:
  struct Sent {
    int slot;
    std::vector<uint8_t> frame;
  };
  std::vector<Sent> sent;  // full ordered log, never cleared
  std::vector<int> closed;

  void send_frame(int slot, const uint8_t *d, size_t n) override {
    sent.push_back({slot, std::vector<uint8_t>(d, d + n)});
  }
  void close_connection(int slot) override { closed.push_back(slot); }

  // frames sent to `slot` since index `from` (into the global log)
  std::vector<MsgDecoded> frames_for(int slot, size_t from = 0) {
    std::vector<MsgDecoded> out;
    for (size_t i = from; i < sent.size(); i++) {
      if (sent[i].slot != slot)
        continue;
      const auto &f = sent[i].frame;
      MsgDecoded m;
      EXPECT_TRUE(msg_decode(MsgType(be_get16(f.data())), f.data() + MSG_HEADER_LEN, f.size() - MSG_HEADER_LEN, m));
      out.push_back(m);
    }
    return out;
  }
  size_t mark() const { return sent.size(); }
};

// Minimal qdevice-side frame builders.
struct MockClient {
  uint32_t seq = 0;

  static void patch(Frame &f) {
    uint32_t len = uint32_t(f.size() - MSG_HEADER_LEN);
    f[2] = uint8_t(len >> 24);
    f[3] = uint8_t(len >> 16);
    f[4] = uint8_t(len >> 8);
    f[5] = uint8_t(len);
  }
  static Frame begin(MsgType type) {
    Frame f;
    be_put16(f, uint16_t(type));
    be_put32(f, 0);
    return f;
  }
  Frame preinit(const char *cluster) {
    Frame f = begin(MsgType::MSG_TYPE_PREINIT);
    TlvWriter w(f);
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, ++seq);
    w.add_string(TlvOpt::TLV_OPT_CLUSTER_NAME, cluster, strlen(cluster));
    patch(f);
    return f;
  }
  Frame init(uint32_t node_id, const RingId &ring, uint32_t hb = 8000, TieBreaker tb = {},
             Algorithm alg = Algorithm::ALGORITHM_FFSPLIT) {
    Frame f = begin(MsgType::MSG_TYPE_INIT);
    TlvWriter w(f);
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, ++seq);
    static const uint16_t MSGS[] = {0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    w.add_u16_array(TlvOpt::TLV_OPT_SUPPORTED_MESSAGES, MSGS, 17);
    static const uint16_t OPTS[] = {0, 1, 2, 3, 4, 5};
    w.add_u16_array(TlvOpt::TLV_OPT_SUPPORTED_OPTIONS, OPTS, 6);
    w.add_u32(TlvOpt::TLV_OPT_NODE_ID, node_id);
    w.add_u16(TlvOpt::TLV_OPT_DECISION_ALGORITHM, uint16_t(alg));
    w.add_u32(TlvOpt::TLV_OPT_HEARTBEAT_INTERVAL, hb);
    w.add_tie_breaker(tb);
    w.add_ring_id(ring);
    patch(f);
    return f;
  }
  Frame config_list(NodeListType type, const std::vector<uint32_t> &ids, bool version_set = false,
                    uint64_t version = 0) {
    Frame f = begin(MsgType::MSG_TYPE_NODE_LIST);
    TlvWriter w(f);
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, ++seq);
    w.add_u8(TlvOpt::TLV_OPT_NODE_LIST_TYPE, uint8_t(type));
    if (version_set)
      w.add_u64(TlvOpt::TLV_OPT_CONFIG_VERSION, version);
    for (uint32_t id : ids)
      w.add_node_info({id, 0, NodeState::NODE_STATE_NOT_SET});
    patch(f);
    return f;
  }
  Frame membership(const RingId &ring, const std::vector<uint32_t> &ids,
                   Heuristics h = Heuristics::HEURISTICS_UNDEFINED) {
    Frame f = begin(MsgType::MSG_TYPE_NODE_LIST);
    TlvWriter w(f);
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, ++seq);
    w.add_u8(TlvOpt::TLV_OPT_NODE_LIST_TYPE, uint8_t(NodeListType::NODE_LIST_TYPE_MEMBERSHIP));
    w.add_ring_id(ring);
    if (h != Heuristics::HEURISTICS_UNDEFINED)
      w.add_u8(TlvOpt::TLV_OPT_HEURISTICS, uint8_t(h));
    for (uint32_t id : ids)
      w.add_node_info({id, 0, NodeState::NODE_STATE_MEMBER});
    patch(f);
    return f;
  }
  Frame quorum_list(bool quorate, const std::vector<uint32_t> &ids) {
    Frame f = begin(MsgType::MSG_TYPE_NODE_LIST);
    TlvWriter w(f);
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, ++seq);
    w.add_u8(TlvOpt::TLV_OPT_NODE_LIST_TYPE, uint8_t(NodeListType::NODE_LIST_TYPE_QUORUM));
    w.add_u8(TlvOpt::TLV_OPT_QUORATE, quorate ? 1 : 0);
    for (uint32_t id : ids)
      w.add_node_info({id, 0, NodeState::NODE_STATE_MEMBER});
    patch(f);
    return f;
  }
  Frame vote_info_reply(uint32_t vi_seq) {
    Frame f = begin(MsgType::MSG_TYPE_VOTE_INFO_REPLY);
    TlvWriter w(f);
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, vi_seq);
    patch(f);
    return f;
  }
  Frame echo_request() {
    Frame f = begin(MsgType::MSG_TYPE_ECHO_REQUEST);
    TlvWriter w(f);
    w.add_u32(TlvOpt::TLV_OPT_MSG_SEQ_NUMBER, ++seq);
    patch(f);
    return f;
  }
};

struct Rig {
  MockTransport tp;
  QnetdServer server{&tp};
  uint64_t now = 1000;
  std::map<int, MockClient> clients;

  int connect() { return server.on_connect(now, "test-peer"); }
  void feed(int slot, const Frame &f) { server.on_data(slot, f.data(), f.size(), now); }
  // full handshake up to active
  int join(const char *cluster, uint32_t node_id, const RingId &ring, TieBreaker tb = {}) {
    int s = connect();
    feed(s, clients[s].preinit(cluster));
    feed(s, clients[s].init(node_id, ring, 8000, tb));
    return s;
  }
  // latest vote_info sent to slot after `from`; returns its wire seq or 0
  uint32_t last_vote_info(int slot, Vote &v, size_t from = 0) {
    uint32_t seq = 0;
    for (auto &m : tp.frames_for(slot, from)) {
      if (m.type == MsgType::MSG_TYPE_VOTE_INFO) {
        seq = m.seq_number;
        v = m.vote;
      }
    }
    return seq;
  }
  int count_vote_infos(int slot, Vote v, size_t from = 0) {
    int n = 0;
    for (auto &m : tp.frames_for(slot, from)) {
      if (m.type == MsgType::MSG_TYPE_VOTE_INFO && m.vote == v)
        n++;
    }
    return n;
  }
  std::string status() const {
    char buf[256];
    server.status_to(buf);
    return buf;
  }
  bool was_closed(int slot) const {
    for (int c : tp.closed) {
      if (c == slot)
        return true;
    }
    return false;
  }
};

// Build a two-node ACKed cluster; a = node 4, b = node 2.
void ack_two_nodes(Rig &r, int &a, int &b, RingId ring = {2, 100}) {
  a = r.join("testcluster", 4, ring);
  b = r.join("testcluster", 2, ring);
  r.feed(a, r.clients[a].config_list(NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG, {2, 4}));
  r.feed(a, r.clients[a].membership(ring, {2, 4}));
  Vote v;
  uint32_t s1 = r.last_vote_info(a, v);
  if (s1)
    r.feed(a, r.clients[a].vote_info_reply(s1));
  r.feed(b, r.clients[b].config_list(NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG, {2, 4}));
  size_t mk = r.tp.mark();
  r.feed(b, r.clients[b].membership(ring, {2, 4}));
  uint32_t s2 = r.last_vote_info(b, v, mk);
  uint32_t s3 = r.last_vote_info(a, v, mk);
  if (s2)
    r.feed(b, r.clients[b].vote_info_reply(s2));
  if (s3)
    r.feed(a, r.clients[a].vote_info_reply(s3));
  ASSERT_TRUE(r.server.any_ack());
}

}  // namespace

TEST(QnetdMsg, GoldenBytes) {
  // PREINIT_REPLY with seq 7: type 0x0001, len, TLVs: seq(0,4,7) tls(2,1,0) cert(3,1,0)
  Frame f;
  build_preinit_reply(f, true, 7, TlsMode::TLS_MODE_UNSUPPORTED, 0);
  const uint8_t expect[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x12,              // header, len 18
                            0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x07,  // seq=7
                            0x00, 0x02, 0x00, 0x01, 0x00,                    // tls=0
                            0x00, 0x03, 0x00, 0x01, 0x00};                   // cert=0
  ASSERT_EQ(f.size(), sizeof(expect));
  EXPECT_EQ(memcmp(f.data(), expect, sizeof(expect)), 0);

  // ring id encoding: node 4, seq 0xb7c
  Frame v;
  build_vote_info(v, 1, {4, 0xb7c}, Vote::VOTE_ACK);
  // header(6) + seq tlv(8) + vote tlv(5) + ring tlv(16)
  ASSERT_EQ(v.size(), 6u + 8 + 5 + 16);
  const uint8_t ring_expect[] = {0x00, 0x0d, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x7c};
  EXPECT_EQ(memcmp(v.data() + 6 + 8 + 5, ring_expect, sizeof(ring_expect)), 0);

  // round-trip
  MsgDecoded m;
  ASSERT_TRUE(msg_decode(MsgType(be_get16(v.data())), v.data() + 6, v.size() - 6, m));
  EXPECT_EQ(m.type, MsgType::MSG_TYPE_VOTE_INFO);
  EXPECT_TRUE(m.vote_set);
  EXPECT_EQ(m.vote, Vote::VOTE_ACK);
  EXPECT_TRUE(m.ring_id_set);
  EXPECT_EQ(m.ring_id.node_id, 4u);
  EXPECT_EQ(m.ring_id.seq, 0xb7cu);
}

TEST(QnetdServer, HandshakeAndOrdering) {
  Rig r;
  int s = r.connect();
  ASSERT_EQ(s, 0);

  // init before preinit -> PREINIT_REQUIRED, connection stays
  r.feed(s, r.clients[s].init(4, {4, 1}));
  auto fr = r.tp.frames_for(s);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_SERVER_ERROR);
  EXPECT_TRUE(r.tp.closed.empty());

  size_t mk = r.tp.mark();
  r.feed(s, r.clients[s].preinit("testcluster"));
  fr = r.tp.frames_for(s, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_PREINIT_REPLY);
  EXPECT_TRUE(fr[0].tls_supported_set);
  EXPECT_EQ(fr[0].tls_supported, TlsMode::TLS_MODE_UNSUPPORTED);

  // bad heartbeat -> error 13 carried in INIT_REPLY, like upstream
  mk = r.tp.mark();
  r.feed(s, r.clients[s].init(4, {4, 1}, 100));  // 100 ms < min
  fr = r.tp.frames_for(s, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_INIT_REPLY);
  EXPECT_EQ(r.server.connected_clients(), 0);

  mk = r.tp.mark();
  r.feed(s, r.clients[s].init(4, {4, 1}));
  fr = r.tp.frames_for(s, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_INIT_REPLY);
  EXPECT_EQ(r.server.connected_clients(), 1);

  // echo round-trip, seq preserved
  mk = r.tp.mark();
  r.feed(s, r.clients[s].echo_request());
  fr = r.tp.frames_for(s, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_ECHO_REPLY);
  EXPECT_TRUE(fr[0].seq_number_set);
  EXPECT_EQ(fr[0].seq_number, r.clients[s].seq);
}

TEST(QnetdServer, TwoNodesJoinAck) {
  Rig r;
  RingId ring{2, 100};
  int a = r.join("testcluster", 4, ring);

  // A: initial config -> ASK_LATER
  size_t mk = r.tp.mark();
  r.feed(a, r.clients[a].config_list(NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG, {2, 4}));
  auto fr = r.tp.frames_for(a, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_NODE_LIST_REPLY);
  EXPECT_EQ(fr[0].vote, Vote::VOTE_ASK_LATER);

  // A: membership [2,4], B not yet connected -> stable -> vote_info ACK to A
  mk = r.tp.mark();
  r.feed(a, r.clients[a].membership(ring, {2, 4}));
  Vote v = Vote::VOTE_UNDEFINED;
  uint32_t seq = r.last_vote_info(a, v, mk);
  ASSERT_NE(seq, 0u);
  EXPECT_EQ(v, Vote::VOTE_ACK);
  r.feed(a, r.clients[a].vote_info_reply(seq));

  // B boots later: config + membership -> both end ACKed
  int b = r.join("testcluster", 2, ring);
  r.feed(b, r.clients[b].config_list(NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG, {2, 4}));
  mk = r.tp.mark();
  r.feed(b, r.clients[b].membership(ring, {2, 4}));
  Vote va = Vote::VOTE_UNDEFINED, vb = Vote::VOTE_UNDEFINED;
  uint32_t sa = r.last_vote_info(a, va, mk);
  uint32_t sb = r.last_vote_info(b, vb, mk);
  EXPECT_EQ(vb, Vote::VOTE_ACK);
  if (sa)
    r.feed(a, r.clients[a].vote_info_reply(sa));
  if (sb)
    r.feed(b, r.clients[b].vote_info_reply(sb));
  EXPECT_TRUE(r.server.any_ack());
  EXPECT_EQ(r.count_vote_infos(a, Vote::VOTE_NACK), 0);
  EXPECT_EQ(r.count_vote_infos(b, Vote::VOTE_NACK), 0);
}

TEST(QnetdServer, FirstReporterWaitsForSilentPeer) {
  // When both nodes are connected but only one has sent its lists, the
  // reporter gets WAIT_FOR_REPLY, not a vote (upstream stability rule).
  Rig r;
  RingId ring{2, 100};
  int a = r.join("testcluster", 4, ring);
  r.join("testcluster", 2, ring);
  r.feed(a, r.clients[a].config_list(NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG, {2, 4}));
  size_t mk = r.tp.mark();
  r.feed(a, r.clients[a].membership(ring, {2, 4}));
  auto fr = r.tp.frames_for(a, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_NODE_LIST_REPLY);
  EXPECT_EQ(fr[0].vote, Vote::VOTE_WAIT_FOR_REPLY);
  Vote v;
  EXPECT_EQ(r.last_vote_info(a, v, mk), 0u);
}

TEST(QnetdServer, NodeRebootSurvivorKeepsVote) {
  Rig r;
  int a, b;
  ack_two_nodes(r, a, b);

  // B goes away (reboot); A reforms alone on a new ring
  size_t mk = r.tp.mark();
  r.server.on_disconnected(b, r.now);
  RingId ring2{4, 101};
  r.feed(a, r.clients[a].membership(ring2, {4}));
  Vote v = Vote::VOTE_UNDEFINED;
  uint32_t seq = r.last_vote_info(a, v, mk);
  ASSERT_NE(seq, 0u);
  EXPECT_EQ(v, Vote::VOTE_ACK);  // 1 of 2 with no competitor -> ACK
  r.feed(a, r.clients[a].vote_info_reply(seq));
  EXPECT_TRUE(r.server.any_ack());
  EXPECT_EQ(r.count_vote_infos(a, Vote::VOTE_NACK, mk), 0);
}

TEST(QnetdServer, SplitBrainExactlyOneAck) {
  Rig r;
  int a, b;  // a = node 4, b = node 2
  ack_two_nodes(r, a, b);

  // link cut: each node reforms alone. A (node 4) reports first.
  size_t mk = r.tp.mark();
  RingId ring_a{4, 200};
  r.feed(a, r.clients[a].membership(ring_a, {4}));
  // not stable yet (B still claims [2,4] on the old ring) -> WAIT_FOR_REPLY
  auto fr = r.tp.frames_for(a, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_NODE_LIST_REPLY);
  EXPECT_EQ(fr[0].vote, Vote::VOTE_WAIT_FOR_REPLY);

  // B (node 2) reports its half
  mk = r.tp.mark();
  RingId ring_b{2, 201};
  r.feed(b, r.clients[b].membership(ring_b, {2}));

  // decision: tie-breaker lowest -> node 2 (b) wins; node 4 (a) NACKed
  Vote va = Vote::VOTE_UNDEFINED, vb = Vote::VOTE_UNDEFINED;
  uint32_t sa = r.last_vote_info(a, va, mk);
  uint32_t sb = r.last_vote_info(b, vb, mk);
  ASSERT_NE(sa, 0u);
  EXPECT_EQ(va, Vote::VOTE_NACK);
  EXPECT_EQ(sb, 0u);  // the ACK must not go out before the NACK is acknowledged

  // loser acknowledges its NACK; only now may the winner see ACK
  r.feed(a, r.clients[a].vote_info_reply(sa));
  sb = r.last_vote_info(b, vb, mk);
  ASSERT_NE(sb, 0u);
  EXPECT_EQ(vb, Vote::VOTE_ACK);
  r.feed(b, r.clients[b].vote_info_reply(sb));

  // exactly one ACK holder
  EXPECT_EQ(r.count_vote_infos(a, Vote::VOTE_ACK, mk), 0);
  EXPECT_EQ(r.count_vote_infos(b, Vote::VOTE_ACK, mk), 1);
}

TEST(QnetdServer, SplitWithDeadLoserNeedsDpd) {
  Rig r;
  int a, b;
  ack_two_nodes(r, a, b);

  // split; A reports alone; B is dead and never reports
  size_t mk = r.tp.mark();
  RingId ring_a{4, 300};
  r.feed(a, r.clients[a].membership(ring_a, {4}));
  auto fr = r.tp.frames_for(a, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].vote, Vote::VOTE_WAIT_FOR_REPLY);  // unstable: B's stored view disagrees
  Vote v = Vote::VOTE_UNDEFINED;
  EXPECT_EQ(r.last_vote_info(a, v, mk), 0u);  // no vote of any kind yet

  // time passes; the survivor keeps heartbeating, B stays silent
  r.now += 6500;
  r.server.tick(r.now);
  r.feed(a, r.clients[a].echo_request());  // refreshes A's deadline
  r.now += 6500;
  r.server.tick(r.now);  // B (silent since the split) crosses 1.5 * 8000 ms
  ASSERT_EQ(r.tp.closed.size(), 1u);
  EXPECT_EQ(r.tp.closed[0], b);  // only B disconnected

  // with B gone the cluster is stable again and A gets the vote
  uint32_t seq = r.last_vote_info(a, v, mk);
  ASSERT_NE(seq, 0u);
  EXPECT_EQ(v, Vote::VOTE_ACK);
  r.feed(a, r.clients[a].vote_info_reply(seq));
  EXPECT_TRUE(r.server.any_ack());
}

TEST(QnetdServer, StaleVoteInfoReplyIgnored) {
  Rig r;
  int a, b;
  ack_two_nodes(r, a, b);
  size_t mk = r.tp.mark();
  RingId ring_a{4, 400};
  r.feed(a, r.clients[a].membership(ring_a, {4}));
  RingId ring_b{2, 401};
  r.feed(b, r.clients[b].membership(ring_b, {2}));
  Vote va;
  uint32_t sa = r.last_vote_info(a, va, mk);
  ASSERT_NE(sa, 0u);
  EXPECT_EQ(va, Vote::VOTE_NACK);
  // stale seq (sa - 1) must be ignored: no ACK released
  r.feed(a, r.clients[a].vote_info_reply(sa - 1));
  Vote vb = Vote::VOTE_UNDEFINED;
  EXPECT_EQ(r.last_vote_info(b, vb, mk), 0u);
  // correct seq releases it
  r.feed(a, r.clients[a].vote_info_reply(sa));
  EXPECT_NE(r.last_vote_info(b, vb, mk), 0u);
  EXPECT_EQ(vb, Vote::VOTE_ACK);
}

TEST(QnetdServer, InitConsistencyErrors) {
  Rig r;
  RingId ring{2, 1};
  r.join("testcluster", 4, ring);
  EXPECT_EQ(r.server.connected_clients(), 1);

  // duplicate node id
  int b = r.connect();
  r.feed(b, r.clients[b].preinit("testcluster"));
  size_t mk = r.tp.mark();
  r.feed(b, r.clients[b].init(4, ring));
  auto fr = r.tp.frames_for(b, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_INIT_REPLY);
  EXPECT_EQ(r.server.connected_clients(), 1);  // b not admitted

  // differing tie-breaker
  int c = r.connect();
  r.feed(c, r.clients[c].preinit("testcluster"));
  mk = r.tp.mark();
  r.feed(c, r.clients[c].init(2, ring, 8000, {TieBreakerMode::TIE_BREAKER_MODE_HIGHEST, 0}));
  fr = r.tp.frames_for(c, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_INIT_REPLY);
  EXPECT_EQ(r.server.connected_clients(), 1);

  // unsupported algorithm
  int d = r.connect();
  r.feed(d, r.clients[d].preinit("testcluster"));
  mk = r.tp.mark();
  r.feed(d, r.clients[d].init(2, ring, 8000, {}, Algorithm::ALGORITHM_LMS));
  fr = r.tp.frames_for(d, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_INIT_REPLY);
  EXPECT_EQ(r.server.connected_clients(), 1);
}

TEST(QnetdServer, MalformedAndOversize) {
  Rig r;
  int s = r.connect();
  // oversize frame: header claims 1 MB
  uint8_t big[6] = {0x00, 0x00, 0x00, 0x10, 0x00, 0x00};
  r.server.on_data(s, big, 6, r.now);
  EXPECT_TRUE(r.was_closed(s));

  // truncated TLV inside a full frame
  int s2 = r.connect();
  Frame f;
  be_put16(f, 0);     // preinit
  be_put32(f, 3);     // claims 3-byte payload
  f.push_back(0x00);  // half a TLV header
  f.push_back(0x01);
  f.push_back(0x00);
  r.feed(s2, f);
  EXPECT_TRUE(r.was_closed(s2));

  // server-to-client message type arriving from a client -> UNEXPECTED_MESSAGE, stays open
  Rig r2;
  RingId ring{2, 1};
  int a = r2.join("testcluster", 4, ring);
  size_t mk = r2.tp.mark();
  Frame weird = MockClient::begin(MsgType::MSG_TYPE_NODE_LIST_REPLY);
  r2.feed(a, weird);
  auto fr = r2.tp.frames_for(a, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_SERVER_ERROR);
  EXPECT_TRUE(r2.tp.closed.empty());
}

TEST(QnetdServer, QuorumListInformative) {
  Rig r;
  int a, b;
  ack_two_nodes(r, a, b);
  size_t mk = r.tp.mark();
  r.feed(a, r.clients[a].quorum_list(true, {2, 4}));
  auto fr = r.tp.frames_for(a, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_NODE_LIST_REPLY);
  EXPECT_EQ(fr[0].vote, Vote::VOTE_NO_CHANGE);
}

TEST(QnetdServer, FragmentedDelivery) {
  // frames delivered one byte at a time must still parse
  Rig r;
  int s = r.connect();
  Frame f = r.clients[s].preinit("testcluster");
  for (uint8_t byte : f)
    r.server.on_data(s, &byte, 1, r.now);
  auto fr = r.tp.frames_for(s);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_PREINIT_REPLY);
}

TEST(QnetdServer, HeuristicsBreakTheSplit) {
  // 50:50 split where the tie-breaker prefers node 2 but node 2's
  // heuristics FAIL: score (active + pass - fail) must override it
  Rig r;
  int a, b;  // a = node 4, b = node 2
  ack_two_nodes(r, a, b);
  size_t mk = r.tp.mark();
  r.feed(a, r.clients[a].membership({4, 500}, {4}, Heuristics::HEURISTICS_PASS));
  r.feed(b, r.clients[b].membership({2, 501}, {2}, Heuristics::HEURISTICS_FAIL));
  Vote va = Vote::VOTE_UNDEFINED, vb = Vote::VOTE_UNDEFINED;
  uint32_t sb = r.last_vote_info(b, vb, mk);
  ASSERT_NE(sb, 0u);
  EXPECT_EQ(vb, Vote::VOTE_NACK);  // the tie-breaker's favourite loses on score
  r.feed(b, r.clients[b].vote_info_reply(sb));
  uint32_t sa = r.last_vote_info(a, va, mk);
  ASSERT_NE(sa, 0u);
  EXPECT_EQ(va, Vote::VOTE_ACK);
}

TEST(QnetdServer, DuplicateMembershipIdsRejected) {
  // [self, self] would count as a majority of a two-node config; reject it
  Rig r;
  RingId ring{4, 1};
  int a = r.join("testcluster", 4, ring);
  r.feed(a, r.clients[a].config_list(NodeListType::NODE_LIST_TYPE_INITIAL_CONFIG, {2, 4}));
  size_t mk = r.tp.mark();
  r.feed(a, r.clients[a].membership(ring, {4, 4}));
  auto fr = r.tp.frames_for(a, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_SERVER_ERROR);
  EXPECT_TRUE(r.tp.closed.empty());
  EXPECT_FALSE(r.server.any_ack());

  // the same for a config list
  mk = r.tp.mark();
  r.feed(a, r.clients[a].config_list(NodeListType::NODE_LIST_TYPE_CHANGED_CONFIG, {4, 4}));
  fr = r.tp.frames_for(a, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_SERVER_ERROR);
}

TEST(QnetdServer, RepeatedInitKeepsIdentity) {
  Rig r;
  int a, b;
  ack_two_nodes(r, a, b);
  size_t mk = r.tp.mark();
  r.feed(a, r.clients[a].init(9, {9, 7}));
  auto fr = r.tp.frames_for(a, mk);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_SERVER_ERROR);
  EXPECT_EQ(r.server.connected_clients(), 2);
  EXPECT_EQ(r.status(), "testcluster: 4=ACK 2=ACK");
  EXPECT_TRUE(r.tp.closed.empty());
}

TEST(QnetdServer, HandshakeDeadlineIsNotExtendedByTraffic) {
  // a peer that never completes INIT must lose its slot after
  // PREACTIVE_TIMEOUT_MS no matter how often it sends something
  Rig r;
  int s = r.connect();
  r.feed(s, r.clients[s].preinit("testcluster"));
  for (int i = 0; i < 5; i++) {
    r.now += PREACTIVE_TIMEOUT_MS / 4;
    r.server.tick(r.now);
    uint8_t byte = 0;
    r.server.on_data(s, &byte, 1, r.now);  // a partial header, never a frame
  }
  EXPECT_TRUE(r.was_closed(s));
}

TEST(QnetdServer, OversizedClusterNameIsMalformed) {
  Rig r;
  int s = r.connect();
  std::string name(MAX_CLUSTER_NAME_LEN + 1, 'x');
  r.feed(s, r.clients[s].preinit(name.c_str()));
  auto fr = r.tp.frames_for(s);
  ASSERT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0].type, MsgType::MSG_TYPE_SERVER_ERROR);
  EXPECT_TRUE(r.was_closed(s));
}

TEST(QnetdServer, StatusAndStateChange) {
  Rig r;
  EXPECT_FALSE(r.server.consume_state_change());
  EXPECT_EQ(r.status(), "idle");
  int a, b;
  ack_two_nodes(r, a, b);
  EXPECT_TRUE(r.server.consume_state_change());
  EXPECT_FALSE(r.server.consume_state_change());
  EXPECT_EQ(r.status(), "testcluster: 4=ACK 2=ACK");
  EXPECT_GE(r.server.decisions(), 1u);
}

}  // namespace esphome::qnetd
