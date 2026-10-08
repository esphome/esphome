// ESPHome glue for the qnetd core: TCP listener on the socket abstraction
// (same pattern as the native API server) and the loop() pump. All protocol
// logic lives in qnetd_server.{h,cpp}.
#pragma once
#include <array>
#include <memory>
#include <span>
#include <vector>

#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "qnetd_server.h"

namespace esphome::qnetd {

class Qnetd final : public Component, public QnetdTransport {
 public:
  explicit Qnetd(uint16_t port) : port_(port) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Arbiter state, for entity platforms. The callback fires from loop() after
  // anything observable changed (connections, votes).
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }
  int connected_clients() const { return this->server_.connected_clients(); }
  uint32_t decisions() const { return this->server_.decisions(); }
  bool vote_granted() const { return this->server_.any_ack(); }
  size_t status_to(std::span<char> buf) const { return this->server_.status_to(buf); }

  // QnetdTransport
  void send_frame(int slot, const uint8_t *data, size_t len) override;
  void close_connection(int slot) override;

 protected:
  // Unsent output allowed to queue while the peer stops reading; beyond this
  // the connection is dropped instead of growing the heap.
  static constexpr size_t MAX_TX_PENDING = 2048;

  struct Connection {
    std::unique_ptr<socket::Socket> sock;
    std::vector<uint8_t> tx;  // unsent remainder after a partial write
    bool failed{false};       // a write hit a fatal error; torn down on the next loop pass
  };

  uint64_t now_ms_();
  void accept_connections_(uint64_t now);
  void service_connection_(int slot, uint64_t now);
  void queue_tx_(Connection &c, const uint8_t *data, size_t len);

  uint16_t port_;
  std::unique_ptr<socket::ListenSocket> listen_;
  std::array<Connection, MAX_CLIENTS> connections_;
  QnetdServer server_{this};
  CallbackManager<void()> state_callback_;
  uint32_t last_millis_{0};
  uint64_t millis_high_{0};
};

}  // namespace esphome::qnetd
