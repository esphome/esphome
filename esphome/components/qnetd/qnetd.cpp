#include "qnetd.h"

#include <cerrno>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::qnetd {

static const char *const TAG = "qnetd";

uint64_t Qnetd::now_ms_() {
  // Extend the 32-bit loop clock to 64 bits so dead-peer deadlines survive
  // the 49.7-day wrap.
  uint32_t now = App.get_loop_component_start_time();
  if (now < this->last_millis_)
    this->millis_high_ += 0x100000000ULL;
  this->last_millis_ = now;
  return this->millis_high_ | now;
}

void Qnetd::setup() {
  this->listen_ = socket::socket_ip_loop_monitored(SOCK_STREAM, 0);
  if (this->listen_ == nullptr) {
    ESP_LOGE(TAG, "Could not create listening socket");
    this->mark_failed();
    return;
  }
  int enable = 1;
  this->listen_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (this->listen_->setblocking(false) != 0) {
    ESP_LOGE(TAG, "Could not make listening socket non-blocking: errno %d", errno);
    this->mark_failed();
    return;
  }
  struct sockaddr_storage server_addr;
  socklen_t sl =
      socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr), this->port_);
  if (sl == 0 || this->listen_->bind(reinterpret_cast<struct sockaddr *>(&server_addr), sl) != 0 ||
      this->listen_->listen(4) != 0) {
    ESP_LOGE(TAG, "Bind/listen on port %u failed: errno %d", this->port_, errno);
    this->mark_failed();
    return;
  }
}

void Qnetd::send_frame(int slot, const uint8_t *data, size_t len) {
  Connection &c = this->connections_[slot];
  if (c.sock == nullptr)
    return;
  if (c.tx.empty()) {
    ssize_t n = c.sock->write(data, len);
    if (n < 0) {
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        // we are inside a server callback, so the session is torn down from loop()
        c.failed = true;
        return;
      }
      n = 0;
    }
    if (static_cast<size_t>(n) < len)
      this->queue_tx_(c, data + n, len - n);
  } else {
    this->queue_tx_(c, data, len);
  }
}

void Qnetd::queue_tx_(Connection &c, const uint8_t *data, size_t len) {
  if (c.tx.size() + len > MAX_TX_PENDING) {
    c.failed = true;  // the peer stopped reading; torn down from loop()
    return;
  }
  c.tx.insert(c.tx.end(), data, data + len);
}

void Qnetd::close_connection(int slot) {
  Connection &c = this->connections_[slot];
  if (c.sock != nullptr)
    c.sock->close();
  c.sock = nullptr;
  c.tx.clear();
  c.failed = false;
}

void Qnetd::accept_connections_(uint64_t now) {
  while (true) {
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    auto sock = this->listen_->accept_loop_monitored(reinterpret_cast<struct sockaddr *>(&source_addr), &addr_len);
    if (sock == nullptr)
      break;
    if (sock->setblocking(false) != 0) {
      ESP_LOGW(TAG, "Could not make client socket non-blocking: errno %d", errno);
      sock->close();
      continue;
    }
    int nodelay = 1;
    sock->setsockopt(IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int));
    char peername[socket::SOCKADDR_STR_LEN];
    sock->getpeername_to(peername);
    int slot = this->server_.on_connect(now, peername);
    if (slot < 0) {
      sock->close();
      continue;
    }
    this->connections_[slot].sock = std::move(sock);
    this->connections_[slot].tx.clear();
    this->connections_[slot].failed = false;
  }
}

void Qnetd::service_connection_(int slot, uint64_t now) {
  Connection &c = this->connections_[slot];
  if (!c.tx.empty() && !c.failed) {
    ssize_t n = c.sock->write(c.tx.data(), c.tx.size());
    if (n > 0) {
      c.tx.erase(c.tx.begin(), c.tx.begin() + n);
    } else if (n < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
      c.failed = true;
    }
  }
  if (c.failed) {
    this->close_connection(slot);
    this->server_.on_disconnected(slot, now);
    return;
  }
  uint8_t buf[512];
  while (c.sock != nullptr) {
    ssize_t n = c.sock->read(buf, sizeof(buf));
    if (n > 0) {
      this->server_.on_data(slot, buf, static_cast<size_t>(n), now);
      continue;
    }
    if (n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
      break;
    // n == 0: orderly shutdown by the peer; otherwise a socket error
    this->close_connection(slot);
    this->server_.on_disconnected(slot, now);
  }
}

void Qnetd::loop() {
  uint64_t now = this->now_ms_();
  this->accept_connections_(now);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (this->connections_[i].sock != nullptr)
      this->service_connection_(i, now);
  }
  this->server_.tick(now);
  if (this->server_.consume_state_change())
    this->state_callback_.call();
}

void Qnetd::dump_config() {
  ESP_LOGCONFIG(TAG,
                "qnetd arbiter:\n"
                "  Port: %u\n"
                "  Algorithm: ffsplit\n"
                "  TLS: unsupported (plaintext)\n"
                "  Max clients: %d",
                this->port_, MAX_CLIENTS);
}

}  // namespace esphome::qnetd
