#include "sndbuf_pin_component.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>

#ifdef __linux__
#include <linux/sockios.h>
#include <sys/ioctl.h>
#endif

#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"

namespace esphome::sndbuf_pin {

static const char *const TAG = "sndbuf_pin";

// Skip stdio; scan the low fd range where the listeners land
static constexpr int FIRST_USER_FD = 3;
static constexpr int MAX_FD_SCAN = 128;

void SndbufPinComponent::setup() {
  int pinned = 0;
  for (int fd = FIRST_USER_FD; fd < MAX_FD_SCAN; fd++) {
    int type = 0;
    socklen_t len = sizeof(type);
    if (::getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &len) != 0 || type != SOCK_STREAM)
      continue;
    struct sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) != 0) {
      ESP_LOGW(TAG, "fd %d: getsockname failed, errno %d", fd, errno);
      continue;
    }
    if (ntohs(addr.sin_port) != api::global_api_server->get_port())
      continue;
    if (::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &this->buffer_size_, sizeof(this->buffer_size_)) != 0) {
      ESP_LOGW(TAG, "fd %d: SO_SNDBUF pin failed, errno %d", fd, errno);
      continue;
    }
    int applied = 0;
    len = sizeof(applied);
    if (::getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &applied, &len) != 0 || applied < this->buffer_size_) {
      // Linux doubles the requested value; anything below it means clamped
      ESP_LOGW(TAG, "fd %d: SO_SNDBUF readback %d below requested %d", fd, applied, this->buffer_size_);
      continue;
    }
    // Tests assert on this line; accepted sockets inherit the pinned size
    ESP_LOGD(TAG, "fd %d port %d: SO_SNDBUF pinned to %d (effective %d)", fd, ntohs(addr.sin_port), this->buffer_size_,
             applied);
    pinned++;
  }
  if (pinned == 0) {
    ESP_LOGE(TAG, "api listener socket was not pinned");
    this->mark_failed();
  }
}

api::APIConnection *api_log_client() {
  for (const auto &conn : api::global_api_server->active_clients()) {
    if (conn->get_log_subscription_level() > 0)
      return conn.get();
  }
  return nullptr;
}

#ifdef __linux__
bool api_peer_window_closed() {
  for (int fd = FIRST_USER_FD; fd < MAX_FD_SCAN; fd++) {
    struct sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) != 0 ||
        ntohs(addr.sin_port) != api::global_api_server->get_port())
      continue;
    // Skips the listener: only the accepted client socket has a peer
    addr_len = sizeof(addr);
    if (::getpeername(fd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) != 0)
      continue;
    int queued = 0;
    int not_sent = 0;
    if (::ioctl(fd, SIOCOUTQ, &queued) != 0 || ::ioctl(fd, SIOCOUTQNSD, &not_sent) != 0)
      return false;
    return not_sent > 0 && queued == not_sent;
  }
  return false;
}
#endif

}  // namespace esphome::sndbuf_pin
