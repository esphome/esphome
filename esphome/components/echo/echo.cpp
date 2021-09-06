#include "echo.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/esphal.h"

namespace esphome {
namespace echo {

static const char *const TAG = "echo";

#ifdef USE_ECHO_SSL
void EchoServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up echo server...");
  socket_ = socket::socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket.");
    this->mark_failed();
    return;
  }
  int enable = 1;
  int err = socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = socket_->setblocking(false);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    this->mark_failed();
    return;
  }

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(6055);

  err = socket_->bind((struct sockaddr *) &server, sizeof(server));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to bind: errno %d", errno);
    this->mark_failed();
    return;
  }

  err = socket_->listen(4);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to listen: errno %d", errno);
    this->mark_failed();
    return;
  }

  ssl_ = ssl::create_context();
  if (!ssl_) {
    ESP_LOGW(TAG, "Failed to create SSL context: errno %d", errno);
    this->mark_failed();
    return;
  }
  ssl_->set_server_certificate(R"(-----BEGIN CERTIFICATE-----
MIIB3zCCAYWgAwIBAgIUZvjl3kvRTeMLlFUXR8Vbhw0UXnMwCgYIKoZIzj0EAwIw
RTELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGElu
dGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0yMTA4MTAxODA5NDNaFw0yNDA1MDYx
ODA5NDNaMEUxCzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYD
VQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAAQrW9JCZlDynrY1DphZGpDxV1jkfoVSTiBWQLWuixolu7aJuR3+o+BJ
ZrvPCdNHpEOyx7r1DV23SWSp1eIZR43co1MwUTAdBgNVHQ4EFgQUPmWned9/9QAq
TCnb3I8dou8plDkwHwYDVR0jBBgwFoAUPmWned9/9QAqTCnb3I8dou8plDkwDwYD
VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNIADBFAiBi375FEb+w297p0J/12lgp
iA9ppA4/QwtZdzioULmwVAIhALhGbVdbSAaLI+bwoICROHnuttY6mxJmDK8158Xe
s2U4
-----END CERTIFICATE-----
)");
  ssl_->set_private_key(R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEICqgqSPPEMmoWbwLpLm1lv4FQ48TsLOmXRbdceKs4DQ/oAoGCCqGSM49
AwEHoUQDQgAEK1vSQmZQ8p62NQ6YWRqQ8VdY5H6FUk4gVkC1rosaJbu2ibkd/qPg
SWa7zwnTR6RDsse69Q1dt0lkqdXiGUeN3A==
-----END EC PRIVATE KEY-----
)");
  err = ssl_->init();
  if (err != 0) {
    ESP_LOGW(TAG, "Failed to initialize SSL context: errno %d", errno);
    this->mark_failed();
    return;
  }
}

void EchoServer::loop() {
  // Accept new clients
  while (true) {
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    auto sock = socket_->accept((struct sockaddr *) &source_addr, &addr_len);
    if (!sock)
      break;
    ESP_LOGD(TAG, "Accepted %s", sock->getpeername().c_str());

    // wrap socket
    auto sock2 = ssl_->wrap_socket(std::move(sock));
    if (!sock2) {
      ESP_LOGW(TAG, "Failed to wrap socket with SSL: errno %d", errno);
      continue;
    }

    auto cli = std::unique_ptr<EchoClient>{new EchoClient(std::move(sock2))};
    cli->start();
    clients_.push_back(std::move(cli));
  }

  auto new_end = std::partition(this->clients_.begin(), this->clients_.end(),
                                [](const std::unique_ptr<EchoClient> &cli) { return !cli->remove_; });
  this->clients_.erase(new_end, this->clients_.end());

  for (auto &client : this->clients_) {
    client->loop();
  }
}

void EchoClient::start() {
  ESP_LOGD(TAG, "Starting socket");
  int err = socket_->setblocking(false);
  if (err != 0) {
    on_error_();
    ESP_LOGW(TAG, "Socket could not enable non-blocking, errno: %d", errno);
    return;
  }
  int enable = 1;
  err = socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
  if (err != 0) {
    on_error_();
    ESP_LOGW(TAG, "Socket could not enable tcp nodelay, errno: %d", errno);
    return;
  }

  rx_buffer_.resize(64);
}
void EchoClient::loop() {
  uint32_t start = millis();
  int err = socket_->loop();
  if (err != 0) {
    on_error_();
    ESP_LOGW(TAG, "Socket loop failed: errno %d", errno);
    return;
  }

  while (!this->remove_) {
    size_t capacity = this->rx_buffer_.size();

    ssize_t received = socket_->read(rx_buffer_.data(), capacity);
    if (received == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // read would block
        break;
      }
      if (errno == ECONNRESET) {
        // connection reset
        this->on_error_();
        ESP_LOGW(TAG, "Client disconnected");
        return;
      }
      this->on_error_();
      ESP_LOGW(TAG, "Error reading from socket: errno %d", errno);
      return;
    } else if (received == 0) {
      break;
    }
    ESP_LOGD(TAG, "received %s", hexencode(rx_buffer_.data(), received).c_str());
    tx_buffer_.insert(tx_buffer_.end(), rx_buffer_.begin(), rx_buffer_.begin() + received);

    if (received != capacity)
      // done with reading
      break;
  }

  while (!this->remove_ && !tx_buffer_.empty()) {
    int err = socket_->write(tx_buffer_.data(), tx_buffer_.size());
    if (err == -1) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        break;
      }
      if (errno == ECONNRESET) {
        this->on_error_();
        ESP_LOGW(TAG, "Client disconnected");
        return;
      }
      on_error_();
      ESP_LOGW(TAG, "Socket write failed: errno %d", errno);
      return;
    } else if (err == 0) {
      break;
    }
    tx_buffer_.erase(tx_buffer_.begin(), tx_buffer_.begin() + err);
  }

  uint32_t end = millis();
  if (end - start > 10) {
    ESP_LOGD(TAG, "loop took %u ms", end - start);
  }
}

void EchoClient::on_error_() {
  this->socket_->close();
  this->remove_ = true;
}
#endif

#ifdef USE_ECHO_NOISE
void EchoNoiseServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up echo server...");
  socket_ = socket::socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket.");
    this->mark_failed();
    return;
  }
  int enable = 1;
  int err = socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = socket_->setblocking(false);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    this->mark_failed();
    return;
  }

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(6056);

  err = socket_->bind((struct sockaddr *) &server, sizeof(server));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to bind: errno %d", errno);
    this->mark_failed();
    return;
  }

  err = socket_->listen(4);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to listen: errno %d", errno);
    this->mark_failed();
    return;
  }
}

void EchoNoiseServer::loop() {
  // Accept new clients
  while (true) {
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    auto sock = socket_->accept((struct sockaddr *) &source_addr, &addr_len);
    if (!sock)
      break;
    ESP_LOGD(TAG, "Accepted %s", sock->getpeername().c_str());

    auto cli = std::unique_ptr<EchoNoiseClient>{new EchoNoiseClient(std::move(sock))};
    cli->start();
    clients_.push_back(std::move(cli));
  }

  auto new_end = std::partition(this->clients_.begin(), this->clients_.end(),
                                [](const std::unique_ptr<EchoNoiseClient> &cli) { return !cli->remove_; });
  this->clients_.erase(new_end, this->clients_.end());

  for (auto &client : this->clients_) {
    client->loop();
  }
}

void EchoNoiseClient::start() {
  ESP_LOGD(TAG, "Starting socket");
  int err = socket_->setblocking(false);
  if (err != 0) {
    on_error_();
    ESP_LOGW(TAG, "Socket could not enable non-blocking, errno: %d", errno);
    return;
  }
  int enable = 1;
  err = socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
  if (err != 0) {
    on_error_();
    ESP_LOGW(TAG, "Socket could not enable tcp nodelay, errno: %d", errno);
    return;
  }

  memset(&nid_, 0, sizeof(nid_));
  const char *proto = "Noise_NNpsk0_25519_ChaChaPoly_SHA256";
  err = noise_protocol_name_to_id(&nid_, proto, strlen(proto));
  if (err != 0) {
    on_error_();
    ESP_LOGW(TAG, "noise_protocol_name_to_id failed: %d", err);
    return;
  }
  /*nid_.pattern_id = NOISE_PATTERN_NN;
  nid_.cipher_id = NOISE_CIPHER_CHACHAPOLY;
  nid_.dh_id = NOISE_DH_CURVE25519;
  nid_.prefix_id = NOISE_PREFIX_STANDARD;
  nid_.hybrid_id = NOISE_DH_NONE;
  nid_.hash_id = NOISE_HASH_SHA256;  // NOISE_HASH_BLAKE2s
  nid_.modifier_ids[0] = NOISE_MODIFIER_PSK0;*/

  err = noise_handshakestate_new_by_id(&handshake_, &nid_, NOISE_ROLE_RESPONDER);
  if (err != 0) {
    on_error_();
    ESP_LOGW(TAG, "noise_handshakestate_new_by_id failed: %d", err);
    return;
  }

  // initialize_handshake
  {
    const uint8_t psk[] = {0xC1, 0xD5, 0xE0, 0x72, 0xE7, 0x77, 0x58, 0x02, 0x45, 0xCB, 0x3A,
                           0x81, 0x04, 0x1B, 0x2D, 0x90, 0x3A, 0x0F, 0x0E, 0xC7, 0x9C, 0xFC,
                           0xB4, 0x2A, 0x50, 0xC0, 0xE6, 0x35, 0xA1, 0x54, 0x18, 0x12};
    static_assert(sizeof(psk) == 32, "error");
    // noise_handshakestate_set_prologue(handshake, prologue, strlen(prologue));
    err = noise_handshakestate_set_pre_shared_key(handshake_, psk, 32);

    if (err != 0) {
      on_error_();
      ESP_LOGW(TAG, "noise_handshakestate_set_pre_shared_key failed: %d", err);
      return;
    }
  }

  // Start the handshake
  err = noise_handshakestate_start(handshake_);
  if (err != 0) {
    on_error_();
    ESP_LOGW(TAG, "noise_handshakestate_start failed: %d", err);
    return;
  }

  rx_buffer_.resize(64);
  msg_buffer_.resize(64);
  do_handshake_ = true;
}

void EchoNoiseClient::loop() {
  if (this->remove_)
    return;

  const uint32_t start = millis();
  int err = socket_->loop();
  if (err != 0) {
    on_error_();
    ESP_LOGW(TAG, "Socket loop failed: errno %d", errno);
    return;
  }
  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);

  if (!this->remove_ && do_handshake_) {
    int action = noise_handshakestate_get_action(handshake_);
    if (action == NOISE_ACTION_WRITE_MESSAGE) {
      // Write the next handshake message with a zero-length payload
      noise_buffer_set_output(mbuf, msg_buffer_.data(), msg_buffer_.size());
      err = noise_handshakestate_write_message(handshake_, &mbuf, nullptr);
      if (err == 0) {
        tx_buffer_.push_back((uint8_t)(mbuf.size >> 8));
        tx_buffer_.push_back((uint8_t) mbuf.size);
        tx_buffer_.insert(tx_buffer_.end(), msg_buffer_.begin(), msg_buffer_.begin() + mbuf.size);
      } else {
        on_error_();
        ESP_LOGW(TAG, "noise_handshakestate_write_message failed: %d", err);
        return;
      }
    } else if (action == NOISE_ACTION_READ_MESSAGE) {
      if (rx_size_ >= 2) {
        uint16_t msg_size = ((uint16_t)(rx_buffer_[0]) << 8) | (rx_buffer_[1]);
        if (rx_size_ >= msg_size + 2) {
          ESP_LOGD(TAG, "Message: %s", hexencode(rx_buffer_.data() + 2, msg_size).c_str());
          noise_buffer_set_input(mbuf, rx_buffer_.data() + 2, msg_size);
          err = noise_handshakestate_read_message(handshake_, &mbuf, nullptr);
          if (err != 0) {
            on_error_();
            ESP_LOGW(TAG, "noise_handshakestate_read_message failed: %d", err);
            return;
          }
          rx_size_ -= msg_size + 2;
        }
      }
    } else if (action == NOISE_ACTION_SPLIT) {
      err = noise_handshakestate_split(handshake_, &send_cipher_, &recv_cipher_);
      if (err != 0) {
        on_error_();
        ESP_LOGW(TAG, "noise_handshakestate_split failed: %d", err);
        return;
      }
      noise_handshakestate_free(handshake_);
      do_handshake_ = false;
      ESP_LOGI(TAG, "handshake finished");
    } else {
      on_error_();
      ESP_LOGW(TAG, "noise_handshakestate_get_action failed: %d", err);
      return;
    }
  }

  while (!this->remove_ && !this->do_handshake_) {
    if (rx_size_ < 2)
      break;
    uint16_t msg_size = ((uint16_t)(rx_buffer_[0]) << 8) | (rx_buffer_[1]);
    if (rx_size_ < msg_size + 2)
      break;
    noise_buffer_set_inout(mbuf, rx_buffer_.data() + 2, msg_size, rx_buffer_.size() - 2);
    err = noise_cipherstate_decrypt(recv_cipher_, &mbuf);
    if (err != 0) {
      on_error_();
      ESP_LOGW(TAG, "noise_cipherstate_decrypt failed: %d", err);
      return;
    }
    rx_size_ -= msg_size + 2;

    err = noise_cipherstate_encrypt(send_cipher_, &mbuf);
    if (err != 0) {
      on_error_();
      ESP_LOGW(TAG, "noise_cipherstate_encrypt failed: %d", err);
      return;
    }
    tx_buffer_.push_back((uint8_t)(mbuf.size >> 8));
    tx_buffer_.push_back((uint8_t) mbuf.size);
    tx_buffer_.insert(tx_buffer_.end(), rx_buffer_.begin() + 2, rx_buffer_.begin() + 2 + mbuf.size);
  }

  while (!this->remove_) {
    size_t capacity = this->rx_buffer_.size();
    size_t used = rx_size_;
    size_t space = capacity - used;
    if (space == 0) {
      rx_buffer_.resize(capacity + 64);
      continue;
    }

    ssize_t received = socket_->read(rx_buffer_.data() + used, space);
    if (received == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // read would block
        break;
      }
      if (errno == ECONNRESET) {
        // connection reset
        this->on_error_();
        ESP_LOGW(TAG, "Client disconnected");
        return;
      }
      this->on_error_();
      ESP_LOGW(TAG, "Error reading from socket: errno %d", errno);
      return;
    } else if (received == 0) {
      break;
    }
    ESP_LOGD(TAG, "received %s", hexencode(rx_buffer_.data(), received).c_str());
    rx_size_ += received;

    if (received != capacity)
      // done with reading
      break;
  }

  while (!this->remove_ && !tx_buffer_.empty()) {
    int err = socket_->write(tx_buffer_.data(), tx_buffer_.size());
    if (err == -1) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        break;
      }
      if (errno == ECONNRESET) {
        this->on_error_();
        ESP_LOGW(TAG, "Client disconnected");
        return;
      }
      on_error_();
      ESP_LOGW(TAG, "Socket write failed: errno %d", errno);
      return;
    } else if (err == 0) {
      break;
    }
    tx_buffer_.erase(tx_buffer_.begin(), tx_buffer_.begin() + err);
  }

  const uint32_t end = millis();
  if (end - start > 10) {
    ESP_LOGD(TAG, "noise took %u ms", end - start);
  }
}

void EchoNoiseClient::on_error_() {
  this->socket_->close();
  this->remove_ = true;
}
#endif

}  // namespace echo
}  // namespace esphome

extern "C" {
void noise_rand_bytes(void *output, size_t len) { esp_fill_random(output, len); }
}
