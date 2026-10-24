#include <gtest/gtest.h>

#include <cmath>
#include <csignal>
#include <cstring>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/tcp_uart/tcp_uart.h"

#if defined(USE_HOST) && defined(USE_NOISE_STREAM)

namespace esphome::tcp_uart::testing {

static const uint8_t KEY[32] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
static const uint8_t OTHER_KEY[32] = {32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17,
                                      16, 15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1};

class NoiseUart : public TcpUart {
 public:
  NoiseUart(const uint8_t *key, bool initiator) : stream_(key, initiator) {
    this->set_host("peer");
    this->set_port(1);
    this->link_.begin("noise_test");
    this->set_noise_stream(&this->stream_);
    this->set_connected_sensor(&this->connected_);
    this->set_disconnects_sensor(&this->disconnects_);
  }
  socket::TcpClientLink &link() { return this->link_; }

  noise::NoiseStream stream_;
  binary_sensor::BinarySensor connected_;
  sensor::Sensor disconnects_;
};

class TcpUartNoise : public ::testing::Test {
 protected:
  void SetUp() override { signal(SIGPIPE, SIG_IGN); }
  void TearDown() override {
    this->client_->link().close();
    this->server_->link().close();
  }
  void make(const uint8_t *server_key) {
    this->client_ = std::make_unique<NoiseUart>(KEY, true);
    this->server_ = std::make_unique<NoiseUart>(server_key, false);
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    this->client_->link().adopt(std::make_unique<socket::Socket>(fds[0]));
    this->server_->link().adopt(std::make_unique<socket::Socket>(fds[1]));
  }
  void loops(int count) {
    for (int i = 0; i < count; i++) {
      this->client_->loop();
      this->server_->loop();
    }
  }

  std::unique_ptr<NoiseUart> client_;
  std::unique_ptr<NoiseUart> server_;
};

TEST_F(TcpUartNoise, ConnectedOnlyOnceTheSessionIsSecure) {
  this->make(KEY);
  EXPECT_FALSE(this->client_->is_connected());
  EXPECT_EQ(this->client_->available_for_write(), 0u);
  const uint8_t early[] = {0x42};
  this->client_->write_array(early, sizeof(early));
  EXPECT_EQ(this->client_->flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED);

  this->loops(5);
  ASSERT_TRUE(this->client_->is_connected());
  ASSERT_TRUE(this->server_->is_connected());
  EXPECT_TRUE(this->client_->connected_.state);
  EXPECT_TRUE(this->server_->connected_.state);
  EXPECT_GT(this->client_->available_for_write(), 0u);
}

TEST_F(TcpUartNoise, BytesCrossInBothDirections) {
  this->make(KEY);
  this->loops(5);
  ASSERT_TRUE(this->client_->is_connected());

  const uint8_t request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
  this->client_->write_array(request, sizeof(request));
  EXPECT_EQ(this->client_->flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS);
  this->loops(2);
  ASSERT_EQ(this->server_->available(), sizeof(request));
  uint8_t got[sizeof(request)];
  ASSERT_TRUE(this->server_->read_array(got, sizeof(got)));
  EXPECT_EQ(std::memcmp(got, request, sizeof(request)), 0);

  const uint8_t answer[] = {0x01, 0x03, 0x04, 0x00, 0x0A, 0x00, 0x0B};
  this->server_->write_array(answer, sizeof(answer));
  this->loops(2);
  ASSERT_EQ(this->client_->available(), sizeof(answer));
  uint8_t back[sizeof(answer)];
  ASSERT_TRUE(this->client_->read_array(back, sizeof(back)));
  EXPECT_EQ(std::memcmp(back, answer, sizeof(answer)), 0);
}

TEST_F(TcpUartNoise, WrongKeyNeverConnects) {
  this->make(OTHER_KEY);
  this->loops(5);
  EXPECT_FALSE(this->client_->is_connected());
  EXPECT_FALSE(this->server_->is_connected());
  EXPECT_FALSE(this->client_->link().connected());
  EXPECT_FALSE(this->server_->link().connected());
  // Never up, so no session ended
  EXPECT_FALSE(this->client_->connected_.state);
  EXPECT_TRUE(std::isnan(this->client_->disconnects_.state));
}

TEST_F(TcpUartNoise, ADropEndsTheSession) {
  this->make(KEY);
  this->loops(5);
  ASSERT_TRUE(this->server_->is_connected());
  this->client_->link().close();
  this->loops(3);
  EXPECT_FALSE(this->server_->is_connected());
  EXPECT_FALSE(this->server_->connected_.state);
  EXPECT_FLOAT_EQ(this->server_->disconnects_.state, 1);
}

}  // namespace esphome::tcp_uart::testing

#endif
