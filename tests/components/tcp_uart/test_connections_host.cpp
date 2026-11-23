#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

#include "common.h"
#include "esphome/components/logger/logger.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/wake/wake_host.h"

#ifdef USE_HOST
#ifdef USE_TCP_UART_MAX_CONNECTIONS

// Host only: ESP-IDF has no poll.h.
#include <poll.h>

namespace esphome::tcp_uart::testing {

class TcpUartConnectionsUnderTest : public TcpUart {
 public:
  uint8_t open_connections() const { return static_cast<uint8_t>(__builtin_popcount(this->up_mask_)); }
  // Hands slot i one end of a local socket pair, as an accept would, and returns the client's end. Unlike loopback
  // TCP its buffers never grow, so a client that stops reading stops the writes at a fixed point.
  int adopt_socket_pair(uint8_t i) {
    int fds[2];
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    this->slot_(i).adopt(std::make_unique<socket::Socket>(fds[0]));
    this->slot_edge_(i);
    return fds[1];
  }
};

// A server with two connection slots and a 100 ms turn timeout. Clients A and B are real TCP
// sockets on loopback, connected at 1000 ms.
class TcpUartTwoClients : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    this->at(1000);
    ASSERT_NO_FATAL_FAILURE(find_free_port(&this->port_));
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

  static void at(uint32_t ms) { set_loop_clock(ms); }
  void loops(int count) {
    for (int i = 0; i < count; i++) {
      loop_pass(this->uart_);
    }
  }
  // Loopback needs a moment to deliver what a client sent. Then 3 passes, which alone take no time while
  // another client's bytes keep select() ready.
  void deliver() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    this->loops(3);
  }
  // Connects a client and runs the loop until the UART took it.
  int connect_client() {
    int fd = -1;
    EXPECT_NO_FATAL_FAILURE(connect_loopback(this->port_, &fd));
    const uint8_t before = this->uart_.open_connections();
    for (int i = 0; i < 50 && this->uart_.open_connections() == before; i++) {
      this->loops(1);
    }
    return fd;
  }
  static void send(int fd, const char *text) { send_bytes(fd, text, strlen(text)); }
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
    pass_until_available(this->uart_, count, [this]() { this->loops(1); });
  }
  // Requests are two bytes. A writes "1." and the first byte of "2.", then B sends "hi". The consumer answers
  // "1.", takes "2" and names the one byte it lacks.
  void split_second_request() {
    send(this->a_, "1.2");
    this->wait_for_bytes(3);
    send(this->b_, "hi");
    char request[3]{};
    this->uart_.hold_turn();
    ASSERT_TRUE(this->uart_.read_array(reinterpret_cast<uint8_t *>(request), 2));
    EXPECT_STREQ(request, "1.");
    this->uart_.release_turn();
    this->loops(3);
    uint8_t first_byte;
    ASSERT_TRUE(this->uart_.read_byte(&first_byte));
    EXPECT_EQ(first_byte, '2');
    this->uart_.release_turn(1);
  }
  std::string read_all() {
    char buf[32]{};
    size_t n = this->uart_.available();
    EXPECT_TRUE(this->uart_.read_array(reinterpret_cast<uint8_t *>(buf), n));
    return std::string(buf, n);
  }
  void write(const char *text) { this->uart_.write_array(reinterpret_cast<const uint8_t *>(text), strlen(text)); }
  // Writes text and returns what that logged. The test logger only prints while it has a baud rate.
  std::string write_logged(const char *text) {
    const uint32_t baud_rate = logger::global_logger->get_baud_rate();
    logger::global_logger->set_baud_rate(115200);
    ::testing::internal::CaptureStdout();
    this->write(text);
    std::string log = ::testing::internal::GetCapturedStdout();
    logger::global_logger->set_baud_rate(baud_rate);
    return log;
  }

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
  this->deliver();
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
  this->deliver();
  EXPECT_EQ(this->uart_.turn(), turn_of_a);
  EXPECT_EQ(this->read_all(), ".");

  // Request 3 is answered; then B has its turn.
  this->uart_.hold_turn();
  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, RestOfASplitRequestIsAllTheReleasedTurnReads) {
  ASSERT_NO_FATAL_FAILURE(this->split_second_request());
  // A sends the missing byte and its next request "3." at once.
  send(this->a_, ".3.");
  this->deliver();
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
  ASSERT_NO_FATAL_FAILURE(this->split_second_request());
  // A sends the missing byte and nothing more.
  send(this->a_, ".");
  this->deliver();
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
  // B and C are open, so the warning names the missing turn, not a missing connection.
  const std::string log = this->write_logged("late");
  EXPECT_NE(log.find("No connection holds the turn, dropped 4 bytes"), std::string::npos) << log;
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

  // C asks, then stops reading while its reply fills its socket, until a part stays in the TX buffer that all
  // clients share. C's turn ends at 1000 ms. Then B asks. Returns C's turn.
  uint16_t stall_c_then_b_asks() {
    this->c_ = this->uart_.adopt_socket_pair(2);
    send(this->c_, "1.");
    this->wait_for_bytes(2);
    const uint16_t turn_of_c = this->uart_.turn();
    this->uart_.hold_turn();
    EXPECT_EQ(this->read_all(), "1.");
    const std::string block(1024, 'x');
    for (int i = 0; i < 1000 && this->uart_.flush() == uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; i++) {
      this->write(block.c_str());
    }
    EXPECT_LT(this->uart_.available_for_write(), block.size());
    this->uart_.release_turn();
    send(this->b_, "hi");
    this->deliver();
    return turn_of_c;
  }
  // Reads what reached C, as a client would.
  void c_reads() {
    char sink[4096];
    while (::recv(this->c_, sink, sizeof(sink), MSG_DONTWAIT) > 0) {
    }
  }
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

TEST_F(TcpUartThreeSlots, NextTurnWaitsUntilTheLastReplyLeftTheSharedBuffer) {
  const uint16_t turn_of_c = this->stall_c_then_b_asks();

  // B's request waits: its reply would need the buffer that still holds C's.
  EXPECT_EQ(this->uart_.turn(), turn_of_c);
  EXPECT_EQ(this->uart_.available(), 0u);

  // C reads everything. Once its reply has left the buffer, B gets the turn and its reply reaches B alone.
  for (int i = 0; i < 1000 && this->uart_.available() == 0; i++) {
    this->c_reads();
    this->loops(1);
  }
  EXPECT_NE(this->uart_.turn(), turn_of_c);
  EXPECT_EQ(this->read_all(), "hi");
  this->write("ok");
  this->loops(1);
  EXPECT_EQ(received(this->b_), "ok");
  // C's last bytes arrived while it read; after them nothing more came.
  this->c_reads();
  EXPECT_EQ(received(this->c_), "");
}

TEST_F(TcpUartThreeSlots, ClientThatStopsReadingIsClosedAfterFiveSecondsWhileAnotherWaits) {
  const uint16_t turn_of_c = this->stall_c_then_b_asks();
  // C sends its next request but still reads nothing, so it never looks idle.
  send(this->c_, "2.");

  // At 5999 ms no byte of C's reply has left the buffer for 4999 ms: B still waits, C stays open.
  this->at(5999);
  this->deliver();
  EXPECT_EQ(this->uart_.turn(), turn_of_c);
  EXPECT_EQ(this->uart_.available(), 0u);
  EXPECT_EQ(this->uart_.open_connections(), 3);

  // At 6000 ms that is 5 s: C is closed and B gets the turn. B's reply reaches B alone.
  this->at(6000);
  this->loops(1);
  EXPECT_EQ(this->uart_.open_connections(), 2);
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);
  EXPECT_EQ(this->read_all(), "hi");
  this->write("ok");
  this->loops(1);
  EXPECT_EQ(received(this->b_), "ok");
}

TEST_F(TcpUartThreeSlots, BytesThatStallLongAfterTheTurnGetTheirOwnFiveSeconds) {
  // C asks and is answered at once: its turn ends at 1000 ms and the shared buffer is empty.
  this->c_ = this->uart_.adopt_socket_pair(2);
  send(this->c_, "1.");
  this->wait_for_bytes(2);
  const uint16_t turn_of_c = this->uart_.turn();
  this->uart_.hold_turn();
  EXPECT_EQ(this->read_all(), "1.");
  this->write("ok");
  this->uart_.release_turn();
  this->loops(1);

  // At 10000 ms the device sends C more than C takes, as C reads no more, and fills the buffer to the top. B asks.
  this->at(10000);
  this->loops(1);
  const std::string block(1024, 'x');
  for (int i = 0; i < 1000 && this->uart_.flush() == uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; i++) {
    this->write(block.c_str());
  }
  this->write(std::string(this->uart_.available_for_write(), 'y').c_str());
  send(this->b_, "hi");
  this->deliver();

  // The 5 s count from 10000 ms, not from the end of C's turn: at 14999 ms B still waits and C stays open.
  this->at(14999);
  this->deliver();
  EXPECT_EQ(this->uart_.turn(), turn_of_c);
  EXPECT_EQ(this->uart_.open_connections(), 3);

  // At 15000 ms C is closed and B gets the turn.
  this->at(15000);
  this->loops(1);
  EXPECT_EQ(this->uart_.open_connections(), 2);
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);
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
