#include "socket.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_SOCKET_IMPL_LWIP_SOCKETS

#include <cstring>
#include "esphome/core/application.h"

namespace esphome {
namespace socket {

std::string format_sockaddr(const struct sockaddr_storage &storage) {
  if (storage.ss_family == AF_INET) {
    const struct sockaddr_in *addr = reinterpret_cast<const struct sockaddr_in *>(&storage);
    char buf[INET_ADDRSTRLEN];
    const char *ret = lwip_inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
    if (ret == nullptr)
      return {};
    return std::string{buf};
  }
#if LWIP_IPV6
  else if (storage.ss_family == AF_INET6) {
    const struct sockaddr_in6 *addr = reinterpret_cast<const struct sockaddr_in6 *>(&storage);
    char buf[INET6_ADDRSTRLEN];
    const char *ret = lwip_inet_ntop(AF_INET6, &addr->sin6_addr, buf, sizeof(buf));
    if (ret == nullptr)
      return {};
    return std::string{buf};
  }
#endif
  return {};
}

class LwIPSocketImpl : public Socket {
 public:
  LwIPSocketImpl(int fd, bool monitor_loop = false) : fd_(fd) {
#ifdef USE_SOCKET_SELECT_SUPPORT
    // Register new socket with the application for select() if monitoring requested
    if (monitor_loop && fd_ >= 0) {
      // Only set loop_monitored_ to true if registration succeeds
      loop_monitored_ = App.register_socket_fd(fd_);
    } else {
      loop_monitored_ = false;
    }
#else
    // Without select support, ignore monitor_loop parameter
    (void) monitor_loop;
#endif
  }
  ~LwIPSocketImpl() override {
    if (!closed_) {
      close();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }
  }
  int connect(const struct sockaddr *addr, socklen_t addrlen) override { return lwip_connect(fd_, addr, addrlen); }
  std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) override {
    return accept_impl_(addr, addrlen, false);
  }
  std::unique_ptr<Socket> accept_loop_monitored(struct sockaddr *addr, socklen_t *addrlen) override {
    return accept_impl_(addr, addrlen, true);
  }

 private:
  std::unique_ptr<Socket> accept_impl_(struct sockaddr *addr, socklen_t *addrlen, bool loop_monitored) {
    int fd = lwip_accept(fd_, addr, addrlen);
    if (fd == -1)
      return {};
    return make_unique<LwIPSocketImpl>(fd, loop_monitored);
  }

 public:
  int bind(const struct sockaddr *addr, socklen_t addrlen) override { return lwip_bind(fd_, addr, addrlen); }
  int close() override {
    if (!closed_) {
#ifdef USE_SOCKET_SELECT_SUPPORT
      // Unregister from select() before closing if monitored
      if (loop_monitored_) {
        App.unregister_socket_fd(fd_);
      }
#endif
      int ret = lwip_close(fd_);
      closed_ = true;
      return ret;
    }
    return 0;
  }
  int shutdown(int how) override { return lwip_shutdown(fd_, how); }

  int getpeername(struct sockaddr *addr, socklen_t *addrlen) override { return lwip_getpeername(fd_, addr, addrlen); }
  std::string getpeername() override {
    struct sockaddr_storage storage;
    socklen_t len = sizeof(storage);
    int err = this->getpeername((struct sockaddr *) &storage, &len);
    if (err != 0)
      return {};
    return format_sockaddr(storage);
  }
  int getsockname(struct sockaddr *addr, socklen_t *addrlen) override { return lwip_getsockname(fd_, addr, addrlen); }
  std::string getsockname() override {
    struct sockaddr_storage storage;
    socklen_t len = sizeof(storage);
    int err = this->getsockname((struct sockaddr *) &storage, &len);
    if (err != 0)
      return {};
    return format_sockaddr(storage);
  }
  int getsockopt(int level, int optname, void *optval, socklen_t *optlen) override {
    return lwip_getsockopt(fd_, level, optname, optval, optlen);
  }
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen) override {
    return lwip_setsockopt(fd_, level, optname, optval, optlen);
  }
  int listen(int backlog) override { return lwip_listen(fd_, backlog); }
  ssize_t read(void *buf, size_t len) override { return lwip_read(fd_, buf, len); }
  ssize_t recvfrom(void *buf, size_t len, sockaddr *addr, socklen_t *addr_len) override {
    return lwip_recvfrom(fd_, buf, len, 0, addr, addr_len);
  }
  ssize_t readv(const struct iovec *iov, int iovcnt) override { return lwip_readv(fd_, iov, iovcnt); }
  ssize_t write(const void *buf, size_t len) override { return lwip_write(fd_, buf, len); }
  ssize_t send(void *buf, size_t len, int flags) { return lwip_send(fd_, buf, len, flags); }
  ssize_t writev(const struct iovec *iov, int iovcnt) override { return lwip_writev(fd_, iov, iovcnt); }
  ssize_t sendto(const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) override {
    return lwip_sendto(fd_, buf, len, flags, to, tolen);
  }
  int setblocking(bool blocking) override {
    int fl = lwip_fcntl(fd_, F_GETFL, 0);
    if (blocking) {
      fl &= ~O_NONBLOCK;
    } else {
      fl |= O_NONBLOCK;
    }
    lwip_fcntl(fd_, F_SETFL, fl);
    return 0;
  }

  int get_fd() const override { return fd_; }

 protected:
  int fd_;
  bool closed_ = false;
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

}  // namespace socket
}  // namespace esphome

#endif  // USE_SOCKET_IMPL_LWIP_SOCKETS
