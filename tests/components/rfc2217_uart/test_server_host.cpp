#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "fakes.h"
#include "esphome/core/application.h"

#ifdef USE_HOST

namespace esphome::rfc2217_uart::testing {
namespace {

class Server : public Rfc2217Server {
 public:
  Server(Pipe *pipe, FakeSerial *serial) {
    this->set_tcp_uart(pipe);
    this->set_uart_parent(serial);
    this->setup();
  }
  size_t pending() const { return this->to_serial_len_; }

  int reloads_{0};
  bool can_reload_{true};
  // How many bytes the UART had taken at the last reload.
  size_t written_at_reload_{0};

 protected:
  bool reload_serial() override {
    this->reloads_++;
    this->written_at_reload_ = static_cast<FakeSerial *>(this->parent_)->n_;
    return this->can_reload_;
  }
};

// The real reload_serial(): a host UART cannot change its line.
class PlainServer : public Rfc2217Server {
 public:
  PlainServer(Pipe *pipe, FakeSerial *serial) {
    this->set_tcp_uart(pipe);
    this->set_uart_parent(serial);
    this->setup();
  }
};

const uint8_t WILL_COM_PORT[] = {0xFF, 0xFB, 0x2C};

// Connected and COM-PORT accepted.
void accept(Pipe &pipe, Rfc2217Server &server) {
  server.loop();
  pipe.feed(WILL_COM_PORT);
  server.loop();
  pipe.clear();
}

// One COM-PORT command with a 1-byte value from the client.
void command(Pipe &pipe, uint8_t code, uint8_t value) {
  const uint8_t sub[] = {0xFF, 0xFA, 0x2C, code, value, 0xFF, 0xF0};
  pipe.feed(sub);
}

void set_baud(Pipe &pipe, uint32_t baud) {
  const uint8_t sub[] = {0xFF,
                         0xFA,
                         0x2C,
                         0x01,
                         static_cast<uint8_t>(baud >> 24),
                         static_cast<uint8_t>(baud >> 16),
                         static_cast<uint8_t>(baud >> 8),
                         static_cast<uint8_t>(baud),
                         0xFF,
                         0xF0};
  pipe.feed(sub);
}

template<size_t N> void expect_only(const Pipe &pipe, const uint8_t (&want)[N]) {
  ASSERT_EQ(pipe.n_, N);
  EXPECT_EQ(std::memcmp(pipe.buf_, want, N), 0);
}

TEST(Rfc2217Server, ConnectRequestsComPort) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  server.dump_config();
  // Stale bytes from before the connect are dropped.
  const uint8_t stale[] = {0x01, 0x02};
  serial.feed(stale, sizeof(stale));
  server.loop();
  const uint8_t want[] = {0xFF, 0xFB, 0x00, 0xFF, 0xFD, 0x00, 0xFF, 0xFD, 0x2C};
  expect_only(pipe, want);
  EXPECT_EQ(serial.rx_n_, 0u);
}

TEST(Rfc2217Server, BaudChangeIsLoadedAndAnswered) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  set_baud(pipe, 19200);
  server.loop();
  EXPECT_EQ(serial.get_baud_rate(), 19200u);
  EXPECT_EQ(server.reloads_, 1);
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 101, 0x00, 0x00, 0x4B, 0x00, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, CommandsOfOnePassShareOneReload) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  // pySerial's open(): all four settings at once.
  set_baud(pipe, 19200);
  command(pipe, 0x02, 7);
  command(pipe, 0x03, 3);
  command(pipe, 0x04, 2);
  server.loop();
  EXPECT_EQ(server.reloads_, 1);
  EXPECT_EQ(serial.get_data_bits(), 7u);
  EXPECT_EQ(serial.get_parity(), uart::UART_CONFIG_PARITY_EVEN);
  EXPECT_EQ(serial.get_stop_bits(), 2u);
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 101,  0x00, 0x00, 0x4B, 0x00, 0xFF, 0xF0, 0xFF,
                          0xFA, 0x2C, 102,  0x07, 0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 103,  0x03,
                          0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 104,  0x02, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, QueryIsAnsweredWithoutAReload) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  set_baud(pipe, 0);
  command(pipe, 0x02, 8);
  server.loop();
  EXPECT_EQ(server.reloads_, 0);
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 101,  0x00, 0x00, 0x25, 0x80, 0xFF,
                          0xF0, 0xFF, 0xFA, 0x2C, 102,  0x08, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, HostUartKeepsItsLine) {
  Pipe pipe;
  FakeSerial serial;
  PlainServer server(&pipe, &serial);
  accept(pipe, server);
  set_baud(pipe, 19200);
  command(pipe, 0x02, 7);
  server.loop();
  EXPECT_EQ(serial.get_baud_rate(), 9600u);
  EXPECT_EQ(serial.get_data_bits(), 8u);
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 101,  0x00, 0x00, 0x25, 0x80, 0xFF,
                          0xF0, 0xFF, 0xFA, 0x2C, 102,  0x08, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, FailedReloadKeepsTheOldLine) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  server.can_reload_ = false;
  accept(pipe, server);
  set_baud(pipe, 19200);
  server.loop();
  EXPECT_EQ(server.reloads_, 1);
  EXPECT_EQ(serial.get_baud_rate(), 9600u);
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 101, 0x00, 0x00, 0x25, 0x80, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, UnsupportedValuesAreAnsweredWithTheValueInUse) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  // 6 Mbaud, 9 data bits, MARK parity, 1.5 stop bits.
  set_baud(pipe, 6000000);
  command(pipe, 0x02, 9);
  command(pipe, 0x03, 4);
  command(pipe, 0x04, 3);
  server.loop();
  EXPECT_EQ(server.reloads_, 0);
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 101,  0x00, 0x00, 0x25, 0x80, 0xFF, 0xF0, 0xFF,
                          0xFA, 0x2C, 102,  0x08, 0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 103,  0x01,
                          0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 104,  0x01, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, ParityValuesMap) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  command(pipe, 0x03, 2);
  server.loop();
  EXPECT_EQ(serial.get_parity(), uart::UART_CONFIG_PARITY_ODD);
  command(pipe, 0x03, 1);
  server.loop();
  EXPECT_EQ(serial.get_parity(), uart::UART_CONFIG_PARITY_NONE);
  EXPECT_EQ(server.reloads_, 2);
}

TEST(Rfc2217Server, PayloadBeforeALineChangeLeavesOnTheOldLine) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  serial.room_ = 2;
  const uint8_t before[] = {1, 2, 3};
  const uint8_t after[] = {4, 5};
  pipe.feed(before);
  set_baud(pipe, 19200);
  pipe.feed(after);
  server.loop();
  // Two of the three bytes fit; the change waits for the third.
  EXPECT_EQ(serial.n_, 2u);
  EXPECT_EQ(server.reloads_, 0);
  EXPECT_EQ(pipe.n_, 0u);
  server.loop();
  EXPECT_EQ(server.reloads_, 1);
  EXPECT_EQ(server.written_at_reload_, 3u);
  server.loop();
  ASSERT_EQ(serial.n_, 5u);
  const uint8_t all[] = {1, 2, 3, 4, 5};
  EXPECT_EQ(std::memcmp(serial.buf_, all, sizeof(all)), 0);
}

TEST(Rfc2217Server, ControlCommandsAreAnsweredOff) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  // Each SET-CONTROL value and the answer: flow control none, BREAK off, DTR off, RTS off, inbound flow control none.
  const uint8_t asked[] = {0, 2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 15, 17, 18, 19, 20};
  const uint8_t answer[] = {1, 1, 6, 6, 9, 9, 9, 12, 12, 12, 14, 14, 1, 14, 1};
  for (uint8_t v : asked) {
    command(pipe, 0x05, v);
  }
  server.loop();
  // 20 is no SET-CONTROL value and gets no answer.
  ASSERT_EQ(pipe.n_, sizeof(answer) * 7);
  for (size_t i = 0; i < sizeof(answer); i++) {
    const uint8_t want[] = {0xFF, 0xFA, 0x2C, 105, answer[i], 0xFF, 0xF0};
    EXPECT_EQ(std::memcmp(pipe.buf_ + i * 7, want, sizeof(want)), 0) << "value " << unsigned(asked[i]);
  }
}

TEST(Rfc2217Server, StateMasksAreAnsweredZero) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  command(pipe, 10, 0xFE);
  command(pipe, 11, 0xFE);
  server.loop();
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 110, 0x00, 0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 111, 0x00, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, SignatureRequestIsAnswered) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  const uint8_t request[] = {0xFF, 0xFA, 0x2C, 0x00, 0xFF, 0xF0};
  // A signature with text is the client's own and needs no answer.
  const uint8_t theirs[] = {0xFF, 0xFA, 0x2C, 0x00, 'p', 'y', 0xFF, 0xF0};
  pipe.feed(request);
  pipe.feed(theirs);
  server.loop();
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 100, 'E', 'S', 'P', 'H', 'o', 'm', 'e', 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, PurgeTransmitDropsThePayloadBeforeIt) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  serial.room_ = 0;
  const uint8_t data[] = {0x41, 0x42};
  pipe.feed(data);
  server.loop();
  EXPECT_EQ(server.pending(), 2u);
  pipe.feed(data);
  command(pipe, 12, 2);
  server.loop();
  EXPECT_EQ(server.pending(), 0u);
  EXPECT_EQ(serial.n_, 0u);
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 112, 0x02, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, PurgeReceiveDropsTheUartInput) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  const uint8_t data[] = {0x01, 0x02, 0x03};
  serial.feed(data, sizeof(data));
  command(pipe, 12, 3);
  // Out of range: no answer.
  command(pipe, 12, 0);
  command(pipe, 12, 4);
  server.loop();
  EXPECT_EQ(serial.rx_n_, 0u);
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 112, 0x03, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, XonXoffAndDoubledIacReachTheUart) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  const uint8_t stream[] = {0x11, 0x13, 0xFF, 0xFF};
  pipe.feed(stream);
  server.loop();
  const uint8_t want[] = {0x11, 0x13, 0xFF};
  ASSERT_EQ(serial.n_, sizeof(want));
  EXPECT_EQ(std::memcmp(serial.buf_, want, sizeof(want)), 0);
  EXPECT_EQ(pipe.n_, 0u);
}

TEST(Rfc2217Server, UartIacIsDoubled) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  const uint8_t data[] = {0x41, 0xFF, 0x13};
  serial.feed(data, sizeof(data));
  server.loop();
  const uint8_t want[] = {0x41, 0xFF, 0xFF, 0x13};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, AllUartBytesMoveInOnePass) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  uint8_t data[400];
  std::memset(data, 0x41, sizeof(data));
  serial.feed(data, sizeof(data));
  server.loop();
  EXPECT_EQ(pipe.n_, sizeof(data));
  EXPECT_EQ(serial.rx_n_, 0u);
}

TEST(Rfc2217Server, WritesArePacedToTheUartRoom) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  serial.room_ = 2;
  const uint8_t data[] = {1, 2, 3, 4, 5};
  pipe.feed(data);
  server.loop();
  EXPECT_EQ(serial.n_, 2u);
  server.loop();
  EXPECT_EQ(serial.n_, 4u);
  server.loop();
  EXPECT_EQ(serial.n_, 5u);
  EXPECT_EQ(std::memcmp(serial.buf_, data, sizeof(data)), 0);
}

TEST(Rfc2217Server, UnknownRoomIsPacedToTheLine) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  serial.room_ = SIZE_MAX;
  App.set_loop_interval(16);
  { LoopBlockingGuard dispatch{nullptr, nullptr, 1000}; }
  uint8_t data[40];
  std::memset(data, 0x41, sizeof(data));
  pipe.feed(data, sizeof(data));
  server.loop();
  // 9600 baud at 10 bits per byte for one 16 ms loop interval.
  EXPECT_EQ(serial.n_, 15u);
  { LoopBlockingGuard dispatch{nullptr, nullptr, 0}; }
}

TEST(Rfc2217Server, CommandWaitsForRoomForItsAnswer) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  pipe.room_ = 10;
  const uint8_t stream[] = {0x41, 0xFF, 0xFA, 0x2C, 0x0C, 0x02, 0xFF, 0xF0};
  pipe.feed(stream);
  server.loop();
  // The payload goes on, the command after its IAC stays in the link.
  EXPECT_EQ(serial.n_, 1u);
  EXPECT_EQ(pipe.rx_n_, 6u);
  pipe.room_ = 1024;
  server.loop();
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 112, 0x02, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, CommandSplitAcrossReadsIsDecoded) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  // Room for one answer plus 2 bytes read per pass.
  pipe.room_ = 20;
  command(pipe, 0x0B, 0xFE);
  const uint8_t data[] = {0x41, 0x42};
  pipe.feed(data);
  for (int i = 0; i < 5; i++) {
    server.loop();
  }
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 111, 0x00, 0xFF, 0xF0};
  expect_only(pipe, want);
  ASSERT_EQ(serial.n_, sizeof(data));
  EXPECT_EQ(std::memcmp(serial.buf_, data, sizeof(data)), 0);
}

TEST(Rfc2217Server, FullBufferSuspendsTheClient) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  serial.room_ = 0;
  uint8_t data[200];
  std::memset(data, 0x30, sizeof(data));
  pipe.feed(data, sizeof(data));
  server.loop();
  server.loop();
  EXPECT_EQ(server.pending(), sizeof(data));
  const uint8_t suspend[] = {0xFF, 0xFA, 0x2C, 108, 0xFF, 0xF0};
  expect_only(pipe, suspend);
  pipe.clear();
  serial.room_ = 1024;
  server.loop();
  const uint8_t resume[] = {0xFF, 0xFA, 0x2C, 109, 0xFF, 0xF0};
  expect_only(pipe, resume);
}

TEST(Rfc2217Server, NoFlowControlWithoutComPort) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  server.loop();
  pipe.clear();
  serial.room_ = 0;
  uint8_t data[200];
  std::memset(data, 0x30, sizeof(data));
  pipe.feed(data, sizeof(data));
  server.loop();
  server.loop();
  EXPECT_EQ(pipe.n_, 0u);
}

TEST(Rfc2217Server, ClientSuspendHoldsTheUartBytes) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  const uint8_t suspend[] = {0xFF, 0xFA, 0x2C, 0x08, 0xFF, 0xF0};
  pipe.feed(suspend);
  server.loop();
  const uint8_t data[] = {0x41};
  serial.feed(data, sizeof(data));
  server.loop();
  EXPECT_EQ(pipe.n_, 0u);
  const uint8_t resume[] = {0xFF, 0xFA, 0x2C, 0x09, 0xFF, 0xF0};
  pipe.feed(resume);
  server.loop();
  expect_only(pipe, data);
}

TEST(Rfc2217Server, ClientSuspendHoldsNoAnswers) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  command(pipe, 0x08, 0x00);
  server.loop();
  set_baud(pipe, 19200);
  server.loop();
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 101, 0x00, 0x00, 0x4B, 0x00, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, OtherOptionsAreRefusedAndRefusalsLogged) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  server.loop();
  pipe.clear();
  // WILL ECHO, DO SGA, DONT BINARY, WONT COM-PORT, WILL COM-PORT once refused.
  const uint8_t stream[] = {0xFF, 0xFB, 0x01, 0xFF, 0xFD, 0x03, 0xFF, 0xFE, 0x00, 0xFF, 0xFC, 0x2C, 0xFF, 0xFB, 0x2C};
  pipe.feed(stream);
  server.loop();
  const uint8_t want[] = {0xFF, 0xFE, 0x01, 0xFF, 0xFC, 0x03, 0xFF, 0xFD, 0x2C};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, OptionTurnedOffIsConfirmedOnce) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  const uint8_t binary_on[] = {0xFF, 0xFB, 0x00};
  const uint8_t binary_off[] = {0xFF, 0xFC, 0x00, 0xFF, 0xFC, 0x00};
  pipe.feed(binary_on);
  pipe.feed(binary_off);
  server.loop();
  const uint8_t want[] = {0xFF, 0xFE, 0x00};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, ShortAndForeignSubnegotiationsAreIgnored) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  // SET-BAUDRATE with two bytes, SET-DATASIZE without a value, an unknown command, TERMINAL-TYPE.
  const uint8_t stream[] = {0xFF, 0xFA, 0x2C, 0x01, 0x00, 0x00, 0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 0x02, 0xFF, 0xF0,
                            0xFF, 0xFA, 0x2C, 0x30, 0x41, 0xFF, 0xF0, 0xFF, 0xFA, 0x18, 0x01, 0xFF, 0xF0};
  pipe.feed(stream);
  server.loop();
  EXPECT_EQ(pipe.n_, 0u);
  EXPECT_EQ(serial.n_, 0u);
}

TEST(Rfc2217Server, SessionEndRestoresTheConfiguredLine) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  set_baud(pipe, 19200);
  server.loop();
  EXPECT_EQ(serial.get_baud_rate(), 19200u);
  pipe.up_ = false;
  server.loop();
  EXPECT_EQ(serial.get_baud_rate(), 9600u);
  EXPECT_EQ(server.reloads_, 2);
}

TEST(Rfc2217Server, PayloadSentBeforeTheCloseReachesTheUart) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  set_baud(pipe, 19200);
  server.loop();
  pipe.clear();
  serial.room_ = 0;
  uint8_t data[300];
  for (size_t i = 0; i < sizeof(data); i++) {
    data[i] = static_cast<uint8_t>(i % 250);
  }
  pipe.feed(data);
  server.loop();
  pipe.up_ = false;
  server.loop();
  // The line stays until the payload has left.
  EXPECT_EQ(serial.get_baud_rate(), 19200u);
  serial.room_ = 100;
  for (int i = 0; i < 5; i++) {
    server.loop();
  }
  ASSERT_EQ(serial.n_, sizeof(data));
  EXPECT_EQ(std::memcmp(serial.buf_, data, sizeof(data)), 0);
  EXPECT_EQ(serial.get_baud_rate(), 9600u);
  EXPECT_EQ(server.written_at_reload_, sizeof(data));
  EXPECT_EQ(pipe.n_, 0u);
}

TEST(Rfc2217Server, NewSessionDropsWhatTheLastOneLeft) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  set_baud(pipe, 19200);
  server.loop();
  serial.room_ = 0;
  const uint8_t data[] = {0x41};
  pipe.feed(data);
  server.loop();
  pipe.up_ = false;
  server.loop();
  EXPECT_EQ(server.pending(), 1u);
  pipe.up_ = true;
  server.loop();
  EXPECT_EQ(server.pending(), 0u);
  EXPECT_EQ(serial.get_baud_rate(), 9600u);
  serial.room_ = 1024;
  server.loop();
  EXPECT_EQ(serial.n_, 0u);
}

TEST(Rfc2217Server, EachCommandOfACodeIsAnswered) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  // A query and a change in one read.
  set_baud(pipe, 0);
  set_baud(pipe, 19200);
  server.loop();
  EXPECT_EQ(server.reloads_, 1);
  const uint8_t answer[] = {0xFF, 0xFA, 0x2C, 101, 0x00, 0x00, 0x4B, 0x00, 0xFF, 0xF0};
  ASSERT_EQ(pipe.n_, 2 * sizeof(answer));
  EXPECT_EQ(std::memcmp(pipe.buf_, answer, sizeof(answer)), 0);
  EXPECT_EQ(std::memcmp(pipe.buf_ + sizeof(answer), answer, sizeof(answer)), 0);
}

TEST(Rfc2217Server, CommandsAfterTheNextPayloadWaitForTheBatch) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  serial.room_ = 0;
  const uint8_t first[] = {1};
  const uint8_t second[] = {2};
  pipe.feed(first);
  set_baud(pipe, 19200);
  pipe.feed(second);
  server.loop();
  // The next change arrives while the first still waits for the UART.
  set_baud(pipe, 38400);
  server.loop();
  EXPECT_EQ(server.reloads_, 0);
  serial.room_ = 1024;
  for (int i = 0; i < 3; i++) {
    server.loop();
  }
  // Byte 2 left on 19200, between the two changes.
  EXPECT_EQ(server.reloads_, 2);
  EXPECT_EQ(server.written_at_reload_, 2u);
  EXPECT_EQ(serial.get_baud_rate(), 38400u);
  const uint8_t answer_19200[] = {0xFF, 0xFA, 0x2C, 101, 0x00, 0x00, 0x4B, 0x00, 0xFF, 0xF0};
  const uint8_t answer_38400[] = {0xFF, 0xFA, 0x2C, 101, 0x00, 0x00, 0x96, 0x00, 0xFF, 0xF0};
  EXPECT_TRUE(pipe.sent(answer_19200));
  EXPECT_TRUE(pipe.sent(answer_38400));
}

TEST(Rfc2217Server, EscapedIacPassesWithoutRoomForAnswers) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  accept(pipe, server);
  pipe.room_ = 0;
  const uint8_t stream[] = {0x41, 0xFF, 0xFF, 0x42};
  pipe.feed(stream);
  server.loop();
  const uint8_t want[] = {0x41, 0xFF, 0x42};
  ASSERT_EQ(serial.n_, sizeof(want));
  EXPECT_EQ(std::memcmp(serial.buf_, want, sizeof(want)), 0);
}

TEST(Rfc2217Server, CommandsWithoutTheClientsWillAreAnswered) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  server.loop();
  pipe.clear();
  // pySerial takes the server's DO as its own WILL and never sends one.
  command(pipe, 0x05, 0x09);
  server.loop();
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 105, 0x09, 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST(Rfc2217Server, LoopFlushesOnlyAfterWriting) {
  Pipe pipe;
  FakeSerial serial;
  Server server(&pipe, &serial);
  server.loop();
  EXPECT_EQ(pipe.flushes_, 1);
  server.loop();
  EXPECT_EQ(pipe.flushes_, 1);
}

}  // namespace
}  // namespace esphome::rfc2217_uart::testing

#endif  // USE_HOST
