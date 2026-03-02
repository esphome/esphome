#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "socket.h"

#ifdef USE_SOCKET_IMPL_LWIP_SOCKETS

#include <cstring>
#include "esphome/core/application.h"

namespace esphome::socket {

LwIPSocketImpl::LwIPSocketImpl(int fd, bool monitor_loop) {
  this->fd_ = fd;
  // Register new socket with the application for select() if monitoring requested
  if (monitor_loop && this->fd_ >= 0) {
    // Only set loop_monitored_ to true if registration succeeds
    this->loop_monitored_ = App.register_socket_fd(this->fd_);
  }
}

LwIPSocketImpl::~LwIPSocketImpl() {
  if (!this->closed_) {
    this->close();
  }
}

int LwIPSocketImpl::close() {
  if (!this->closed_) {
    // Unregister from select() before closing if monitored
    if (this->loop_monitored_) {
      App.unregister_socket_fd(this->fd_);
    }
    int ret = lwip_close(this->fd_);
    this->closed_ = true;
    return ret;
  }
  return 0;
}

int LwIPSocketImpl::setblocking(bool blocking) {
  int fl = lwip_fcntl(this->fd_, F_GETFL, 0);
  if (blocking) {
    fl &= ~O_NONBLOCK;
  } else {
    fl |= O_NONBLOCK;
  }
  lwip_fcntl(this->fd_, F_SETFL, fl);
  return 0;
}

bool LwIPSocketImpl::ready() const { return socket_ready_fd(this->fd_, this->loop_monitored_); }

size_t LwIPSocketImpl::getpeername_to(std::span<char, SOCKADDR_STR_LEN> buf) {
  struct sockaddr_storage storage;
  socklen_t len = sizeof(storage);
  if (this->getpeername(reinterpret_cast<struct sockaddr *>(&storage), &len) != 0) {
    buf[0] = '\0';
    return 0;
  }
  return format_sockaddr_to(reinterpret_cast<struct sockaddr *>(&storage), len, buf);
}

size_t LwIPSocketImpl::getsockname_to(std::span<char, SOCKADDR_STR_LEN> buf) {
  struct sockaddr_storage storage;
  socklen_t len = sizeof(storage);
  if (this->getsockname(reinterpret_cast<struct sockaddr *>(&storage), &len) != 0) {
    buf[0] = '\0';
    return 0;
  }
  return format_sockaddr_to(reinterpret_cast<struct sockaddr *>(&storage), len, buf);
}

// Helper to create a socket with optional monitoring
static std::unique_ptr<LwIPSocketImpl> create_socket(int domain, int type, int protocol, bool loop_monitored = false) {
  int ret = lwip_socket(domain, type, protocol);
  if (ret == -1)
    return nullptr;
  return make_unique<LwIPSocketImpl>(ret, loop_monitored);
}

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, false);
}

std::unique_ptr<Socket> socket_loop_monitored(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, true);
}

std::unique_ptr<ListenSocket> socket_listen(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, false);
}

std::unique_ptr<ListenSocket> socket_listen_loop_monitored(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, true);
}

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_SOCKETS
