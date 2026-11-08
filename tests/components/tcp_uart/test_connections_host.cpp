#include <gtest/gtest.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/core/application.h"
#include "esphome/core/wake/wake_host.h"

#ifdef USE_HOST
#ifdef USE_TCP_UART_MAX_CONNECTIONS

// Host only: ESP-IDF has no poll.h.
#include <poll.h>

namespace esphome::tcp_uart::testing {

class TcpUartConnectionsUnderTest : public TcpUart {
 public:
  uint8_t open_connections() const { return static_cast<uint8_t>(__builtin_popcount(this->up_mask_)); }
};

// A server with two connection slots and a 100 ms turn timeout. Clients A and B are real TCP
// sockets on loopback, connected at 1000 ms.
class TcpUartTwoClients : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    this->at(1000);
    this->port_ = free_port();
    this->uart_.set_server(true);
    this->uart_.set_port(this->port_);
    this->uart_.set_max_connections(this->slots_);
    this->uart_.set_turn_timeout(100);
    this->uart_.set_idle_timeout(this->idle_timeout_);
    this->uart_.set_disconnects_sensor(&this->disconnects_);
    this->uart_.setup();
    // Opens the listener.
    this->loops(1);
    this->a_ = this->connect_client();
    this->b_ = this->connect_client();
    ASSERT_EQ(this->uart_.open_connections(), 2);
  }
  void TearDown() override {
    for (int fd : {this->a_, this->b_, this->c_, this->d_}) {
      if (fd >= 0) {
        ::close(fd);
      }
    }
    this->uart_.on_shutdown();
  }

  static uint16_t free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    socklen_t len = sizeof(addr);
    ::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), len);
    ::getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr), &len);
    ::close(fd);
    return ntohs(addr.sin_port);
  }
  // Sets the loop clock.
  static void at(uint32_t ms) { LoopBlockingGuard{nullptr, nullptr, ms}; }
  // Each pass: select() marks the ready sockets, as the main loop does, then the UART runs.
  void loops(int count) {
    for (int i = 0; i < count; i++) {
      internal::wakeable_delay(5);
      this->uart_.loop();
    }
  }
  // Connects a client and runs the loop until the UART took it.
  int connect_client() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(this->port_);
    EXPECT_EQ(::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
    const uint8_t before = this->uart_.open_connections();
    for (int i = 0; i < 50 && this->uart_.open_connections() == before; i++) {
      this->loops(1);
    }
    return fd;
  }
  static void send(int fd, const char *text) {
    ASSERT_EQ(::write(fd, text, strlen(text)), static_cast<ssize_t>(strlen(text)));
  }
  // What the client received within 200 ms: "" for nothing, "<closed>" for EOF.
  static std::string received(int fd) {
    struct pollfd pfd{fd, POLLIN, 0};
    if (::poll(&pfd, 1, 200) != 1) {
      return "";
    }
    char buf[32];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    return n > 0 ? std::string(buf, n) : "<closed>";
  }
  // Runs the loop until the UART has count bytes to read.
  void wait_for_bytes(size_t count) {
    for (int i = 0; i < 50 && this->uart_.available() < count; i++) {
      this->loops(1);
    }
  }
  std::string read_all() {
    char buf[32]{};
    size_t n = this->uart_.available();
    EXPECT_TRUE(this->uart_.read_array(reinterpret_cast<uint8_t *>(buf), n));
    return std::string(buf, n);
  }
  void write(const char *text) { this->uart_.write_array(reinterpret_cast<const uint8_t *>(text), strlen(text)); }

  TcpUartConnectionsUnderTest uart_;
  sensor::Sensor disconnects_;
  uint16_t port_{0};
  uint8_t slots_{2};
  // 0 leaves quiet connections open.
  uint32_t idle_timeout_{0};
  int a_{-1};
  int b_{-1};
  int c_{-1};
  int d_{-1};
};

TEST_F(TcpUartTwoClients, ReplyGoesOnlyToTheClientThatAsked) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  EXPECT_EQ(this->read_all(), "ping");
  this->write("pong");
  this->loops(1);
  EXPECT_EQ(received(this->a_), "pong");
  EXPECT_EQ(received(this->b_), "");
}

TEST_F(TcpUartTwoClients, SecondClientWaitsUntilTheFirstIsQuietFor100ms) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  EXPECT_EQ(this->read_all(), "ping");
  const uint16_t turn_of_a = this->uart_.turn();

  send(this->b_, "hi");
  this->loops(3);
  EXPECT_EQ(this->uart_.available(), 0u);
  this->at(1099);
  this->loops(3);
  EXPECT_EQ(this->uart_.available(), 0u);

  this->at(1100);
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
  EXPECT_NE(this->uart_.turn(), turn_of_a);
  // The reply goes to the new holder.
  this->write("ok");
  this->loops(1);
  EXPECT_EQ(received(this->b_), "ok");
  EXPECT_EQ(received(this->a_), "");
}

TEST_F(TcpUartTwoClients, HeldTurnStaysUntilReleased) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  this->uart_.hold_turn();
  EXPECT_EQ(this->read_all(), "ping");

  send(this->b_, "hi");
  this->at(5000);
  this->loops(3);
  EXPECT_EQ(this->uart_.available(), 0u);

  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, ReleaseWaitsForTheBytesAlreadyRead) {
  // A sends two requests, "1" and "2", that arrive in one read; both come before B's.
  send(this->a_, "12");
  this->wait_for_bytes(2);
  send(this->b_, "hi");
  this->uart_.hold_turn();
  uint8_t request;
  ASSERT_TRUE(this->uart_.read_byte(&request));
  EXPECT_EQ(request, '1');
  this->uart_.release_turn();
  this->loops(3);

  // The consumer takes "2" and holds the turn again for its reply.
  this->uart_.hold_turn();
  ASSERT_TRUE(this->uart_.read_byte(&request));
  EXPECT_EQ(request, '2');
  this->at(5000);
  this->loops(3);
  EXPECT_EQ(this->uart_.available(), 0u);

  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, ReleasedTurnReadsNothingNewFromItsConnection) {
  // A sends "1" and "2" in one write, then B sends "hi".
  send(this->a_, "12");
  this->wait_for_bytes(2);
  send(this->b_, "hi");
  this->uart_.hold_turn();
  uint8_t request;
  ASSERT_TRUE(this->uart_.read_byte(&request));
  EXPECT_EQ(request, '1');
  this->uart_.release_turn();
  this->loops(3);

  // The consumer takes "2" and holds the turn for its reply; meanwhile A sends "3".
  this->uart_.hold_turn();
  ASSERT_TRUE(this->uart_.read_byte(&request));
  EXPECT_EQ(request, '2');
  send(this->a_, "3");
  // 20 ms for loopback to deliver "3". The loop passes alone take no time: B's "hi" keeps select() ready.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  this->loops(3);
  // "3" stays in A's socket for A's next turn.
  EXPECT_EQ(this->uart_.available(), 0u);

  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, RequestSplitAfterAReleaseKeepsItsTurn) {
  // Requests are two bytes. A writes "1." and "2." and the first byte of "3." at once, then B sends "hi".
  send(this->a_, "1.2.3");
  this->wait_for_bytes(5);
  send(this->b_, "hi");
  const uint16_t turn_of_a = this->uart_.turn();
  char request[3]{};
  for (const char *expected : {"1.", "2."}) {
    this->uart_.hold_turn();
    ASSERT_TRUE(this->uart_.read_array(reinterpret_cast<uint8_t *>(request), 2));
    EXPECT_STREQ(request, expected);
    this->uart_.release_turn();
    this->loops(3);
  }

  // The consumer takes "3" and names the one byte it lacks. A writes it 20 ms later, within the 100 ms turn timeout.
  uint8_t first_byte;
  ASSERT_TRUE(this->uart_.read_byte(&first_byte));
  EXPECT_EQ(first_byte, '3');
  this->uart_.release_turn(1);
  this->loops(3);
  this->at(1020);
  send(this->a_, ".");
  // Loopback needs a moment; B's "hi" keeps select() ready, so the loop passes alone take no time.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  this->loops(3);
  EXPECT_EQ(this->uart_.turn(), turn_of_a);
  EXPECT_EQ(this->read_all(), ".");

  // Request 3 is answered; then B has its turn.
  this->uart_.hold_turn();
  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, RestOfASplitRequestIsAllTheReleasedTurnReads) {
  // Requests are two bytes. A writes "1." and the first byte of "2.", then B sends "hi".
  send(this->a_, "1.2");
  this->wait_for_bytes(3);
  send(this->b_, "hi");
  char request[3]{};
  this->uart_.hold_turn();
  ASSERT_TRUE(this->uart_.read_array(reinterpret_cast<uint8_t *>(request), 2));
  EXPECT_STREQ(request, "1.");
  this->uart_.release_turn();
  this->loops(3);

  // The consumer takes "2" and names the one byte it lacks. A sends that byte and its next request "3." at once.
  uint8_t first_byte;
  ASSERT_TRUE(this->uart_.read_byte(&first_byte));
  EXPECT_EQ(first_byte, '2');
  this->uart_.release_turn(1);
  send(this->a_, ".3.");
  // Loopback needs a moment; B's "hi" keeps select() ready, so the loop passes alone take no time.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  this->loops(3);
  // Only the missing "." is read. "3." stays in A's socket.
  EXPECT_EQ(this->read_all(), ".");

  // Request 2 is answered. B has the next turn, and A's "3." comes in the turn after it.
  this->uart_.hold_turn();
  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
  this->uart_.hold_turn();
  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "3.");
}

TEST_F(TcpUartTwoClients, ExactRestLeavesNoEmptyTurn) {
  // Requests are two bytes. A writes "1." and the first byte of "2.", then B sends "hi".
  send(this->a_, "1.2");
  this->wait_for_bytes(3);
  send(this->b_, "hi");
  char request[3]{};
  this->uart_.hold_turn();
  ASSERT_TRUE(this->uart_.read_array(reinterpret_cast<uint8_t *>(request), 2));
  EXPECT_STREQ(request, "1.");
  this->uart_.release_turn();
  this->loops(3);

  // The consumer takes "2" and names the one byte it lacks. A sends that byte and nothing more.
  uint8_t first_byte;
  ASSERT_TRUE(this->uart_.read_byte(&first_byte));
  EXPECT_EQ(first_byte, '2');
  this->uart_.release_turn(1);
  send(this->a_, ".");
  // Loopback needs a moment; B's "hi" keeps select() ready, so the loop passes alone take no time.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  this->loops(3);
  EXPECT_EQ(this->read_all(), ".");

  // Request 2 is answered, then B's "hi".
  this->uart_.hold_turn();
  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");

  // B is answered and asks again. A has nothing left, so B does not wait the 100 ms turn timeout for A.
  this->uart_.hold_turn();
  this->uart_.release_turn();
  send(this->b_, "x");
  this->wait_for_bytes(1);
  EXPECT_EQ(this->read_all(), "x");
}

TEST_F(TcpUartTwoClients, ClosingDuringItsTurnDropsItsBytes) {
  send(this->a_, "abc");
  this->wait_for_bytes(3);
  const uint16_t turn_of_a = this->uart_.turn();
  ::close(this->a_);
  this->a_ = -1;
  for (int i = 0; i < 50 && this->uart_.open_connections() == 2; i++) {
    this->loops(1);
  }
  EXPECT_EQ(this->uart_.available(), 0u);
  EXPECT_NE(this->uart_.turn(), turn_of_a);
  EXPECT_TRUE(this->uart_.is_connected());
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);

  send(this->b_, "hi");
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, WriteAfterTheTurnEndedGoesToTheLastHolder) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  this->read_all();
  this->at(1100);
  this->loops(1);
  this->write("late");
  this->loops(1);
  EXPECT_EQ(received(this->a_), "late");
  EXPECT_EQ(received(this->b_), "");
}

TEST_F(TcpUartTwoClients, WriteAfterTheLastHolderClosedNeverReachesTheNextClient) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  this->read_all();
  this->at(1100);
  this->loops(1);
  const uint16_t turn_of_a = this->uart_.turn();
  ::close(this->a_);
  this->a_ = -1;
  for (int i = 0; i < 50 && this->uart_.open_connections() == 2; i++) {
    this->loops(1);
  }
  // A's turn ended at 1100, but writes went to A until it closed, so the close changes the turn.
  EXPECT_NE(this->uart_.turn(), turn_of_a);
  // C takes the slot A left.
  this->c_ = this->connect_client();
  this->write("late");
  this->loops(1);
  EXPECT_EQ(received(this->c_), "");
  EXPECT_EQ(received(this->b_), "");
}

TEST_F(TcpUartTwoClients, FullPoolClosesTheClientQuietLongest) {
  // A connected at 1000 and never sent. B sent at 2000, and its turn ended at 2100.
  this->at(2000);
  send(this->b_, "x");
  this->wait_for_bytes(1);
  this->read_all();
  this->at(2100);
  this->loops(1);
  const uint16_t turn_of_b = this->uart_.turn();

  this->at(3000);
  this->c_ = this->connect_client();
  EXPECT_EQ(received(this->a_), "<closed>");
  EXPECT_EQ(received(this->b_), "");
  EXPECT_EQ(this->uart_.open_connections(), 2);
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);
  // Writes still go to B, so closing A keeps the turn.
  EXPECT_EQ(this->uart_.turn(), turn_of_b);

  send(this->c_, "new");
  this->wait_for_bytes(3);
  EXPECT_EQ(this->read_all(), "new");
}

// The same server with three connection slots. Clients A and B connected at 1000 ms.
class TcpUartThreeSlots : public TcpUartTwoClients {
 protected:
  TcpUartThreeSlots() { this->slots_ = 3; }
};

TEST_F(TcpUartThreeSlots, FullPoolSparesTheClientWhoseRequestWaits) {
  // A has the turn and holds it for its reply; B's request waits for its turn.
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  this->uart_.hold_turn();
  EXPECT_EQ(this->read_all(), "ping");
  send(this->b_, "hi");
  this->loops(3);
  EXPECT_EQ(this->uart_.available(), 0u);
  // C connects at 1500 and sends nothing.
  this->at(1500);
  this->c_ = this->connect_client();

  // D connects at 2000 to the full pool. B has been connected longer than C, but its request waits, so C is closed.
  this->at(2000);
  this->d_ = this->connect_client();
  EXPECT_EQ(received(this->c_), "<closed>");
  EXPECT_EQ(received(this->b_), "");
  EXPECT_EQ(this->uart_.open_connections(), 3);
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);

  // A is answered; B's request is still there and gets the next turn.
  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

// The two-client server with a 1000 ms idle timeout: a connection that moves no byte for 1000 ms is closed.
// Clients A and B connected at 1000 ms.
class TcpUartIdleTimeout : public TcpUartTwoClients {
 protected:
  TcpUartIdleTimeout() { this->idle_timeout_ = 1000; }
};

TEST_F(TcpUartIdleTimeout, OnlyTheQuietClientIsClosed) {
  // A sends a request at 1500 and gets its reply. B sends nothing.
  this->at(1500);
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  EXPECT_EQ(this->read_all(), "ping");
  this->write("pong");
  this->loops(1);
  EXPECT_EQ(received(this->a_), "pong");

  // B has been quiet since it connected at 1000, so its 1000 ms are up at 2000. A's last bytes moved at 1500.
  this->at(1999);
  this->loops(2);
  EXPECT_EQ(this->uart_.open_connections(), 2);
  this->at(2000);
  this->loops(2);
  EXPECT_EQ(received(this->b_), "<closed>");
  EXPECT_EQ(this->uart_.open_connections(), 1);
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);

  // A goes on exchanging data.
  send(this->a_, "hi");
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
  this->write("ok");
  this->loops(1);
  EXPECT_EQ(received(this->a_), "ok");
}

TEST_F(TcpUartIdleTimeout, ClientThatKeepsSendingStaysOpen) {
  // B sends a request every 500 ms until 4000 and gets each reply: 3000 ms, three times the timeout.
  for (uint32_t ms = 1500; ms <= 4000; ms += 500) {
    this->at(ms);
    send(this->b_, "x");
    this->wait_for_bytes(1);
    EXPECT_EQ(this->read_all(), "x");
    this->write("y");
    this->loops(1);
    EXPECT_EQ(received(this->b_), "y");
  }
  // Only A, quiet since 1000, was closed.
  EXPECT_EQ(received(this->a_), "<closed>");
  EXPECT_EQ(this->uart_.open_connections(), 1);
}

TEST_F(TcpUartIdleTimeout, HeldTurnKeepsItsClientOpenUntilReleased) {
  // A's request takes the turn at 1000. Its reply takes 1500 ms, and the turn is held meanwhile.
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  this->uart_.hold_turn();
  EXPECT_EQ(this->read_all(), "ping");
  this->at(2500);
  this->loops(2);
  // A sent nothing for 1500 ms, but it waits for its reply. B, quiet since 1000, is closed.
  EXPECT_EQ(received(this->b_), "<closed>");
  EXPECT_EQ(this->uart_.open_connections(), 1);
  this->write("pong");
  this->uart_.release_turn();
  this->loops(1);
  EXPECT_EQ(received(this->a_), "pong");

  // Once answered, A is quiet like any other connection: 1000 ms after its reply it is closed too.
  this->at(3500);
  this->loops(2);
  EXPECT_EQ(received(this->a_), "<closed>");
}

TEST_F(TcpUartIdleTimeout, RequestWaitingForItsTurnKeepsItsClientOpen) {
  // As above, A's request takes the turn at 1000 and A holds it for its reply. This time B's request waits.
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  this->uart_.hold_turn();
  EXPECT_EQ(this->read_all(), "ping");
  send(this->b_, "hi");
  this->at(2500);
  this->loops(2);
  // At 2500 none of B's bytes has moved for 1500 ms, but they wait for their turn, so B is not quiet.
  EXPECT_EQ(this->uart_.open_connections(), 2);

  // A is answered, and B's request gets the next turn.
  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartIdleTimeout, FullReceiveBufferKeepsItsOwnClientOpen) {
  // B fills the 1024-byte receive buffer at 1000 and stops; nothing reads the buffer. A sends nothing.
  const std::string block(1024, 'b');
  send(this->b_, block.c_str());
  this->wait_for_bytes(1024);

  // At 2000 neither client has sent anything for 1000 ms. B's bytes still wait here, so only A is closed.
  this->at(2000);
  this->loops(2);
  EXPECT_EQ(received(this->a_), "<closed>");
  EXPECT_EQ(this->uart_.open_connections(), 1);
  EXPECT_EQ(this->uart_.available(), 1024u);
}

TEST_F(TcpUartIdleTimeout, BytesLeftInTheSocketKeepTheirClientOpen) {
  // B sends 1500 bytes at 1000. The 1024-byte receive buffer takes 1024 of them, and they are consumed;
  // 476 stay in B's socket.
  const std::string block(1500, 'b');
  send(this->b_, block.c_str());
  this->wait_for_bytes(1024);
  uint8_t taken[1024];
  ASSERT_TRUE(this->uart_.read_array(taken, sizeof(taken)));
  // At 1500 B's turn is released, and A's request takes the turn and holds it. B's 476 bytes wait.
  send(this->a_, "ping");
  this->at(1500);
  this->uart_.release_turn();
  this->wait_for_bytes(4);
  EXPECT_EQ(this->read_all(), "ping");
  this->uart_.hold_turn();

  // At 2500 no socket is reported ready, as with lwIP fast select (ESP32, LibreTiny) after a read that stopped early.
  this->at(2500);
  internal::wakeable_delay(5);
  FD_ZERO(&internal::g_read_fds);
  this->uart_.loop();
  // B's bytes still wait for their turn, so B is not quiet and stays open.
  EXPECT_EQ(received(this->b_), "");
}

}  // namespace esphome::tcp_uart::testing

#endif
#endif
