#include <gtest/gtest.h>

#include <csignal>
#include <cstring>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/socket/tcp_client_link.h"
#include "esphome/core/application.h"

#ifdef USE_HOST

#include <sys/uio.h>

// Raw lwIP (ESP8266, RP2040) returns 0 when its send buffer takes no byte; a POSIX
// socket never does. In this test binary, write() on zero_write_fd does the same.
static int zero_write_fd = -1;

extern "C" ssize_t write(int fd, const void *buf, size_t len) {
  if (fd == zero_write_fd) {
    return 0;
  }
  struct iovec iov = {const_cast<void *>(buf), len};
  return ::writev(fd, &iov, 1);
}

namespace esphome::socket::testing {

class LinkPeer {
 public:
  LinkPeer() {
    // EPIPE must come back as an errno, not a signal.
    signal(SIGPIPE, SIG_IGN);
    int fds[2];
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    this->link_fd_ = fds[0];
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
  int link_fd_{-1};
  int peer_fd_{-1};
};

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

// Publish the loop start time the link reads, as Application::loop() does.
static void set_now(uint32_t now) { LoopBlockingGuard dispatch{nullptr, nullptr, now}; }

TEST(TcpClientLink, QuietLinkClosesAfterTheTimeout) {
  LinkPeer p;
  p.link_.set_idle_timeout(100);
  set_now(1000);
  p.link_.note_io();
  set_now(1099);
  p.link_.check_idle();
  EXPECT_TRUE(p.link_.connected());
  set_now(1100);
  p.link_.check_idle();
  EXPECT_FALSE(p.link_.connected());
}

TEST(TcpClientLink, AReadRestartsTheIdleClock) {
  LinkPeer p;
  p.link_.set_idle_timeout(100);
  set_now(1000);
  p.link_.note_io();
  set_now(1090);
  char byte = 'x';
  ASSERT_EQ(::write(p.peer_fd_, &byte, 1), 1);
  uint8_t buf[4];
  ASSERT_EQ(p.link_.read(buf, sizeof(buf)), 1);
  set_now(1189);
  p.link_.check_idle();
  EXPECT_TRUE(p.link_.connected());
  set_now(1190);
  p.link_.check_idle();
  EXPECT_FALSE(p.link_.connected());
}

TEST(TcpClientLink, AWriteRestartsTheIdleClock) {
  LinkPeer p;
  p.link_.set_idle_timeout(100);
  set_now(1000);
  p.link_.note_io();
  set_now(1090);
  ASSERT_EQ(p.link_.queue(reinterpret_cast<const uint8_t *>("x"), 1), 1u);
  EXPECT_TRUE(p.link_.flush_tx());
  set_now(1189);
  p.link_.check_idle();
  EXPECT_TRUE(p.link_.connected());
  set_now(1190);
  p.link_.check_idle();
  EXPECT_FALSE(p.link_.connected());
}

TEST(TcpClientLink, ZeroTimeoutLeavesAQuietLinkUp) {
  LinkPeer p;
  p.link_.set_idle_timeout(0);
  set_now(1000);
  p.link_.note_io();
  set_now(5000);
  p.link_.check_idle();
  EXPECT_TRUE(p.link_.connected());
}

TEST(TcpClientLink, NotingIoKeepsABlockedConsumerUp) {
  LinkPeer p;
  p.link_.set_idle_timeout(100);
  set_now(1000);
  p.link_.note_io();
  set_now(1090);
  // A full local buffer calls this instead of reading. The peer is not idle.
  p.link_.note_io();
  set_now(1189);
  p.link_.check_idle();
  EXPECT_TRUE(p.link_.connected());
}

TEST(TcpClientLink, StuckSendClosesAfterTheTimeout) {
  LinkPeer p;
  p.link_.set_idle_timeout(100);
  set_now(1000);
  // Fill the socket until the peer, which never reads, takes no more bytes.
  uint8_t block[256]{};
  bool emptied = true;
  for (int i = 0; i < 10000 && emptied; i++) {
    p.link_.queue(block, sizeof(block));
    emptied = p.link_.flush_tx();
  }
  ASSERT_FALSE(emptied);
  // A send refused with EAGAIN does not restart the clock.
  set_now(1050);
  EXPECT_FALSE(p.link_.flush_tx());
  set_now(1099);
  p.link_.check_idle();
  EXPECT_TRUE(p.link_.connected());
  set_now(1100);
  p.link_.check_idle();
  EXPECT_FALSE(p.link_.connected());
}

TEST(TcpClientLink, ASendThatTakesNothingKeepsTheIdleClock) {
  LinkPeer p;
  p.link_.set_idle_timeout(100);
  set_now(1000);
  p.link_.note_io();
  ASSERT_EQ(p.link_.queue(reinterpret_cast<const uint8_t *>("x"), 1), 1u);
  zero_write_fd = p.link_fd_;
  // The send returns 0: the byte stays queued, the link stays up, the clock keeps running.
  set_now(1050);
  bool emptied = p.link_.flush_tx();
  zero_write_fd = -1;
  EXPECT_FALSE(emptied);
  EXPECT_TRUE(p.link_.connected());
  set_now(1099);
  p.link_.check_idle();
  EXPECT_TRUE(p.link_.connected());
  set_now(1100);
  p.link_.check_idle();
  EXPECT_FALSE(p.link_.connected());
}

TEST(TcpClientLink, IdleCloseWaitsTheReconnectInterval) {
  LinkPeer p;
  p.link_.set_reconnect_interval(5000);
  p.link_.set_idle_timeout(100);
  set_now(1000);
  p.link_.note_io();
  set_now(1100);
  p.link_.check_idle();
  ASSERT_FALSE(p.link_.connected());
  EXPECT_TRUE(p.link_.in_backoff());
  set_now(6099);
  EXPECT_TRUE(p.link_.in_backoff());
  set_now(6100);
  EXPECT_FALSE(p.link_.in_backoff());
}

}  // namespace esphome::socket::testing

#endif
