#include "tcp_listener.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <span>

namespace esphome::socket {

void TcpListener::try_listen_(TcpClientLink &link, uint8_t backlog) {
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
        this->listen_->listen(backlog) == 0) {
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

std::unique_ptr<Socket> TcpListener::accept(TcpClientLink &link) {
  if (this->listen_ == nullptr) {
    return nullptr;
  }
  struct sockaddr_storage peer{};
  socklen_t peer_len = sizeof(peer);
  auto client = this->listen_->accept_loop_monitored(reinterpret_cast<struct sockaddr *>(&peer), &peer_len);
  if (client == nullptr) {
    // Out of sockets or memory, or a reset/signal on that connection: try again next loop.
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED || errno == EINTR || errno == ENFILE ||
        errno == EMFILE || errno == ENOBUFS || errno == ENOMEM) {
      return nullptr;
    }
    // Rebuild the listener after the backoff instead of spinning on it.
    int err = errno;
    this->listen_.reset();
    ESP_LOGW(this->tag_, "Accept failed: %d", err);
    link.note_attempt();
    return nullptr;
  }
  const auto *sa = reinterpret_cast<const struct sockaddr *>(&peer);
#ifdef USE_SOCKET_IPV4_ALLOW
  if (!this->allow_.admit(sa, peer_len, this->tag_, App.get_loop_component_start_time())) {
    return nullptr;
  }
#endif
  char text[SOCKADDR_STR_LEN];
  format_sockaddr_to(sa, peer_len, std::span<char, SOCKADDR_STR_LEN>(text));
  ESP_LOGI(this->tag_, "Client connected from %s", text);
  return client;
}

void TcpListener::dump_config() const {
#ifdef USE_SOCKET_IPV4_ALLOW
  this->allow_.dump_config(this->tag_);
#endif
}

}  // namespace esphome::socket

#endif
