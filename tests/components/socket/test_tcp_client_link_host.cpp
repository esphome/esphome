#include <gtest/gtest.h>

#include <csignal>
#include <cstring>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/socket/tcp_client_link.h"

#ifdef USE_HOST

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
