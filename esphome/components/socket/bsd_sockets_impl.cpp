#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "socket.h"

#ifdef USE_SOCKET_IMPL_BSD_SOCKETS

#include <cstring>
#include "esphome/core/application.h"
#ifdef USE_HOST
#include "esphome/core/wake.h"
#endif

namespace esphome::socket {

BSDSocketImpl::BSDSocketImpl(int fd, bool monitor_loop) {
  this->fd_ = fd;
  if (this->fd_ < 0)
    return;
#ifdef USE_HOST
  // Release listening ports on OTA re-exec.
  int flags = ::fcntl(this->fd_, F_GETFD, 0);
  if (flags >= 0)
    ::fcntl(this->fd_, F_SETFD, flags | FD_CLOEXEC);
#endif
  if (!monitor_loop)
    return;
#ifdef USE_LWIP_FAST_SELECT
  this->cached_sock_ = hook_fd_for_fast_select(this->fd_);
#else
  this->loop_monitored_ = wake_register_fd(this->fd_);
#endif
}

BSDSocketImpl::~BSDSocketImpl() { this->close(); }

int BSDSocketImpl::close() {
  if (this->fd_ < 0) {
    // Already closed, or never opened.
    return 0;
  }
#ifdef USE_LWIP_FAST_SELECT
  // Null the cached lwip_sock pointer before closing. The underlying lwip slot can be
  // recycled for a new connection as soon as ::close() returns, so anything that might
  // dereference cached_sock_ post-close (e.g. setsockopt(TCP_NODELAY)) would otherwise
  // touch an unrelated socket's pcb. No per-socket callback unhook is needed —
  // all LwIP sockets share the same static event_callback.
  this->cached_sock_ = nullptr;
#else
  if (this->loop_monitored_) {
    wake_unregister_fd(this->fd_);
  }
#endif
  int ret = ::close(this->fd_);
  this->fd_ = -1;  // Sentinel for "closed" — prevents double-close and makes use-after-close visible.
  return ret;
}

int BSDSocketImpl::setblocking(bool blocking) {
  int fl = ::fcntl(this->fd_, F_GETFL, 0);
  if (blocking) {
    fl &= ~O_NONBLOCK;
  } else {
    fl |= O_NONBLOCK;
  }
  ::fcntl(this->fd_, F_SETFL, fl);
  return 0;
}

size_t BSDSocketImpl::getpeername_to(std::span<char, SOCKADDR_STR_LEN> buf) {
  struct sockaddr_storage storage;
  socklen_t len = sizeof(storage);
  if (this->getpeername(reinterpret_cast<struct sockaddr *>(&storage), &len) != 0) {
    buf[0] = '\0';
    return 0;
  }
  return format_sockaddr_to(reinterpret_cast<struct sockaddr *>(&storage), len, buf);
}

size_t BSDSocketImpl::getsockname_to(std::span<char, SOCKADDR_STR_LEN> buf) {
  struct sockaddr_storage storage;
  socklen_t len = sizeof(storage);
  if (this->getsockname(reinterpret_cast<struct sockaddr *>(&storage), &len) != 0) {
    buf[0] = '\0';
    return 0;
  }
  return format_sockaddr_to(reinterpret_cast<struct sockaddr *>(&storage), len, buf);
}

// Helper to create a socket with optional monitoring
static std::unique_ptr<BSDSocketImpl> create_socket(int domain, int type, int protocol, bool loop_monitored = false) {
  int ret = ::socket(domain, type, protocol);
  if (ret == -1)
    return nullptr;
  return make_unique<BSDSocketImpl>(ret, loop_monitored);
}

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, false);
}

std::unique_ptr<Socket> socket_loop_monitored(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, true);
}

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_BSD_SOCKETS
