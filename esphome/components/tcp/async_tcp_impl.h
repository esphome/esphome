#pragma once

#include "esphome/core/defines.h"
#ifdef USE_TCP_ASYNC_TCP

#include "tcp.h"
#include "esphome/core/optional.h"
#include "ring_buffer.h"
#include <queue>
#include <memory>
#include "Arduino.h"

#ifdef ARDUINO_ARCH_ESP32
#include <AsyncTCP.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <ESPAsyncTCP.h>
#endif


namespace esphome {
namespace tcp {

/** An implementation of our TCPClient using raw LWIP calls.
 *
 * See also https://www.nongnu.org/lwip/2_0_x/tcp_8c.html and https://www.nongnu.org/lwip/2_0_x/raw_api.html
 */
class AsyncTCPImpl : public TCPClient {
 public:
  AsyncTCPImpl() {
    this->client_ = new AsyncClient();
  }
  ~AsyncTCPImpl() override {
    delete this->client_;
  }
  bool connect(const std::string &host, uint16_t port) override {
    this->host_ = host;
    this->setup_callbacks_();

    bool ret = this->client_->connect(host.c_str(), port);
    this->connect_called_ = true;
    return ret;
  }
  bool connect(IPAddress ip, uint16_t port) override {
    this->host_ = ip.toString().c_str();
    this->setup_callbacks_();

    bool ret = this->client_->connect(ip, port);
    this->connect_called_ = true;
    return ret;
  }

  void close(bool force) override {
    this->client_->close(force);
  }

  void set_no_delay(bool no_delay) override {
    this->client_->setNoDelay(no_delay);
  }
  IPAddress get_remote_address() override {
    return this->client_->remoteIP();
  }
  uint16_t get_remote_port() override {
    return this->remote_port_;
  }
  IPAddress get_local_address() override {
    return this->client_->localIP();
  }
  uint16_t get_local_port() override {
    return this->client_->localPort();
  }

  size_t available() override { return this->rx_buffer_.size(); }
  bool read(uint8_t *destination_buffer, size_t size) override;
  size_t available_for_write() override { return this->reserve_buffer_.size() + this->client_->space(); }
  bool write(const uint8_t *buffer, size_t size) override;
  bool flush() override;

  void set_timeout(uint32_t timeout_ms) override {
    this->client_->setRxTimeout(timeout_ms);
  }
  void reserve_at_least(size_t size) override {
    if (size <= TCP_SND_BUF)
      return;
    this->reserve_buffer_.reserve(size - TCP_SND_BUF);
  }
  void ensure_capacity(size_t size) override {
    size_t avail = this->available_for_write();
    size_t sndbuf = this->client_->space();
    if (size <= avail || size <= sndbuf)
      return;
    this->reserve_buffer_.reserve(size - sndbuf);
  }
  void loop();
  State state() override {
    if (this->connect_called_)
      return STATE_INITIALIZED;
    if (this->client_->connecting())
      return STATE_CONNECTING;
    if (this->client_->connected())
      return STATE_CONNECTED;
    if (this->client_->disconnecting())
      return STATE_CLOSING;
    return STATE_CLOSED;
  }
  std::string get_host() override { return this->host_; }

 protected:
  void setup_callbacks_() {
    this->client_->onConnect([](void *arg, AsyncClient *client) {
      ((AsyncTCPImpl *) arg)->on_connect_cb_();
    }, this);
    this->client_->onDisconnect([](void *arg, AsyncClient *client) {
      ((AsyncTCPImpl *) arg)->on_disconnect_cb_();
    }, this);
    this->client_->onAck([](void *arg, AsyncClient *client, size_t len, uint32_t time) {
      ((AsyncTCPImpl *) arg)->on_ack_cb_(len, time);
    }, this);
    this->client_->onError([](void *arg, AsyncClient *client, int8_t error) {
      ((AsyncTCPImpl *) arg)->on_error_cb_(error);
    }, this);
    this->client_->onData([](void *arg, AsyncClient *client, void *data, size_t len) {
      ((AsyncTCPImpl *) arg)->on_data_cb_(reinterpret_cast<uint8_t *>(data), len);
    }, this);
    this->client_->onTimeout([](void *arg, AsyncClient *client, uint32_t time) {
      ((AsyncTCPImpl *) arg)->on_timeout_cb_(time);
    }, this);
  }
  size_t write_internal_(const uint8_t *buffer, size_t size, bool psh_flag);
  bool drain_reserve_buffer_();

  // Callbacks:
  void on_connect_cb_();
  void on_disconnect_cb_();
  void on_ack_cb_(size_t len, uint32_t time);
  void on_error_cb_(int8_t error);
  void on_data_cb_(uint8_t *data, size_t len);
  void on_timeout_cb_(uint32_t time);

  friend class AsyncTCPServerImpl;
  AsyncTCPImpl(AsyncClient *client) {
    this->client_ = client;
    this->setup_callbacks_();
    this->host_ = this->get_remote_address().toString().c_str();
    this->remote_port_ = client->remotePort();
  }

  AsyncClient *client_;
  RingBuffer rx_buffer_;

  bool connect_called_{false};

  /// The host we're connecting to.
  std::string host_;
  uint16_t remote_port_;
  /// A buffer in which we store TX data that didn't fit in lwIP's buffers.
  RingBuffer reserve_buffer_;
  State last_state_{STATE_INITIALIZED};
};

class AsyncTCPServerImpl : public TCPServer {
 public:
  bool bind(uint16_t port) override {
    this->server_ = new AsyncServer(port);
    this->server_->onClient([](void *arg, AsyncClient *client) {
      ((AsyncTCPServerImpl *) arg)->on_accept_cb_(client);
    }, this);
    this->server_->begin();
    return true;
  }
  std::unique_ptr<TCPSocket> accept() override {
    if (this->accepted_.empty())
      return std::unique_ptr<TCPSocket>();

    auto *socket = this->accepted_.front();
    this->accepted_.pop();
    return std::unique_ptr<TCPSocket>(socket);
  }
  void close(bool force) override {
    this->server_->end();
  }
 protected:
  void on_accept_cb_(AsyncClient *client);

  /// A queue of accepted TCP client connections waiting to be processed by the user.
  std::queue<AsyncTCPImpl *> accepted_;
  AsyncServer *server_{nullptr};
};

}  // namespace tcp
}  // namespace esphome

#endif  // USE_TCP_ASYNC_TCP
