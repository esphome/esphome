#include "socket.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_SOCKET_IMPL_BSD_SOCKETS

#include <cstring>
#include "esphome/core/application.h"

#ifdef USE_ESP32
#include <esp_idf_version.h>
#include <lwip/sockets.h>
#endif

namespace esphome::socket {

class BSDSocketImpl final : public Socket {
 public:
  BSDSocketImpl(int fd, bool monitor_loop = false) : fd_(fd) {
#ifdef USE_SOCKET_SELECT_SUPPORT
    // Register new socket with the application for select() if monitoring requested
    if (monitor_loop && this->fd_ >= 0) {
      // Only set loop_monitored_ to true if registration succeeds
      this->loop_monitored_ = App.register_socket_fd(this->fd_);
    } else {
      this->loop_monitored_ = false;
    }
#else
    // Without select support, ignore monitor_loop parameter
    (void) monitor_loop;
#endif
  }
  ~BSDSocketImpl() override {
    if (!this->closed_) {
      this->close();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }
  }
  int connect(const struct sockaddr *addr, socklen_t addrlen) override { return ::connect(this->fd_, addr, addrlen); }
  std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) override {
    int fd = ::accept(this->fd_, addr, addrlen);
    if (fd == -1)
      return {};
    return make_unique<BSDSocketImpl>(fd, false);
  }
  std::unique_ptr<Socket> accept_loop_monitored(struct sockaddr *addr, socklen_t *addrlen) override {
    int fd = ::accept(this->fd_, addr, addrlen);
    if (fd == -1)
      return {};
    return make_unique<BSDSocketImpl>(fd, true);
  }

  int bind(const struct sockaddr *addr, socklen_t addrlen) override { return ::bind(this->fd_, addr, addrlen); }
  int close() override {
    if (!this->closed_) {
#ifdef USE_SOCKET_SELECT_SUPPORT
      // Unregister from select() before closing if monitored
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
  int shutdown(int how) override { return ::shutdown(this->fd_, how); }

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
#if defined(USE_ESP32) || defined(USE_HOST)
    return ::recvfrom(this->fd_, buf, len, 0, addr, addr_len);
#else
    return ::lwip_recvfrom(this->fd_, buf, len, 0, addr, addr_len);
#endif
  }
  ssize_t readv(const struct iovec *iov, int iovcnt) override {
#if defined(USE_ESP32)
    return ::lwip_readv(this->fd_, iov, iovcnt);
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

  int get_fd() const override { return this->fd_; }

#ifdef USE_SOCKET_SELECT_SUPPORT
  bool ready() const override {
    if (!this->loop_monitored_)
      return true;
    return App.is_socket_ready(this->fd_);
  }
#endif

 protected:
  int fd_;
  bool closed_{false};
#ifdef USE_SOCKET_SELECT_SUPPORT
  bool loop_monitored_{false};
#endif
};

// Helper to create a socket with optional monitoring
static std::unique_ptr<Socket> create_socket(int domain, int type, int protocol, bool loop_monitored = false) {
  int ret = ::socket(domain, type, protocol);
  if (ret == -1)
    return nullptr;
  return std::unique_ptr<Socket>{new BSDSocketImpl(ret, loop_monitored)};
}

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, false);
}

std::unique_ptr<Socket> socket_loop_monitored(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, true);
}

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_BSD_SOCKETS
