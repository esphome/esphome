#include "socket.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_SOCKET_IMPL_LWIP_SOCKETS

#include <cstring>
#include "esphome/core/application.h"

namespace esphome::socket {

class LwIPSocketImpl final : public Socket {
 public:
  LwIPSocketImpl(int fd, bool monitor_loop = false) : fd_(fd) {
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
  ~LwIPSocketImpl() override {
    if (!this->closed_) {
      this->close();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }
  }
  int connect(const struct sockaddr *addr, socklen_t addrlen) override {
    return lwip_connect(this->fd_, addr, addrlen);
  }
  std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) override {
    int fd = lwip_accept(this->fd_, addr, addrlen);
    if (fd == -1)
      return {};
    return make_unique<LwIPSocketImpl>(fd, false);
  }
  std::unique_ptr<Socket> accept_loop_monitored(struct sockaddr *addr, socklen_t *addrlen) override {
    int fd = lwip_accept(this->fd_, addr, addrlen);
    if (fd == -1)
      return {};
    return make_unique<LwIPSocketImpl>(fd, true);
  }

  int bind(const struct sockaddr *addr, socklen_t addrlen) override { return lwip_bind(this->fd_, addr, addrlen); }
  int close() override {
    if (!this->closed_) {
#ifdef USE_SOCKET_SELECT_SUPPORT
      // Unregister from select() before closing if monitored
      if (this->loop_monitored_) {
        App.unregister_socket_fd(this->fd_);
      }
#endif
      int ret = lwip_close(this->fd_);
      this->closed_ = true;
      return ret;
    }
    return 0;
  }
  int shutdown(int how) override { return lwip_shutdown(this->fd_, how); }

  int getpeername(struct sockaddr *addr, socklen_t *addrlen) override {
    return lwip_getpeername(this->fd_, addr, addrlen);
  }
  int getsockname(struct sockaddr *addr, socklen_t *addrlen) override {
    return lwip_getsockname(this->fd_, addr, addrlen);
  }
  int getsockopt(int level, int optname, void *optval, socklen_t *optlen) override {
    return lwip_getsockopt(this->fd_, level, optname, optval, optlen);
  }
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen) override {
    return lwip_setsockopt(this->fd_, level, optname, optval, optlen);
  }
  int listen(int backlog) override { return lwip_listen(this->fd_, backlog); }
  ssize_t read(void *buf, size_t len) override { return lwip_read(this->fd_, buf, len); }
  ssize_t recvfrom(void *buf, size_t len, sockaddr *addr, socklen_t *addr_len) override {
    return lwip_recvfrom(this->fd_, buf, len, 0, addr, addr_len);
  }
  ssize_t readv(const struct iovec *iov, int iovcnt) override { return lwip_readv(this->fd_, iov, iovcnt); }
  ssize_t write(const void *buf, size_t len) override { return lwip_write(this->fd_, buf, len); }
  ssize_t send(void *buf, size_t len, int flags) { return lwip_send(this->fd_, buf, len, flags); }
  ssize_t writev(const struct iovec *iov, int iovcnt) override { return lwip_writev(this->fd_, iov, iovcnt); }
  ssize_t sendto(const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) override {
    return lwip_sendto(this->fd_, buf, len, flags, to, tolen);
  }
  int setblocking(bool blocking) override {
    int fl = lwip_fcntl(this->fd_, F_GETFL, 0);
    if (blocking) {
      fl &= ~O_NONBLOCK;
    } else {
      fl |= O_NONBLOCK;
    }
    lwip_fcntl(this->fd_, F_SETFL, fl);
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
  int ret = lwip_socket(domain, type, protocol);
  if (ret == -1)
    return nullptr;
  return std::unique_ptr<Socket>{new LwIPSocketImpl(ret, loop_monitored)};
}

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, false);
}

std::unique_ptr<Socket> socket_loop_monitored(int domain, int type, int protocol) {
  return create_socket(domain, type, protocol, true);
}

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_SOCKETS
