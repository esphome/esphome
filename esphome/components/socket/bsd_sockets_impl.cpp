#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "socket.h"

#ifdef USE_SOCKET_IMPL_BSD_SOCKETS

#include <cstring>
#include "esphome/core/application.h"

namespace esphome::socket {

BSDSocketImpl::BSDSocketImpl(int fd, bool monitor_loop) {
  this->fd_ = fd;
  if (!monitor_loop || this->fd_ < 0)
    return;
#ifdef USE_LWIP_FAST_SELECT
  // Cache lwip_sock pointer and register for monitoring (hooks callback internally)
  this->cached_sock_ = esphome_lwip_get_sock(this->fd_);
  this->loop_monitored_ = App.register_socket(this->cached_sock_);
#else
  this->loop_monitored_ = App.register_socket_fd(this->fd_);
#endif
}

BSDSocketImpl::~BSDSocketImpl() {
  if (!this->closed_) {
    this->close();
  }
}

int BSDSocketImpl::close() {
  if (!this->closed_) {
    // Unregister before closing to avoid dangling pointer in monitored set
#ifdef USE_LWIP_FAST_SELECT
    if (this->loop_monitored_) {
      App.unregister_socket(this->cached_sock_);
      this->cached_sock_ = nullptr;
    }
#else
    if (this->loop_monitored_) {
      App.unregister_socket_fd(this->fd_);
    }
#endif
    int ret = ::close(this->fd_);
    this->closed_ = true;
    return ret;
  }
  return 0;
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
