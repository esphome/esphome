#include "tcp_listener.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <span>

namespace esphome::socket {

// One client at a time; a second connection waits in the stack until the first drops.
static constexpr int LISTEN_BACKLOG = 1;
#ifdef USE_SOCKET_IPV4_ALLOW
static constexpr uint32_t REJECT_LOG_INTERVAL_MS = 5000;
#endif

void TcpListener::try_listen_(TcpClientLink &link) {
  this->listen_ = socket_ip_loop_monitored(SOCK_STREAM, IPPROTO_TCP);
  int err = errno;
  if (this->listen_ != nullptr) {
    int yes = 1;
    this->listen_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_storage local;
    socklen_t local_len = set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&local), sizeof(local), link.port());
    // A blocking listener would stall loop() inside accept(), so its
    // setblocking result is part of the success condition.
    if (this->listen_->setblocking(false) == 0 && local_len != 0 &&
        this->listen_->bind(reinterpret_cast<struct sockaddr *>(&local), local_len) == 0 &&
        this->listen_->listen(LISTEN_BACKLOG) == 0) {
      ESP_LOGI(this->tag_, "Listening on %u", link.port());
      return;
    }
    // Captured before reset(); the close inside can overwrite errno.
    err = errno;
    this->listen_.reset();
  }
  ESP_LOGW(this->tag_, "Listen on %u failed: %d", link.port(), err);
  link.note_attempt();
}

void TcpListener::accept_(TcpClientLink &link) {
  struct sockaddr_storage peer {};
  socklen_t peer_len = sizeof(peer);
  auto client = this->listen_->accept_loop_monitored(reinterpret_cast<struct sockaddr *>(&peer), &peer_len);
  if (client == nullptr) {
    // A reset during the handshake or a signal only affects that connection.
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED || errno == EINTR) {
      return;
    }
    // Rebuild the listener after the backoff instead of spinning on it.
    int err = errno;
    this->listen_.reset();
    ESP_LOGW(this->tag_, "Accept failed: %d", err);
    link.note_attempt();
    return;
  }
  const auto *sa = reinterpret_cast<const struct sockaddr *>(&peer);
  char text[SOCKADDR_STR_LEN];
  format_sockaddr_to(sa, peer_len, std::span<char, SOCKADDR_STR_LEN>(text));
#ifdef USE_SOCKET_IPV4_ALLOW
  if (!this->allow_.allows(sa)) {
    uint32_t now = App.get_loop_component_start_time();
    if (this->last_reject_log_ms_ == 0 || now - this->last_reject_log_ms_ >= REJECT_LOG_INTERVAL_MS) {
      this->last_reject_log_ms_ = now;
      ESP_LOGW(this->tag_, "Rejected %s", text);
    }
    return;
  }
#endif
  link.adopt(std::move(client));
  ESP_LOGI(this->tag_, "Client connected from %s", text);
}

void TcpListener::dump_config() const {
#ifdef USE_SOCKET_IPV4_ALLOW
  for (size_t i = 0; i < this->allow_.size(); i++) {
    Ipv4AllowEntry e = this->allow_.entry(i);
    // Network order is dotted order, and the contiguous mask's popcount is the prefix.
    const auto *b = reinterpret_cast<const uint8_t *>(&e.addr);
    ESP_LOGCONFIG(this->tag_, "  Allowed IP: %u.%u.%u.%u/%u", b[0], b[1], b[2], b[3],
                  static_cast<unsigned>(__builtin_popcount(e.mask)));
  }
#endif
}

}  // namespace esphome::socket

#endif
