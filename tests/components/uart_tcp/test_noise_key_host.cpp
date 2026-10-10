#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "esphome/components/uart_tcp/uart_tcp.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"
#include "esphome/core/wake.h"

#if defined(USE_HOST) && defined(USE_NOISE_STREAM_RUNTIME_KEY)

#include "esphome/components/host/preferences.h"

namespace esphome::uart_tcp::testing {

// Bytes 1 to 32 and 2 to 33, raw and in base64
static const uint8_t KEY[32] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
static const uint8_t OTHER_KEY[32] = {2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                                      18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33};
static const std::string KEY_B64 = "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA=";
static const std::string OTHER_KEY_B64 = "AgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICE=";

// A UART the test fills directly.
class KeyFakeUart : public uart::UARTComponent {
 public:
  KeyFakeUart() { this->set_baud_rate(115200); }
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty())
      return false;
    *data = this->rx.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx.size())
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }
  size_t available_for_write() override { return SIZE_MAX; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

  std::vector<uint8_t> rx;
};

// The bridge dials a loopback listener the test owns; the test's stream answers.
class UartTcpKey : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    static bool prefs_ready = false;
    if (!prefs_ready) {
      // The host backend reads its file once, so no key saved by an earlier run is found
      std::string dir = (std::filesystem::temp_directory_path() / "uart-tcp-key-XXXXXX").string();
      ASSERT_NE(mkdtemp(dir.data()), nullptr);
      setenv("ESPHOME_PREFDIR", dir.c_str(), 1);
      host::setup_preferences();
      prefs_ready = true;
    }
    global_preferences->reset();
    App.set_loop_interval(16);
    this->listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(this->listen_fd_, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(::bind(this->listen_fd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
    ASSERT_EQ(::listen(this->listen_fd_, 1), 0);
    ASSERT_EQ(::fcntl(this->listen_fd_, F_SETFL, O_NONBLOCK), 0);
    socklen_t len = sizeof(addr);
    ASSERT_EQ(::getsockname(this->listen_fd_, reinterpret_cast<struct sockaddr *>(&addr), &len), 0);

    this->bridge_.set_uart_parent(&this->uart_);
    this->bridge_.set_host("127.0.0.1");
    this->bridge_.set_port(ntohs(addr.sin_port));
    this->bridge_.set_reconnect_interval(0);
    this->bridge_.set_noise_stream(&this->bridge_stream_);
    this->bridge_.set_connected_sensor(&this->sensor_);
    this->tick(0);
    this->bridge_.setup();
    this->peer_.begin("peer");
  }
  void TearDown() override {
    this->bridge_.on_shutdown();
    this->peer_.close();
    ::close(this->listen_fd_);
    global_preferences->reset();
  }
  void tick(uint32_t elapsed_ms) {
    this->now_ += elapsed_ms;
    LoopBlockingGuard dispatch{nullptr, nullptr, this->now_};
  }
  // One main loop pass: the bridge runs only while its loop is on, as under App. The test's end
  // accepts, answers the handshake and collects what arrives.
  void pass() {
    internal::wakeable_delay(5);
    this->tick(16);
    if (!this->bridge_.is_idle())
      this->bridge_.loop();
    if (!this->peer_.connected()) {
      int fd = ::accept(this->listen_fd_, nullptr, nullptr);
      if (fd >= 0) {
        this->accepts_++;
        this->peer_.adopt(std::make_unique<socket::Socket>(fd));
      }
    }
    if (this->peer_stream_->up(this->peer_)) {
      uint8_t buf[256];
      ssize_t n = this->peer_stream_->read(this->peer_, buf, sizeof(buf));
      if (n > 0)
        this->received_.append(reinterpret_cast<char *>(buf), static_cast<size_t>(n));
    }
    this->peer_stream_->flush(this->peer_);
  }
  void passes(int count) {
    for (int i = 0; i < count; i++)
      this->pass();
  }

  KeyFakeUart uart_;
  noise::NoiseStream bridge_stream_{uint32_t{7}, true};
  UartTcp bridge_;
  binary_sensor::BinarySensor sensor_;
  noise::NoiseStream key_peer_{KEY, false};
  noise::NoiseStream other_key_peer_{OTHER_KEY, false};
  noise::NoiseStream *peer_stream_{&key_peer_};
  socket::TcpClientLink peer_;
  std::string received_;
  int accepts_{0};
  int listen_fd_{-1};
  uint32_t now_{100000};
};

TEST_F(UartTcpKey, ClosedUntilAKeyIsSet) {
  // The loop stays off, so the client never dials
  EXPECT_TRUE(this->bridge_.is_idle());
  this->uart_.rx.assign({'e', 'a', 'r', 'l', 'y'});
  this->passes(10);
  EXPECT_EQ(this->accepts_, 0);
  EXPECT_FALSE(this->sensor_.state);

  this->bridge_.set_encryption_key(KEY_B64);
  for (int i = 0; i < 50 && !this->sensor_.state; i++)
    this->pass();
  ASSERT_TRUE(this->sensor_.state);
  EXPECT_EQ(this->accepts_, 1);
  // What the UART held while closed is dropped; live bytes cross
  this->uart_.rx.assign({'l', 'i', 'v', 'e'});
  for (int i = 0; i < 50 && this->received_.size() < 4; i++)
    this->pass();
  EXPECT_EQ(this->received_, "live");
}

TEST_F(UartTcpKey, ANewKeyEndsTheSession) {
  this->bridge_.set_encryption_key(KEY_B64);
  for (int i = 0; i < 50 && !this->sensor_.state; i++)
    this->pass();
  ASSERT_TRUE(this->sensor_.state);

  this->bridge_.set_encryption_key(OTHER_KEY_B64);
  this->pass();
  EXPECT_FALSE(this->sensor_.state);
  // The peer with the new key gets the next session
  for (int i = 0; i < 50 && this->peer_.connected(); i++)
    this->pass();
  this->peer_stream_ = &this->other_key_peer_;
  for (int i = 0; i < 100 && !this->sensor_.state; i++)
    this->pass();
  EXPECT_TRUE(this->sensor_.state);
  EXPECT_TRUE(this->other_key_peer_.ready());
  EXPECT_EQ(this->accepts_, 2);
}

}  // namespace esphome::uart_tcp::testing

#endif
