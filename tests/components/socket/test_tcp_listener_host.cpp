#include <gtest/gtest.h>

#include <fcntl.h>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/socket/tcp_listener.h"

#ifdef USE_HOST

// Host only: ESP-IDF has no poll.h.
#include <poll.h>

namespace esphome::socket::testing {

class ListenerUnderTest : public TcpListener {
 public:
  void set_listen(std::unique_ptr<ListenSocket> sock) { this->listen_ = std::move(sock); }
  bool listening() const { return this->listen_ != nullptr; }
  void accept(TcpClientLink &link) { this->accept_(link); }
};

class TcpListenerAccept : public ::testing::Test {
 protected:
  void SetUp() override {
    this->link_.set_port(1);
    this->link_.begin("listener_test");
    this->listener_.begin("listener_test");
  }
  void TearDown() override {
    this->listener_.close();
    this->link_.close();
    if (this->client_fd_ >= 0) {
      ::close(this->client_fd_);
    }
  }
  // A non-blocking IPv4 listener on a free loopback port; returns the port.
  uint16_t listen_on_loopback() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(fd, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof(addr);
    EXPECT_EQ(::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), len), 0);
    EXPECT_EQ(::listen(fd, 1), 0);
    EXPECT_EQ(::getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr), &len), 0);
    EXPECT_EQ(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK), 0);
    this->listen_fd_ = fd;
    this->listener_.set_listen(std::make_unique<ListenSocket>(fd));
    return ntohs(addr.sin_port);
  }
  // A blocking connect to the loopback listener; it completes into the backlog.
  void connect_client(uint16_t port) {
    this->client_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(this->client_fd_, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    ASSERT_EQ(::connect(this->client_fd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
    // The stack may queue the connection for accept() a moment after connect() returns.
    struct pollfd pfd {
      this->listen_fd_, POLLIN, 0
    };
    ASSERT_EQ(::poll(&pfd, 1, 1000), 1);
  }

  TcpClientLink link_;
  ListenerUnderTest listener_;
  int listen_fd_{-1};
  int client_fd_{-1};
};

TEST_F(TcpListenerAccept, AcceptErrorRebuildsAfterTheBackoff) {
  // accept() on a bad descriptor fails with EBADF, which no retry fixes.
  this->listener_.set_listen(std::make_unique<ListenSocket>(-1));
  this->listener_.accept(this->link_);
  EXPECT_FALSE(this->listener_.listening());
  EXPECT_TRUE(this->link_.in_backoff());
  // No new listen socket until the backoff has passed.
  this->listener_.poll(this->link_, true);
  EXPECT_FALSE(this->listener_.listening());
}

TEST_F(TcpListenerAccept, NothingPendingKeepsTheListener) {
  this->listen_on_loopback();
  this->listener_.accept(this->link_);
  EXPECT_TRUE(this->listener_.listening());
  EXPECT_FALSE(this->link_.connected());
  EXPECT_FALSE(this->link_.in_backoff());
}

TEST_F(TcpListenerAccept, AcceptedClientIsAdopted) {
  this->connect_client(this->listen_on_loopback());
  this->listener_.accept(this->link_);
  EXPECT_TRUE(this->link_.connected());
  EXPECT_TRUE(this->listener_.listening());
}

#ifdef USE_SOCKET_IPV4_ALLOW
TEST_F(TcpListenerAccept, PeerOutsideTheAllowListIsClosed) {
  static const Ipv4AllowEntry ONLY_TEN[] = {{htonl(0x0A000000), htonl(0xFF000000)}};
  this->listener_.set_allow(ONLY_TEN, 1);
  this->connect_client(this->listen_on_loopback());
  this->listener_.accept(this->link_);
  EXPECT_FALSE(this->link_.connected());
  EXPECT_TRUE(this->listener_.listening());
  EXPECT_FALSE(this->link_.in_backoff());
  // The peer sees the close.
  struct pollfd pfd {
    this->client_fd_, POLLIN, 0
  };
  ASSERT_EQ(::poll(&pfd, 1, 1000), 1);
  char b;
  EXPECT_EQ(::read(this->client_fd_, &b, 1), 0);
}

TEST_F(TcpListenerAccept, PeerInsideTheAllowListIsAdopted) {
  static const Ipv4AllowEntry LOOPBACK_ONLY[] = {{htonl(INADDR_LOOPBACK), htonl(0xFFFFFFFF)}};
  this->listener_.set_allow(LOOPBACK_ONLY, 1);
  this->connect_client(this->listen_on_loopback());
  this->listener_.accept(this->link_);
  EXPECT_TRUE(this->link_.connected());
}
#endif

}  // namespace esphome::socket::testing

#endif
