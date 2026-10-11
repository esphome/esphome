#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/logger/logger.h"
#include "esphome/components/noise/noise.h"
#include "esphome/components/noise/noise_stream.h"
#include "esphome/core/application.h"

namespace esphome::noise::testing {

// Sets the cached loop time the stream's handshake clock reads.
static void at(uint32_t now) { LoopBlockingGuard guard(nullptr, LOG_STR("test"), now); }

/// One end of an in-memory byte pipe with TcpClientLink's calls. flush_tx() hands at most
/// chunk_ bytes per call to the peer, so frames can arrive in pieces.
class MemLink {
 public:
  void pair(MemLink *peer) { this->peer_ = peer; }

  bool connected() const { return this->up_; }
  ssize_t read(uint8_t *buf, size_t len) {
    if (!this->up_) {
      return 0;
    }
    if (this->rx_.empty()) {
      if (this->peer_closed_) {
        this->up_ = false;
        return -1;
      }
      return 0;
    }
    size_t count = std::min(len, this->rx_.size());
    std::memcpy(buf, this->rx_.data(), count);
    this->rx_.erase(this->rx_.begin(), this->rx_.begin() + count);
    return static_cast<ssize_t>(count);
  }
  size_t tx_free() const { return this->up_ ? sizeof(this->tx_) - this->tx_len_ : 0; }
  uint8_t *tx_tail() { return this->tx_ + this->tx_len_; }
  void tx_commit(size_t len) { this->tx_len_ += len; }
  bool flush_tx() {
    size_t count = std::min(this->tx_len_, this->chunk_);
    if (count != 0 && this->up_) {
      this->sent_.insert(this->sent_.end(), this->tx_, this->tx_ + count);
      this->peer_->rx_.insert(this->peer_->rx_.end(), this->tx_, this->tx_ + count);
      this->tx_len_ -= count;
      std::memmove(this->tx_, this->tx_ + count, this->tx_len_);
    }
    return this->tx_len_ == 0;
  }
  void close() {
    this->up_ = false;
    this->tx_len_ = 0;
    this->peer_->peer_closed_ = true;
  }
  void note_attempt() {
    this->attempts_++;
    this->last_attempt_ms_ = App.get_loop_component_start_time();
  }
  void note_io() { this->io_notes_++; }
  uint32_t reconnect_interval() const { return this->interval_; }
  /// TcpClientLink::poll() connects again once this holds.
  bool may_retry() const { return App.get_loop_component_start_time() - this->last_attempt_ms_ >= this->interval_; }
  /// Stands in for the idle timeout: closes while idle_ is set.
  void check_idle() {
    if (this->idle_ && this->up_) {
      this->close();
    }
  }

  /// A fresh connection on both ends.
  void reconnect() {
    this->up_ = this->peer_->up_ = true;
    this->peer_closed_ = this->peer_->peer_closed_ = false;
    this->rx_.clear();
    this->peer_->rx_.clear();
    this->tx_len_ = this->peer_->tx_len_ = 0;
    this->sent_.clear();
    this->peer_->sent_.clear();
  }

  MemLink *peer_{nullptr};
  std::vector<uint8_t> rx_;
  // Everything this end put on the wire.
  std::vector<uint8_t> sent_;
  uint8_t tx_[1024]{};
  size_t tx_len_{0};
  size_t chunk_{SIZE_MAX};
  int attempts_{0};
  int io_notes_{0};
  uint32_t last_attempt_ms_{0};
  uint32_t interval_{5000};
  bool idle_{false};
  bool up_{true};
  bool peer_closed_{false};
};

/// Exposes the stream's plaintext limit per frame and its retry backoff.
class StreamProbe : public NoiseStream {
 public:
  using NoiseStream::NoiseStream;
  static constexpr size_t MAX_PLAIN_SIZE = MAX_PLAIN;
  static constexpr uint32_t BACKOFF_MAX = BACKOFF_MAX_MS;
  uint32_t next_wait(uint32_t interval) { return this->next_retry_wait_(interval); }
  /// When the hold set by the last failed handshake runs out.
  uint32_t hold_end() const { return this->started_ms_ + this->hold_ms_; }
};

#ifdef USE_LOG_LISTENERS
/// Everything logged since the last clear().
static std::string &captured_log() {
  static std::string text;
  static bool registered = false;
  if (!registered) {
    registered = true;
    logger::global_logger->add_log_callback(nullptr, [](void *, uint8_t, const char *, const char *message,
                                                        size_t len) { text.append(message, len).push_back('\n'); });
  }
  return text;
}
#endif

static psk_t make_psk(uint8_t seed) {
  psk_t psk;
  for (size_t i = 0; i < psk.size(); i++) {
    psk[i] = static_cast<uint8_t>(seed + i);
  }
  return psk;
}

class NoiseStreamTest : public ::testing::Test {
 protected:
  void SetUp() override {
    at(1000);
    this->client_link_.pair(&this->server_link_);
    this->server_link_.pair(&this->client_link_);
  }
  void TearDown() override { at(0); }

  /// Runs both ends like two loops until both are up or nothing moves.
  void pump_(NoiseStream &client, NoiseStream &server, int passes = 20) {
    for (int i = 0; i < passes; i++) {
      bool client_up = client.up(this->client_link_);
      bool server_up = server.up(this->server_link_);
      this->client_link_.flush_tx();
      this->server_link_.flush_tx();
      if (client_up && server_up) {
        return;
      }
    }
  }

  /// One loop pass of both ends at now. TcpClientLink::poll() runs before up(), so the client's link
  /// connects first once its interval has passed since the last attempt.
  void pass_(NoiseStream &client, NoiseStream &server, uint32_t now) {
    at(now);
    if (!this->client_link_.connected() && this->client_link_.may_retry()) {
      this->client_link_.reconnect();
      this->connects_++;
    }
    client.up(this->client_link_);
    server.up(this->server_link_);
    this->client_link_.flush_tx();
    this->server_link_.flush_tx();
  }

  /// Runs the client's loop while its link is down; returns when the link would connect again.
  uint32_t next_attempt_(NoiseStream &client, uint32_t now) {
    for (uint32_t end = now + 2 * StreamProbe::BACKOFF_MAX; now < end; now += 16) {
      at(now);
      if (this->client_link_.may_retry()) {
        break;
      }
      client.up(this->client_link_);
    }
    return now;
  }

  std::vector<uint8_t> read_all_(NoiseStream &stream, MemLink &link) {
    std::vector<uint8_t> out;
    uint8_t buf[256];
    for (;;) {
      ssize_t count = stream.read(link, buf, sizeof(buf));
      if (count <= 0) {
        return out;
      }
      out.insert(out.end(), buf, buf + count);
    }
  }

  psk_t key_{make_psk(1)};
  psk_t other_key_{make_psk(2)};
  MemLink client_link_;
  MemLink server_link_;
  // Connections pass_() opened
  int connects_{0};
};

TEST_F(NoiseStreamTest, HandshakeAndRoundTrip) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(client.ready());
  ASSERT_TRUE(server.ready());

  const uint8_t hello[] = {'h', 'e', 'l', 'l', 'o', 0x00, 0xFF};
  ASSERT_EQ(client.queue(this->client_link_, hello, sizeof(hello)), sizeof(hello));
  ASSERT_TRUE(client.flush(this->client_link_));
  // The wire never carries the plaintext
  auto &wire = this->client_link_.sent_;
  EXPECT_EQ(std::search(wire.begin(), wire.end(), hello, hello + 5), wire.end());
  EXPECT_EQ(this->read_all_(server, this->server_link_), std::vector<uint8_t>(hello, hello + sizeof(hello)));

  const uint8_t reply[] = {1, 2, 3};
  ASSERT_EQ(server.queue(this->server_link_, reply, sizeof(reply)), sizeof(reply));
  ASSERT_TRUE(server.flush(this->server_link_));
  EXPECT_EQ(this->read_all_(client, this->client_link_), std::vector<uint8_t>(reply, reply + sizeof(reply)));
}

TEST_F(NoiseStreamTest, NothingMovesBeforeTheSessionIsUp) {
  NoiseStream client(this->key_.data(), true);
  const uint8_t data[] = {1};
  EXPECT_EQ(client.queue(this->client_link_, data, sizeof(data)), 0u);
  EXPECT_EQ(client.tx_free(this->client_link_), 0u);
}

TEST_F(NoiseStreamTest, WrongKeyIsRejectedAndBothSidesClose) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->other_key_.data(), false);
  this->pump_(client, server);
  EXPECT_FALSE(client.ready());
  EXPECT_FALSE(server.ready());
  EXPECT_FALSE(this->server_link_.connected());
  EXPECT_EQ(this->server_link_.attempts_, 1);
  // The responder told the initiator why before it closed
  const char reason[] = "Handshake MAC failure";
  auto &wire = this->server_link_.sent_;
  ASSERT_GE(wire.size(), FRAME_HEADER_SIZE + 1);
  EXPECT_EQ(wire[FRAME_HEADER_SIZE], HANDSHAKE_STATUS_REJECT);
  EXPECT_NE(std::search(wire.begin(), wire.end(), reason, reason + sizeof(reason) - 1), wire.end());
  EXPECT_FALSE(this->client_link_.connected());
}

TEST_F(NoiseStreamTest, FramesInPiecesStillDecrypt) {
  this->client_link_.chunk_ = 1;
  this->server_link_.chunk_ = 2;
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server, 400);
  ASSERT_TRUE(client.ready());
  ASSERT_TRUE(server.ready());

  uint8_t data[300];
  for (size_t i = 0; i < sizeof(data); i++) {
    data[i] = static_cast<uint8_t>(i * 7);
  }
  ASSERT_EQ(client.queue(this->client_link_, data, sizeof(data)), sizeof(data));
  std::vector<uint8_t> got;
  for (int i = 0; i < 1000 && got.size() < sizeof(data); i++) {
    client.flush(this->client_link_);
    auto part = this->read_all_(server, this->server_link_);
    got.insert(got.end(), part.begin(), part.end());
  }
  EXPECT_EQ(got, std::vector<uint8_t>(data, data + sizeof(data)));
}

TEST_F(NoiseStreamTest, SmallWritesShareOneFrame) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(client.ready());
  this->client_link_.sent_.clear();

  for (uint8_t i = 0; i < 8; i++) {
    ASSERT_EQ(client.queue(this->client_link_, &i, 1), 1u);
  }
  ASSERT_TRUE(client.flush(this->client_link_));
  EXPECT_EQ(this->client_link_.sent_.size(), FRAME_HEADER_SIZE + 8 + MAC_SIZE);
  EXPECT_EQ(this->read_all_(server, this->server_link_), (std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST_F(NoiseStreamTest, LargeWritesAreSplitAndLimitedByTheLinkBuffer) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(client.ready());
  this->client_link_.sent_.clear();

  std::vector<uint8_t> data(2000);
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = static_cast<uint8_t>(i);
  }
  size_t room = client.tx_free(this->client_link_);
  EXPECT_GT(room, 0u);
  size_t taken = client.queue(this->client_link_, data.data(), data.size());
  // Two full frames and part of a third fit the 1024-byte buffer; every frame carries its own MAC
  EXPECT_GT(taken, room);
  EXPECT_LT(taken, data.size());
  EXPECT_LE(this->client_link_.tx_len_, sizeof(this->client_link_.tx_));
  ASSERT_TRUE(client.flush(this->client_link_));
  // The flush sealed the last frame; what fits next starts a new one
  taken += client.queue(this->client_link_, data.data() + taken, data.size() - taken);
  ASSERT_TRUE(client.flush(this->client_link_));
  std::vector<uint8_t> got = this->read_all_(server, this->server_link_);
  ASSERT_EQ(got.size(), taken);
  EXPECT_TRUE(std::equal(got.begin(), got.end(), data.begin()));
}

TEST_F(NoiseStreamTest, TxFreeIsWhatQueueTakes) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(client.ready());
  std::vector<uint8_t> data(2000, 0x5A);
  for (int i = 0; i < 4; i++) {
    size_t room = client.tx_free(this->client_link_);
    EXPECT_EQ(client.queue(this->client_link_, data.data(), room), room);
  }
}

TEST_F(NoiseStreamTest, TamperedFrameClosesTheLink) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(server.ready());
  const uint8_t data[] = {9, 9, 9};
  client.queue(this->client_link_, data, sizeof(data));
  ASSERT_TRUE(client.flush(this->client_link_));
  // One ciphertext byte flipped on the way
  ASSERT_GT(this->server_link_.rx_.size(), FRAME_HEADER_SIZE);
  this->server_link_.rx_[FRAME_HEADER_SIZE] ^= 0x01;
  uint8_t buf[16];
  EXPECT_EQ(server.read(this->server_link_, buf, sizeof(buf)), -1);
  EXPECT_FALSE(server.ready());
  EXPECT_FALSE(this->server_link_.connected());
}

TEST_F(NoiseStreamTest, PlaintextPeerIsDropped) {
  NoiseStream server(this->key_.data(), false);
  const uint8_t text[] = {'A', 'T', '\r', '\n'};
  this->server_link_.rx_.assign(text, text + sizeof(text));
  EXPECT_FALSE(server.up(this->server_link_));
  EXPECT_FALSE(this->server_link_.connected());
  EXPECT_EQ(this->server_link_.attempts_, 1);
}

TEST_F(NoiseStreamTest, SilentPeerTimesOut) {
  NoiseStream server(this->key_.data(), false);
  EXPECT_FALSE(server.up(this->server_link_));
  at(1000 + 59999);
  EXPECT_FALSE(server.up(this->server_link_));
  EXPECT_TRUE(this->server_link_.connected());
  at(1000 + 60000);
  EXPECT_FALSE(server.up(this->server_link_));
  EXPECT_FALSE(this->server_link_.connected());
}

#ifdef USE_LOG_LISTENERS
TEST_F(NoiseStreamTest, AResponderDoesNotLogThePeersBytes) {
  NoiseStream server(this->key_.data(), false);
  // A reject frame with text from a host that dialed in
  const uint8_t text[] = {'E', 'V', 'I', 'L', '\n', 0x1B, '[', '2', 'J'};
  this->server_link_.rx_ = {FRAME_INDICATOR, 0, static_cast<uint8_t>(sizeof(text) + 1), HANDSHAKE_STATUS_REJECT};
  this->server_link_.rx_.insert(this->server_link_.rx_.end(), text, text + sizeof(text));
  captured_log().clear();
  EXPECT_FALSE(server.up(this->server_link_));
  EXPECT_FALSE(this->server_link_.connected());
  EXPECT_EQ(this->server_link_.attempts_, 1);
  EXPECT_NE(captured_log().find("Bad handshake status"), std::string::npos) << captured_log();
  EXPECT_EQ(captured_log().find("EVIL"), std::string::npos) << captured_log();
  // An initiator logs the reason its responder sent
  NoiseStream client(this->key_.data(), true);
  NoiseStream other(this->other_key_.data(), false);
  this->server_link_.reconnect();
  captured_log().clear();
  this->pump_(client, other);
  EXPECT_NE(captured_log().find("Peer rejected the handshake: Handshake MAC failure"), std::string::npos)
      << captured_log();
}
#endif

TEST_F(NoiseStreamTest, AFirstMessageTheSocketHeldBackIsSentLater) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  // The socket takes nothing on the first try
  this->client_link_.chunk_ = 0;
  EXPECT_FALSE(client.up(this->client_link_));
  EXPECT_GT(this->client_link_.tx_len_, 0u);
  this->client_link_.chunk_ = SIZE_MAX;
  // No flush from outside: an owner does not flush while the session is down
  for (int i = 0; i < 4 && !(client.ready() && server.ready()); i++) {
    client.up(this->client_link_);
    server.up(this->server_link_);
  }
  EXPECT_TRUE(client.ready());
  EXPECT_TRUE(server.ready());
}

TEST_F(NoiseStreamTest, AFlushAfterTheLinkClosedDropsTheOpenFrame) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(client.ready());
  const uint8_t data[] = {1, 2, 3};
  ASSERT_EQ(client.queue(this->client_link_, data, sizeof(data)), sizeof(data));
  // The owner closes the link without resetting the stream
  this->client_link_.close();
  EXPECT_FALSE(client.flush(this->client_link_));
  EXPECT_EQ(this->client_link_.tx_len_, 0u);
  EXPECT_FALSE(client.ready());
}

TEST_F(NoiseStreamTest, PlaintextHandedOutLaterCountsAsTraffic) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(server.ready());
  uint8_t data[100]{};
  ASSERT_EQ(client.queue(this->client_link_, data, sizeof(data)), sizeof(data));
  ASSERT_TRUE(client.flush(this->client_link_));
  uint8_t buf[10];
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(server.read(this->server_link_, buf, sizeof(buf)), static_cast<ssize_t>(sizeof(buf)));
    // The first call took the whole frame off the link
    ASSERT_TRUE(this->server_link_.rx_.empty());
  }
  EXPECT_EQ(this->server_link_.io_notes_, 10);
}

TEST_F(NoiseStreamTest, AWaitingHandshakeHonoursTheLinkIdleTimeout) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  // Part of a frame header, then the peer goes quiet
  this->server_link_.rx_ = {FRAME_INDICATOR};
  EXPECT_FALSE(server.up(this->server_link_));
  EXPECT_TRUE(this->server_link_.connected());
  this->server_link_.idle_ = true;
  EXPECT_FALSE(server.up(this->server_link_));
  EXPECT_FALSE(this->server_link_.connected());
  // The next connection starts a fresh handshake, without the stale byte
  this->server_link_.idle_ = false;
  this->server_link_.reconnect();
  this->pump_(client, server);
  EXPECT_TRUE(client.ready());
  EXPECT_TRUE(server.ready());
}

TEST_F(NoiseStreamTest, AHandshakeFrameOutsideTheLimitsIsDropped) {
  // One byte over a status byte and the largest message, then an empty frame; only the header arrives
  constexpr size_t over = 1 + MAX_HANDSHAKE_SIZE + 1;
  for (size_t payload : {over, size_t{0}}) {
    this->server_link_.reconnect();
    this->server_link_.attempts_ = 0;
    NoiseStream server(this->key_.data(), false);
    this->server_link_.rx_ = {FRAME_INDICATOR, static_cast<uint8_t>(payload >> 8), static_cast<uint8_t>(payload)};
    EXPECT_FALSE(server.up(this->server_link_));
    EXPECT_FALSE(this->server_link_.connected());
    EXPECT_EQ(this->server_link_.attempts_, 1);
  }
}

TEST_F(NoiseStreamTest, ASessionFrameOverTheLimitIsDropped) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(server.ready());
  // One byte over the largest sealed frame; only the header arrives
  constexpr size_t over = StreamProbe::MAX_PLAIN_SIZE + MAC_SIZE + 1;
  this->server_link_.rx_ = {FRAME_INDICATOR, static_cast<uint8_t>(over >> 8), static_cast<uint8_t>(over)};
  uint8_t buf[16];
  EXPECT_EQ(server.read(this->server_link_, buf, sizeof(buf)), -1);
  EXPECT_FALSE(server.ready());
  EXPECT_FALSE(this->server_link_.connected());
}

TEST_F(NoiseStreamTest, TheHandshakeTimeoutCountsFromTheStart) {
  NoiseStream server(this->key_.data(), false);
  EXPECT_FALSE(server.up(this->server_link_));
  // A byte now and then does not extend the handshake
  at(30000);
  this->server_link_.rx_.push_back(FRAME_INDICATOR);
  EXPECT_FALSE(server.up(this->server_link_));
  at(60000);
  this->server_link_.rx_.push_back(0x00);
  EXPECT_FALSE(server.up(this->server_link_));
  EXPECT_TRUE(this->server_link_.connected());
  at(61000);
  EXPECT_FALSE(server.up(this->server_link_));
  EXPECT_FALSE(this->server_link_.connected());
}

TEST_F(NoiseStreamTest, TheRetryWaitDoublesUpToTheCapWithJitter) {
  StreamProbe client(this->key_.data(), true);
  uint32_t backoff = 5000;
  for (int i = 0; i < 10; i++) {
    uint32_t wait = client.next_wait(5000);
    EXPECT_GE(wait, backoff - backoff / 5) << i;
    EXPECT_LE(wait, backoff + backoff / 5) << i;
    backoff = std::min(backoff * 2, StreamProbe::BACKOFF_MAX);
  }
  // At the cap the jitter spreads both ways
  uint32_t low = UINT32_MAX;
  uint32_t high = 0;
  for (int i = 0; i < 200; i++) {
    uint32_t wait = client.next_wait(5000);
    low = std::min(low, wait);
    high = std::max(high, wait);
  }
  EXPECT_GE(low, StreamProbe::BACKOFF_MAX - StreamProbe::BACKOFF_MAX / 5);
  EXPECT_LT(low, StreamProbe::BACKOFF_MAX);
  EXPECT_GT(high, StreamProbe::BACKOFF_MAX);
  EXPECT_LE(high, StreamProbe::BACKOFF_MAX + StreamProbe::BACKOFF_MAX / 5);
  // A configured interval over the cap is never shortened
  StreamProbe slow(this->key_.data(), true);
  for (int i = 0; i < 3; i++) {
    EXPECT_GE(slow.next_wait(400000), 320000u);
  }
}

TEST_F(NoiseStreamTest, FailedHandshakesHoldTheClientLonger) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream other(this->other_key_.data(), false);
  NoiseStream server(this->key_.data(), false);
  uint32_t now = 1000;
  // The link's own interval is the shortest wait, then each failure doubles it with +/-20%
  for (uint32_t backoff : {5000u, 10000u, 20000u}) {
    this->client_link_.reconnect();
    this->pump_(client, other);
    ASSERT_FALSE(this->client_link_.connected());
    uint32_t wait = this->next_attempt_(client, now) - now;
    // One loop pass of rounding either way
    EXPECT_GE(wait + 16, std::max(backoff - backoff / 5, 5000u)) << backoff;
    EXPECT_LE(wait, backoff + backoff / 5 + 16) << backoff;
    now += wait;
  }
  // A good handshake starts the backoff over
  this->client_link_.reconnect();
  this->pump_(client, server);
  ASSERT_TRUE(client.ready());
  this->client_link_.close();
  client.up(this->client_link_);
  this->client_link_.reconnect();
  this->pump_(client, other);
  ASSERT_FALSE(this->client_link_.connected());
  EXPECT_LE(this->next_attempt_(client, now) - now, 6000u + 16);
  // A responder that failed waits only its link's interval
  int attempts = this->server_link_.attempts_;
  for (int i = 0; i < 10; i++) {
    at(now + i * 16);
    other.up(this->server_link_);
  }
  EXPECT_EQ(this->server_link_.attempts_, attempts);
}

TEST_F(NoiseStreamTest, AConnectionDuringTheHoldGetsNoHandshake) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream other(this->other_key_.data(), false);
  // Shorter than a loop pass, so the link connects again in every pass, also while the stream holds
  this->client_link_.interval_ = 10;
  for (uint32_t now = 1000; now < 4000; now += 16) {
    this->pass_(client, other, now);
  }
  // Each failure's wait is at least 80% of 10 ms doubled per failure, so 3 s leave room for 8 or 9 failed
  // handshakes; the responder notes an attempt for each
  EXPECT_GE(this->server_link_.attempts_, 8);
  EXPECT_LE(this->server_link_.attempts_, 9);
  // The link connected in most passes; those inside a hold were closed without a handshake
  EXPECT_GT(this->connects_, 10 * this->server_link_.attempts_);
}

TEST_F(NoiseStreamTest, AGoodHandshakeEndsTheHold) {
  StreamProbe client(this->key_.data(), true);
  NoiseStream other(this->other_key_.data(), false);
  NoiseStream server(this->key_.data(), false);
  this->client_link_.interval_ = 100;
  uint32_t now = 1000;
  // Six failed handshakes, each after the previous hold; the last holds for at least 2460 ms past the interval
  for (int i = 0; i < 6; i++) {
    if (i != 0) {
      now = this->next_attempt_(client, now);
    }
    at(now);
    this->client_link_.reconnect();
    this->pump_(client, other);
    ASSERT_FALSE(this->client_link_.connected());
  }
  uint32_t end = client.hold_end();
  ASSERT_GE(end, now + 2460);

  // One loop pass longer than the interval: the link connects inside the hold and is closed unused
  now += 150;
  at(now);
  ASSERT_TRUE(this->client_link_.may_retry());
  this->client_link_.reconnect();
  EXPECT_FALSE(client.up(this->client_link_));
  EXPECT_FALSE(this->client_link_.connected());
  EXPECT_TRUE(this->client_link_.sent_.empty());

  // Passes up to the last one inside the hold, then one longer than the interval that ends after it: the link
  // connects before up() has seen the hold run out
  for (now += 16; now + 16 < end; now += 16) {
    at(now);
    client.up(this->client_link_);
  }
  at(now);
  client.up(this->client_link_);
  now += 150;
  at(now);
  ASSERT_GE(now, end);
  ASSERT_TRUE(this->client_link_.may_retry());
  this->client_link_.reconnect();
  this->pump_(client, server);
  ASSERT_TRUE(client.ready());

  // A drop one second into the session waits only the link's interval
  now += 1000;
  at(now);
  this->client_link_.close();
  this->client_link_.note_attempt();
  EXPECT_LE(this->next_attempt_(client, now) - now, 100u + 16);
}

TEST_F(NoiseStreamTest, ADroppedLinkStartsANewSession) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(client.ready());
  const uint8_t data[] = {1, 2};
  client.queue(this->client_link_, data, sizeof(data));

  this->client_link_.close();
  EXPECT_FALSE(client.up(this->client_link_));
  EXPECT_FALSE(client.ready());
  uint8_t buf[4];
  EXPECT_EQ(server.read(this->server_link_, buf, sizeof(buf)), -1);
  EXPECT_FALSE(server.ready());

  this->client_link_.reconnect();
  this->pump_(client, server);
  ASSERT_TRUE(client.ready());
  ASSERT_TRUE(server.ready());
  ASSERT_EQ(client.queue(this->client_link_, data, sizeof(data)), sizeof(data));
  ASSERT_TRUE(client.flush(this->client_link_));
  EXPECT_EQ(this->read_all_(server, this->server_link_), (std::vector<uint8_t>{1, 2}));
}

TEST_F(NoiseStreamTest, ApiPrologueDoesNotMatch) {
  // A responder with another prologue (the api's) never completes a stream handshake
  NoiseStream client(this->key_.data(), true);
  NoiseContext ctx;
  ctx.set_psk(this->key_.data());
  NoiseResponderHandshake api;
  const uint8_t api_prologue[] = {'N', 'o', 'i', 's', 'e', 'A', 'P', 'I', 'I', 'n', 'i', 't'};
  ASSERT_EQ(api.init(ctx, api_prologue, sizeof(api_prologue)), 0);
  EXPECT_FALSE(client.up(this->client_link_));
  this->client_link_.flush_tx();
  auto &rx = this->server_link_.rx_;
  ASSERT_GT(rx.size(), FRAME_HEADER_SIZE + 1);
  EXPECT_NE(api.read_message(rx.data() + FRAME_HEADER_SIZE + 1, rx.size() - FRAME_HEADER_SIZE - 1), 0);
}

#ifdef USE_NOISE_SPARE_EPHEMERAL
TEST_F(NoiseStreamTest, OnlyTheResponderTakesTheSpareKey) {
  prepare_spare_ephemeral();
  ASSERT_TRUE(has_spare_ephemeral());
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  client.up(this->client_link_);
  EXPECT_TRUE(has_spare_ephemeral());
  this->client_link_.flush_tx();
  this->pump_(client, server);
  ASSERT_TRUE(server.ready());
  EXPECT_FALSE(has_spare_ephemeral());
}

TEST_F(NoiseStreamTest, AnIdleResponderPreparesTheSpareKey) {
  NoiseStream client(this->key_.data(), true);
  NoiseStream server(this->key_.data(), false);
  this->pump_(client, server);
  ASSERT_TRUE(server.ready());
  ASSERT_FALSE(has_spare_ephemeral());
  // A session in progress leaves the slot alone
  EXPECT_TRUE(server.up(this->server_link_));
  EXPECT_FALSE(has_spare_ephemeral());
  this->server_link_.close();
  EXPECT_FALSE(server.up(this->server_link_));
#ifdef USE_API
  // The api server refills it under its connect grace
  EXPECT_FALSE(has_spare_ephemeral());
#else
  EXPECT_TRUE(has_spare_ephemeral());
#endif
  // An idle initiator does not
  std::memset(spare_ephemeral, 0, sizeof(spare_ephemeral));
  this->client_link_.close();
  EXPECT_FALSE(client.up(this->client_link_));
  EXPECT_FALSE(has_spare_ephemeral());
}
#endif

}  // namespace esphome::noise::testing
