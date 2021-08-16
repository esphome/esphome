#pragma once
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/socket/socket.h"
#ifdef USE_ECHO_SSL
#include "esphome/components/ssl/ssl_context.h"
#endif
#include "noise/protocol.h"

namespace esphome {
namespace echo {

#ifdef USE_ECHO_SSL
class EchoClient {
 public:
  EchoClient(std::unique_ptr<socket::Socket> socket) : socket_(std::move(socket)) {}
  void start();
  void loop();

 protected:
  friend class EchoServer;

  void on_error_();

  std::unique_ptr<socket::Socket> socket_ = nullptr;
  bool remove_ = false;
  std::vector<uint8_t> rx_buffer_;
  std::vector<uint8_t> tx_buffer_;
};

class EchoServer : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  friend class EchoClient;

  std::unique_ptr<socket::Socket> socket_ = nullptr;
  std::unique_ptr<ssl::SSLContext> ssl_ = nullptr;
  std::vector<std::unique_ptr<EchoClient>> clients_;
};
#endif

#ifdef USE_ECHO_NOISE
class EchoNoiseClient {
 public:
  EchoNoiseClient(std::unique_ptr<socket::Socket> socket) : socket_(std::move(socket)) {}
  void start();
  void loop();

 protected:
  friend class EchoNoiseServer;

  void on_error_();

  std::unique_ptr<socket::Socket> socket_ = nullptr;
  bool remove_ = false;
  std::vector<uint8_t> rx_buffer_;
  size_t rx_size_ = 0;
  std::vector<uint8_t> tx_buffer_;
  std::vector<uint8_t> msg_buffer_;

  NoiseHandshakeState *handshake_ = nullptr;
  NoiseCipherState *send_cipher_ = nullptr;
  NoiseCipherState *recv_cipher_ = nullptr;
  NoiseProtocolId nid_;
  bool do_handshake_ = false;
};

class EchoNoiseServer : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  friend class EchoNoiseClient;

  std::unique_ptr<socket::Socket> socket_ = nullptr;
  std::vector<std::unique_ptr<EchoNoiseClient>> clients_;
};
#endif

}  // namespace echo
}  // namespace esphome
