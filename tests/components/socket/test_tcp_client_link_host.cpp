#include <gtest/gtest.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/socket/tcp_client_link.h"
#include "esphome/core/application.h"

#ifdef USE_HOST

// Host only: ESP-IDF has no poll.h.
#include <poll.h>

namespace esphome::socket::testing {

class LinkPeer {
 public:
  LinkPeer() {
    // EPIPE must come back as an errno, not a signal.
    signal(SIGPIPE, SIG_IGN);
    int fds[2];
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    this->peer_fd_ = fds[1];
    this->link_.set_host("peer");
    this->link_.set_port(1);
    this->link_.begin("link_test");
    this->link_.adopt(std::make_unique<Socket>(fds[0]));
  }
  ~LinkPeer() {
    if (this->peer_fd_ >= 0) {
      ::close(this->peer_fd_);
    }
    this->link_.close();
  }
  void close_peer() {
    ::close(this->peer_fd_);
    this->peer_fd_ = -1;
  }

  TcpClientLink link_;
  int peer_fd_{-1};
};

// Sets the cached loop time the link's clock reads, as the main loop does.
static void set_loop_time(uint32_t now) { LoopBlockingGuard guard(nullptr, LOG_STR("test"), now); }

class LinkUnderTest : public TcpClientLink {
 public:
  void set_socket(std::unique_ptr<Socket> sock) { this->sock_ = std::move(sock); }
  bool has_socket() const { return this->sock_ != nullptr; }
  int fd() const { return this->sock_->get_fd(); }
};

class TcpClientLinkClock : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    set_loop_time(1000);
  }
  void TearDown() override {
    this->link_.close();
    if (this->peer_fd_ >= 0) {
      ::close(this->peer_fd_);
    }
    set_loop_time(0);
  }
  // A connected socket whose send buffer is full selects as not writable,
  // which poll_connect() reports as a connect still in progress.
  void set_pending_socket() {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    this->peer_fd_ = fds[1];
    ASSERT_EQ(fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK), 0);
    char fill[1024]{};
    while (::write(fds[0], fill, sizeof(fill)) > 0) {
    }
    ASSERT_EQ(errno, EAGAIN);
    this->link_.set_socket(std::make_unique<Socket>(fds[0]));
  }

  LinkUnderTest link_;
  int peer_fd_{-1};
};

TEST_F(TcpClientLinkClock, PendingConnectTimesOutIntoBackoff) {
  this->link_.begin("link_test");
  this->set_pending_socket();
  this->link_.note_attempt();
  set_loop_time(1000 + 9999);
  this->link_.poll();
  ASSERT_TRUE(this->link_.has_socket());
  EXPECT_FALSE(this->link_.connected());
  set_loop_time(1000 + 10000);
  this->link_.poll();
  EXPECT_FALSE(this->link_.has_socket());
  EXPECT_TRUE(this->link_.in_backoff());
}

TEST_F(TcpClientLinkClock, ConnectTimeoutFollowsALongerInterval) {
  this->link_.set_reconnect_interval(20000);
  this->link_.begin("link_test");
  this->set_pending_socket();
  this->link_.note_attempt();
  set_loop_time(1000 + 19999);
  this->link_.poll();
  ASSERT_TRUE(this->link_.has_socket());
  set_loop_time(1000 + 20000);
  this->link_.poll();
  EXPECT_FALSE(this->link_.has_socket());
}

TEST_F(TcpClientLinkClock, ResolveFailureBacksOff) {
  // An IPv6 literal fails the IPv4 lookup without DNS.
  this->link_.set_host("::1");
  this->link_.set_port(1);
  this->link_.begin("link_test");
  this->link_.poll();
  ASSERT_FALSE(this->link_.in_backoff());
  // The failure is consumed by the next attempt and restarts the clock.
  this->link_.poll();
  EXPECT_FALSE(this->link_.has_socket());
  EXPECT_TRUE(this->link_.in_backoff());
  set_loop_time(1000 + 4999);
  EXPECT_TRUE(this->link_.in_backoff());
  set_loop_time(1000 + 5000);
  EXPECT_FALSE(this->link_.in_backoff());
}

TEST_F(TcpClientLinkClock, RefusedConnectBacksOff) {
  // A loopback port that was just free refuses the connect.
  int probe = ::socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_GE(probe, 0);
  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  socklen_t len = sizeof(addr);
  ASSERT_EQ(::bind(probe, reinterpret_cast<struct sockaddr *>(&addr), len), 0);
  ASSERT_EQ(::getsockname(probe, reinterpret_cast<struct sockaddr *>(&addr), &len), 0);
  ::close(probe);

  this->link_.set_host("127.0.0.1");
  this->link_.set_port(ntohs(addr.sin_port));
  this->link_.begin("link_test");
  // The refusal comes back from connect() itself or from a later poll.
  this->link_.poll();
  if (this->link_.has_socket()) {
    // Wait for the stack to finish the connect, then let poll() read the result.
    struct pollfd pfd {
      this->link_.fd(), POLLOUT, 0
    };
    ASSERT_EQ(::poll(&pfd, 1, 1000), 1);
    this->link_.poll();
  }
  EXPECT_FALSE(this->link_.connected());
  EXPECT_FALSE(this->link_.has_socket());
  EXPECT_TRUE(this->link_.in_backoff());
}

TEST_F(TcpClientLinkClock, BackoffSpansAMillisWrap) {
  set_loop_time(UINT32_MAX - 999);
  this->link_.begin("link_test");
  // begin() back-dates the clock so the first attempt is immediate.
  EXPECT_FALSE(this->link_.in_backoff());
  this->link_.note_attempt();
  set_loop_time(3999);
  EXPECT_TRUE(this->link_.in_backoff());
  set_loop_time(4000);
  EXPECT_FALSE(this->link_.in_backoff());
}

TEST(TcpClientLink, AdoptedSocketFlushesQueuedBytes) {
  LinkPeer p;
  ASSERT_TRUE(p.link_.connected());
  EXPECT_EQ(p.link_.queue(reinterpret_cast<const uint8_t *>("ping"), 4), 4u);
  EXPECT_TRUE(p.link_.flush_tx());
  char buf[8];
  EXPECT_EQ(::read(p.peer_fd_, buf, sizeof(buf)), 4);
  EXPECT_EQ(std::memcmp(buf, "ping", 4), 0);
}

TEST(TcpClientLink, CloseClearsQueuedBytes) {
  LinkPeer p;
  EXPECT_EQ(p.link_.queue(reinterpret_cast<const uint8_t *>("MARKER"), 6), 6u);
  p.link_.close();
  EXPECT_FALSE(p.link_.connected());
  EXPECT_EQ(p.link_.tx_free(), 0u);
  // An uncleared buffer would make flush_tx() report it as still pending.
  EXPECT_TRUE(p.link_.flush_tx());
}

TEST(TcpClientLink, FatalWriteInsideFlushDropsTheLink) {
  LinkPeer p;
  EXPECT_EQ(p.link_.queue(reinterpret_cast<const uint8_t *>("MARKER"), 6), 6u);
  p.close_peer();
  // Still connected from the link's point of view: the drop must happen
  // inside this flush, the exact ordering TcpUart::flush() reports FAILED.
  ASSERT_TRUE(p.link_.connected());
  bool emptied = p.link_.flush_tx();
  EXPECT_TRUE(emptied);
  EXPECT_FALSE(p.link_.connected());
}

}  // namespace esphome::socket::testing

#endif
