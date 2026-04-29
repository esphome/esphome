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
  if (!monitor_loop || this->fd_ < 0)
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

  int getpeername(struct sockaddr *addr, socklen_t *addrlen) override {
    return ::getpeername(this->fd_, addr, addrlen);
  }
  int getsockname(struct sockaddr *addr, socklen_t *addrlen) override {
    return ::getsockname(this->fd_, addr, addrlen);
  }
  int getsockopt(int level, int optname, void *optval, socklen_t *optlen) override {
    return ::getsockopt(this->fd_, level, optname, optval, optlen);
  }
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen) override {
    return ::setsockopt(this->fd_, level, optname, optval, optlen);
  }
  int listen(int backlog) override { return ::listen(this->fd_, backlog); }
  ssize_t read(void *buf, size_t len) override {
#ifdef USE_ESP32
    return ::lwip_read(this->fd_, buf, len);
#else
    return ::read(this->fd_, buf, len);
#endif
  }
  ssize_t recvfrom(void *buf, size_t len, sockaddr *addr, socklen_t *addr_len) override {
#if defined(USE_ESP32) || defined(USE_HOST) || defined(USE_ZEPHYR)
    return ::recvfrom(this->fd_, buf, len, 0, addr, addr_len);
#else
    return ::lwip_recvfrom(this->fd_, buf, len, 0, addr, addr_len);
#endif
  }
  ssize_t readv(const struct iovec *iov, int iovcnt) override {
#if defined(USE_ESP32)
    return ::lwip_readv(this->fd_, iov, iovcnt);
#elif defined(USE_ZEPHYR)
    struct msghdr msg = {
        .msg_iov = const_cast<struct iovec *>(iov),
        .msg_iovlen = static_cast<size_t>(iovcnt),
    };
    return ::zsock_recvmsg(this->fd_, &msg, 0);
#else
    return ::readv(this->fd_, iov, iovcnt);
#endif
  }
  ssize_t write(const void *buf, size_t len) override {
#ifdef USE_ESP32
    return ::lwip_write(this->fd_, buf, len);
#else
    return ::write(this->fd_, buf, len);
#endif
  }
  ssize_t send(void *buf, size_t len, int flags) { return ::send(this->fd_, buf, len, flags); }
  ssize_t writev(const struct iovec *iov, int iovcnt) override {
#if defined(USE_ESP32)
    return ::lwip_writev(this->fd_, iov, iovcnt);
#elif defined(USE_ZEPHYR)
    struct msghdr msg = {
        .msg_iov = const_cast<struct iovec *>(iov),
        .msg_iovlen = static_cast<size_t>(iovcnt),
    };
    return ::zsock_sendmsg(this->fd_, &msg, 0);
#else
    return ::writev(this->fd_, iov, iovcnt);
#endif
  }

  ssize_t sendto(const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) override {
    return ::sendto(this->fd_, buf, len, flags, to, tolen);  // NOLINT(readability-suspicious-call-argument)
  }

  int setblocking(bool blocking) override {
    int fl = ::fcntl(this->fd_, F_GETFL, 0);
    if (blocking) {
      fl &= ~O_NONBLOCK;
    } else {
      fl |= O_NONBLOCK;
    }
    ::fcntl(this->fd_, F_SETFL, fl);
    return 0;
  }
};

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
