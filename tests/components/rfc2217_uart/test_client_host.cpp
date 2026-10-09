#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "fakes.h"
#include "esphome/core/application.h"

#ifdef USE_HOST

namespace esphome::rfc2217_uart::testing {
namespace {

class Client : public Rfc2217Client {
 public:
  explicit Client(Pipe *pipe) {
    this->set_tcp_uart(pipe);
    this->set_baud_rate(9600);
    this->set_data_bits(8);
    this->set_parity(uart::UART_CONFIG_PARITY_NONE);
    this->set_stop_bits(1);
  }
  // What load_settings() does on ESP8266 and ESP32.
  void reload() { this->request_settings_(); }
  bool com_port() const { return this->com_port_(); }
};

const uint8_t OFFERS[] = {0xFF, 0xFB, 0x00, 0xFF, 0xFD, 0x00, 0xFF, 0xFB, 0x2C};
const uint8_t DO_COM_PORT[] = {0xFF, 0xFD, 0x2C};
const uint8_t SETTINGS[] = {0xFF, 0xFA, 0x2C, 0x01, 0x00, 0x00, 0x25, 0x80, 0xFF, 0xF0, 0xFF,
                            0xFA, 0x2C, 0x02, 0x08, 0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 0x03, 0x01,
                            0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 0x04, 0x01, 0xFF, 0xF0};
const uint8_t SERVER_SUSPEND[] = {0xFF, 0xFA, 0x2C, 108, 0xFF, 0xF0};
const uint8_t SERVER_RESUME[] = {0xFF, 0xFA, 0x2C, 109, 0xFF, 0xF0};

void at(uint32_t now_ms) { LoopBlockingGuard dispatch{nullptr, nullptr, now_ms}; }

// Connected, COM-PORT accepted and the settings sent.
void accept(Pipe &pipe, Client &client) {
  client.loop();
  pipe.feed(DO_COM_PORT);
  client.loop();
  pipe.clear();
}

template<size_t N> void expect_only(const Pipe &pipe, const uint8_t (&want)[N]) {
  ASSERT_EQ(pipe.n_, N);
  EXPECT_EQ(std::memcmp(pipe.buf_, want, N), 0);
}

class Rfc2217ClientTest : public ::testing::Test {
 protected:
  void SetUp() override { at(0); }
  void TearDown() override { at(0); }
};

TEST_F(Rfc2217ClientTest, ConnectOffersBinaryAndComPort) {
  Pipe pipe;
  Client client(&pipe);
  client.dump_config();
  client.loop();
  expect_only(pipe, OFFERS);
  EXPECT_FALSE(client.com_port());
}

TEST_F(Rfc2217ClientTest, SettingsGoOutWhenTheServerAcceptsComPort) {
  Pipe pipe;
  Client client(&pipe);
  client.loop();
  pipe.clear();
  client.loop();
  EXPECT_EQ(pipe.n_, 0u);
  pipe.feed(DO_COM_PORT);
  client.loop();
  EXPECT_TRUE(client.com_port());
  // The accepted offer is not answered.
  expect_only(pipe, SETTINGS);
}

TEST_F(Rfc2217ClientTest, LoadSettingsSendsTheNewLine) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  client.set_baud_rate(115200);
  client.set_parity(uart::UART_CONFIG_PARITY_EVEN);
  client.reload();
  const uint8_t baud[] = {0xFF, 0xFA, 0x2C, 0x01, 0x00, 0x01, 0xC2, 0x00, 0xFF, 0xF0};
  const uint8_t parity[] = {0xFF, 0xFA, 0x2C, 0x03, 0x03, 0xFF, 0xF0};
  EXPECT_TRUE(pipe.sent(baud));
  EXPECT_TRUE(pipe.sent(parity));
}

TEST_F(Rfc2217ClientTest, SettingsWaitForRoom) {
  Pipe pipe;
  Client client(&pipe);
  client.loop();
  pipe.feed(DO_COM_PORT);
  pipe.room_ = 34;
  pipe.clear();
  client.loop();
  EXPECT_EQ(pipe.n_, 0u);
  pipe.room_ = 1024;
  client.loop();
  expect_only(pipe, SETTINGS);
}

TEST_F(Rfc2217ClientTest, PayloadWaitsForTheSettings) {
  Pipe pipe;
  Client client(&pipe);
  client.loop();
  pipe.clear();
  const uint8_t data[] = {0x41, 0x42};
  client.write_array(data, sizeof(data));
  EXPECT_EQ(pipe.n_, 0u);
  EXPECT_EQ(client.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT);
  pipe.feed(DO_COM_PORT);
  client.loop();
  ASSERT_EQ(pipe.n_, sizeof(SETTINGS) + sizeof(data));
  EXPECT_EQ(std::memcmp(pipe.buf_, SETTINGS, sizeof(SETTINGS)), 0);
  EXPECT_EQ(std::memcmp(pipe.buf_ + sizeof(SETTINGS), data, sizeof(data)), 0);
}

TEST_F(Rfc2217ClientTest, PayloadGoesWhenTheServerDoesNotAnswer) {
  Pipe pipe;
  Client client(&pipe);
  client.loop();
  pipe.clear();
  const uint8_t data[] = {0x41};
  client.write_array(data, sizeof(data));
  at(2999);
  client.loop();
  EXPECT_EQ(pipe.n_, 0u);
  at(3000);
  client.loop();
  expect_only(pipe, data);
}

TEST_F(Rfc2217ClientTest, PayloadGoesWhenComPortIsRefused) {
  Pipe pipe;
  Client client(&pipe);
  client.loop();
  pipe.clear();
  const uint8_t dont[] = {0xFF, 0xFE, 0x2C};
  pipe.feed(dont);
  const uint8_t data[] = {0x41};
  client.write_array(data, sizeof(data));
  EXPECT_EQ(pipe.n_, 0u);
  client.loop();
  EXPECT_FALSE(client.com_port());
  expect_only(pipe, data);
}

TEST_F(Rfc2217ClientTest, PayloadIacIsDoubled) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  const uint8_t data[] = {0x01, 0xFF, 0x13, 0x11};
  client.write_array(data, sizeof(data));
  const uint8_t want[] = {0x01, 0xFF, 0xFF, 0x13, 0x11};
  expect_only(pipe, want);
  EXPECT_EQ(client.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS);
}

TEST_F(Rfc2217ClientTest, LargeWriteIsSentWhole) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  uint8_t data[600];
  std::memset(data, 0x30, sizeof(data));
  client.write_array(data, sizeof(data));
  EXPECT_EQ(pipe.n_, sizeof(data));
}

TEST_F(Rfc2217ClientTest, XonXoffAndDoubledIacArePayload) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  const uint8_t stream[] = {0x11, 0x13, 0xFF, 0xFF, 0x41};
  pipe.feed(stream);
  client.loop();
  uint8_t got[4];
  ASSERT_EQ(client.available(), sizeof(got));
  ASSERT_TRUE(client.read_array(got, sizeof(got)));
  const uint8_t want[] = {0x11, 0x13, 0xFF, 0x41};
  EXPECT_EQ(std::memcmp(got, want, sizeof(want)), 0);
  EXPECT_EQ(pipe.n_, 0u);
}

TEST_F(Rfc2217ClientTest, ServerSuspendHoldsTheWritesUntilResume) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  pipe.feed(SERVER_SUSPEND);
  client.loop();
  const uint8_t data[] = {0x11, 0x13};
  client.write_array(data, sizeof(data));
  EXPECT_EQ(pipe.n_, 0u);
  EXPECT_EQ(client.available_for_write(), 256u - sizeof(data));
  pipe.feed(SERVER_RESUME);
  client.loop();
  expect_only(pipe, data);
}

TEST_F(Rfc2217ClientTest, FullTxBufferDropsTheRest) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  pipe.feed(SERVER_SUSPEND);
  client.loop();
  uint8_t data[300];
  std::memset(data, 0x30, sizeof(data));
  client.write_array(data, sizeof(data));
  EXPECT_EQ(client.available_for_write(), 0u);
  pipe.feed(SERVER_RESUME);
  client.loop();
  EXPECT_EQ(pipe.n_, 256u);
}

TEST_F(Rfc2217ClientTest, FullRxSendsNoSuspend) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  uint8_t data[300];
  std::memset(data, 0x30, sizeof(data));
  pipe.feed(data, sizeof(data));
  for (int i = 0; i < 4; i++) {
    client.loop();
  }
  // The client leaves the bytes in the link; TCP holds the server back.
  EXPECT_EQ(client.available(), 256u);
  EXPECT_EQ(pipe.rx_n_, sizeof(data) - 256);
  EXPECT_EQ(pipe.n_, 0u);
}

TEST_F(Rfc2217ClientTest, ResumeBehindUnreadBytesIsReached) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  pipe.feed(SERVER_SUSPEND);
  client.loop();
  const uint8_t out[] = {0x41};
  client.write_array(out, sizeof(out));
  uint8_t data[300];
  // No 0xFF, which would start a command.
  for (size_t i = 0; i < sizeof(data); i++) {
    data[i] = static_cast<uint8_t>(i % 250);
  }
  pipe.feed(data, sizeof(data));
  pipe.feed(SERVER_RESUME);
  for (int i = 0; i < 5; i++) {
    client.loop();
  }
  // Nobody read the RX bytes; the oldest went so the RESUME got through.
  expect_only(pipe, out);
  EXPECT_EQ(client.available(), 256u);
  uint8_t first;
  ASSERT_TRUE(client.peek_byte(&first));
  EXPECT_EQ(first, static_cast<uint8_t>(sizeof(data) - 256));
}

TEST_F(Rfc2217ClientTest, OtherOptionsAreRefused) {
  Pipe pipe;
  Client client(&pipe);
  client.loop();
  pipe.clear();
  // WILL ECHO, DO SGA, WONT 5.
  const uint8_t stream[] = {0xFF, 0xFB, 0x01, 0xFF, 0xFD, 0x03, 0xFF, 0xFC, 0x05};
  pipe.feed(stream);
  client.loop();
  const uint8_t want[] = {0xFF, 0xFE, 0x01, 0xFF, 0xFC, 0x03};
  expect_only(pipe, want);
}

TEST_F(Rfc2217ClientTest, OptionStateIsAnsweredOnce) {
  Pipe pipe;
  Client client(&pipe);
  client.loop();
  pipe.clear();
  // Both BINARY offers accepted, then accepted again.
  const uint8_t accepted[] = {0xFF, 0xFD, 0x00, 0xFF, 0xFB, 0x00, 0xFF, 0xFD, 0x00};
  pipe.feed(accepted);
  client.loop();
  EXPECT_EQ(pipe.n_, 0u);
  // WONT BINARY while on, WILL COM-PORT while off, WONT BINARY while off.
  const uint8_t changes[] = {0xFF, 0xFC, 0x00, 0xFF, 0xFB, 0x2C, 0xFF, 0xFC, 0x00};
  pipe.feed(changes);
  client.loop();
  const uint8_t want[] = {0xFF, 0xFE, 0x00, 0xFF, 0xFD, 0x2C};
  expect_only(pipe, want);
}

TEST_F(Rfc2217ClientTest, ServerAnswersAreNoPayload) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  // Answers that differ from the request, short ones, a line state notice and another subnegotiation.
  const uint8_t stream[] = {0xFF, 0xFA, 0x2C, 101,  0x00, 0x00, 0x12, 0xC0, 0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 102,
                            7,    0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 103,  2,    0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 104,
                            2,    0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 101,  0x00, 0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 102,
                            0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 103,  0xFF, 0xF0, 0xFF, 0xFA, 0x2C, 104,  0xFF, 0xF0,
                            0xFF, 0xFA, 0x2C, 106,  0x60, 0xFF, 0xF0, 0xFF, 0xFA, 0x18, 0x01, 0xFF, 0xF0};
  pipe.feed(stream);
  client.loop();
  EXPECT_EQ(client.available(), 0u);
  EXPECT_EQ(pipe.n_, 0u);
}

TEST_F(Rfc2217ClientTest, SignatureRequestIsAnswered) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  const uint8_t request[] = {0xFF, 0xFA, 0x2C, 100, 0xFF, 0xF0};
  const uint8_t theirs[] = {0xFF, 0xFA, 0x2C, 100, 's', 'e', 'r', 0xFF, 0xF0};
  pipe.feed(request);
  pipe.feed(theirs);
  client.loop();
  const uint8_t want[] = {0xFF, 0xFA, 0x2C, 0x00, 'E', 'S', 'P', 'H', 'o', 'm', 'e', 0xFF, 0xF0};
  expect_only(pipe, want);
}

TEST_F(Rfc2217ClientTest, NotConnectedUntilItsOwnUpEdge) {
  Pipe pipe;
  Client client(&pipe);
  EXPECT_FALSE(client.is_connected());
  EXPECT_EQ(client.available_for_write(), 0u);
  const uint8_t data[] = {0x41};
  client.write_array(data, sizeof(data));
  EXPECT_EQ(client.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED);
  client.loop();
  EXPECT_TRUE(client.is_connected());
  expect_only(pipe, OFFERS);
}

TEST_F(Rfc2217ClientTest, PayloadBeforeALineChangeLeavesFirst) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  pipe.feed(SERVER_SUSPEND);
  client.loop();
  const uint8_t before[] = {0x41, 0x42};
  const uint8_t after[] = {0x43};
  client.write_array(before, sizeof(before));
  client.set_baud_rate(115200);
  client.reload();
  client.write_array(after, sizeof(after));
  // The suspend holds the commands too.
  EXPECT_EQ(pipe.n_, 0u);
  pipe.feed(SERVER_RESUME);
  client.loop();
  client.loop();
  const uint8_t baud[] = {0xFF, 0xFA, 0x2C, 0x01, 0x00, 0x01, 0xC2, 0x00, 0xFF, 0xF0};
  ASSERT_EQ(pipe.n_, sizeof(before) + sizeof(SETTINGS) + sizeof(after));
  EXPECT_EQ(std::memcmp(pipe.buf_, before, sizeof(before)), 0);
  EXPECT_EQ(std::memcmp(pipe.buf_ + sizeof(before), baud, sizeof(baud)), 0);
  EXPECT_EQ(pipe.buf_[pipe.n_ - 1], 0x43);
}

TEST_F(Rfc2217ClientTest, BytesSentBeforeTheCloseStayReadable) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  uint8_t data[200];
  for (size_t i = 0; i < sizeof(data); i++) {
    data[i] = static_cast<uint8_t>(i % 250);
  }
  pipe.feed(data);
  pipe.up_ = false;
  client.loop();
  client.loop();
  ASSERT_EQ(client.available(), sizeof(data));
  uint8_t got[sizeof(data)];
  ASSERT_TRUE(client.read_array(got, sizeof(got)));
  EXPECT_EQ(std::memcmp(got, data, sizeof(data)), 0);
}

TEST_F(Rfc2217ClientTest, LinkDownDropsTheUnsentAndUpClearsTheRest) {
  Pipe pipe;
  Client client(&pipe);
  accept(pipe, client);
  pipe.feed(SERVER_SUSPEND);
  const uint8_t one[] = {0x42};
  pipe.feed(one);
  client.loop();
  client.write_array(one, sizeof(one));
  EXPECT_EQ(client.available(), 1u);
  pipe.up_ = false;
  client.loop();
  // Still readable while down.
  EXPECT_EQ(client.available(), 1u);
  pipe.clear();
  pipe.up_ = true;
  client.loop();
  EXPECT_EQ(client.available(), 0u);
  expect_only(pipe, OFFERS);
}

}  // namespace
}  // namespace esphome::rfc2217_uart::testing

#endif  // USE_HOST
