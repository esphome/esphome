#include <gtest/gtest.h>

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/core/preferences.h"

#if defined(USE_HOST) && defined(USE_NOISE_STREAM_RUNTIME_KEY)

#include "esphome/components/host/preferences.h"

namespace esphome::tcp_uart::testing {

// Bytes 1 to 32 and 2 to 33 in base64
static const std::string KEY_B64 = "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA=";
static const std::string OTHER_KEY_B64 = "AgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICE=";

class KeyUart : public TcpUart {
 public:
  KeyUart(uint32_t key_hash, bool initiator) : stream_(key_hash, initiator) {
    this->set_host("peer");
    this->set_port(1);
    this->set_noise_stream(&this->stream_);
    this->set_connected_sensor(&this->connected_);
    this->set_disconnects_sensor(&this->disconnects_);
  }
  socket::TcpClientLink &link() { return this->link_; }

  noise::NoiseStream stream_;
  binary_sensor::BinarySensor connected_;
  sensor::Sensor disconnects_;
};

class TcpUartKey : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    static bool prefs_ready = false;
    if (!prefs_ready) {
      // The host backend reads its file once, so no key saved by an earlier run is found
      std::string dir = (std::filesystem::temp_directory_path() / "tcp-uart-key-XXXXXX").string();
      ASSERT_NE(mkdtemp(dir.data()), nullptr);
      setenv("ESPHOME_PREFDIR", dir.c_str(), 1);
      host::setup_preferences();
      prefs_ready = true;
    }
    global_preferences->reset();
  }
  void TearDown() override {
    for (auto *end : {this->client_.get(), this->server_.get()}) {
      if (end != nullptr) {
        end->link().close();
      }
    }
    global_preferences->reset();
  }
  void make() {
    this->client_ = std::make_unique<KeyUart>(1, true);
    this->server_ = std::make_unique<KeyUart>(2, false);
    this->client_->setup();
    this->server_->setup();
  }
  void connect() {
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

  std::unique_ptr<KeyUart> client_;
  std::unique_ptr<KeyUart> server_;
};

TEST_F(TcpUartKey, ClosedUntilAKeyIsSet) {
  this->make();
  // The loop stays off: a client does not dial, a server does not listen
  EXPECT_TRUE(this->client_->is_idle());
  EXPECT_TRUE(this->server_->is_idle());
  EXPECT_FALSE(this->client_->is_connected());
  EXPECT_EQ(this->client_->available_for_write(), 0u);
  // A refused key changes nothing
  this->client_->set_encryption_key("AAECAw==");
  EXPECT_TRUE(this->client_->is_idle());

  this->client_->set_encryption_key(KEY_B64);
  this->server_->set_encryption_key(KEY_B64);
  EXPECT_TRUE(this->client_->is_in_loop_state());
  EXPECT_TRUE(this->server_->is_in_loop_state());
  this->connect();
  this->loops(5);
  EXPECT_TRUE(this->client_->is_connected());
  EXPECT_TRUE(this->server_->is_connected());
  EXPECT_TRUE(this->client_->connected_.state);
}

TEST_F(TcpUartKey, TheKeyOutlivesARestart) {
  this->make();
  this->client_->set_encryption_key(KEY_B64);
  this->client_ = std::make_unique<KeyUart>(1, true);
  this->client_->setup();
  EXPECT_FALSE(this->client_->is_idle());
  // Another entry has its own key
  KeyUart other(3, true);
  other.setup();
  EXPECT_TRUE(other.is_idle());
}

TEST_F(TcpUartKey, ANewKeyEndsTheSession) {
  this->make();
  this->client_->set_encryption_key(KEY_B64);
  this->server_->set_encryption_key(KEY_B64);
  this->connect();
  this->loops(5);
  ASSERT_TRUE(this->server_->is_connected());

  // The same key keeps the session
  this->client_->set_encryption_key(KEY_B64);
  this->loops(2);
  EXPECT_TRUE(this->client_->is_connected());

  this->client_->set_encryption_key(OTHER_KEY_B64);
  EXPECT_FALSE(this->client_->link().connected());
  this->loops(3);
  EXPECT_FALSE(this->client_->connected_.state);
  EXPECT_FLOAT_EQ(this->client_->disconnects_.state, 1);
  EXPECT_FALSE(this->server_->is_connected());
  EXPECT_FLOAT_EQ(this->server_->disconnects_.state, 1);

  // With the new key on both ends the next connection has a session
  this->server_->set_encryption_key(OTHER_KEY_B64);
  this->connect();
  this->loops(5);
  EXPECT_TRUE(this->client_->is_connected());
  EXPECT_TRUE(this->server_->is_connected());
}

}  // namespace esphome::tcp_uart::testing

#endif
