#pragma once
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_BSD_SOCKETS

#include <memory>
#include <span>

#include "esphome/core/helpers.h"
#include "headers.h"

#ifdef USE_ESP32
#include <lwip/sockets.h>
#endif

#ifdef USE_LWIP_FAST_SELECT
#include "esphome/core/lwip_fast_select.h"
#endif

namespace esphome::socket {

class BSDSocketImpl {
 public:
  BSDSocketImpl(int fd, bool monitor_loop = false);
  ~BSDSocketImpl();
  BSDSocketImpl(const BSDSocketImpl &) = delete;
  BSDSocketImpl &operator=(const BSDSocketImpl &) = delete;

  int connect(const struct sockaddr *addr, socklen_t addrlen) { return ::connect(this->fd_, addr, addrlen); }
  std::unique_ptr<BSDSocketImpl> accept(struct sockaddr *addr, socklen_t *addrlen) {
    int fd = ::accept(this->fd_, addr, addrlen);
    if (fd == -1)
      return {};
    return make_unique<BSDSocketImpl>(fd, false);
  }
  std::unique_ptr<BSDSocketImpl> accept_loop_monitored(struct sockaddr *addr, socklen_t *addrlen) {
    int fd = ::accept(this->fd_, addr, addrlen);
    if (fd == -1)
      return {};
    return make_unique<BSDSocketImpl>(fd, true);
  }

  int bind(const struct sockaddr *addr, socklen_t addrlen) { return ::bind(this->fd_, addr, addrlen); }
  int close();
  int shutdown(int how) { return ::shutdown(this->fd_, how); }

  int getpeername(struct sockaddr *addr, socklen_t *addrlen) { return ::getpeername(this->fd_, addr, addrlen); }
  int getsockname(struct sockaddr *addr, socklen_t *addrlen) { return ::getsockname(this->fd_, addr, addrlen); }

  /// Format peer address into a fixed-size buffer (no heap allocation)
  size_t getpeername_to(std::span<char, SOCKADDR_STR_LEN> buf);
  /// Format local address into a fixed-size buffer (no heap allocation)
  size_t getsockname_to(std::span<char, SOCKADDR_STR_LEN> buf);

  int getsockopt(int level, int optname, void *optval, socklen_t *optlen) {
    return ::getsockopt(this->fd_, level, optname, optval, optlen);
  }
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen) {
#if defined(USE_LWIP_FAST_SELECT) && defined(CONFIG_LWIP_TCPIP_CORE_LOCKING)
    // Fast path for TCP_NODELAY: directly set the pcb flag under the TCPIP core lock,
    // bypassing lwip_setsockopt overhead (socket lookups, hook, switch cascade, refcounting).
    if (level == IPPROTO_TCP && optname == TCP_NODELAY && optlen == sizeof(int) && optval != nullptr) {
      LwIPLock lock;
      if (esphome_lwip_set_nodelay(this->cached_sock_, *reinterpret_cast<const int *>(optval) != 0))
        return 0;
    }
#endif
    return ::setsockopt(this->fd_, level, optname, optval, optlen);
  }
  int listen(int backlog) { return ::listen(this->fd_, backlog); }
  ssize_t read(void *buf, size_t len) {
#ifdef USE_ESP32
    return ::lwip_read(this->fd_, buf, len);
#else
    return ::read(this->fd_, buf, len);
#endif
  }
  ssize_t recvfrom(void *buf, size_t len, sockaddr *addr, socklen_t *addr_len) {
#if defined(USE_ESP32) || defined(USE_HOST)
    return ::recvfrom(this->fd_, buf, len, 0, addr, addr_len);
#else
    return ::lwip_recvfrom(this->fd_, buf, len, 0, addr, addr_len);
#endif
  }
  ssize_t readv(const struct iovec *iov, int iovcnt) {
#if defined(USE_ESP32)
    return ::lwip_readv(this->fd_, iov, iovcnt);
#else
    return ::readv(this->fd_, iov, iovcnt);
#endif
  }
  ssize_t write(const void *buf, size_t len) {
#ifdef USE_ESP32
    return ::lwip_write(this->fd_, buf, len);
#else
    return ::write(this->fd_, buf, len);
#endif
  }
  ssize_t send(const void *buf, size_t len, int flags) { return ::send(this->fd_, buf, len, flags); }
  ssize_t writev(const struct iovec *iov, int iovcnt) {
#if defined(USE_ESP32)
    return ::lwip_writev(this->fd_, iov, iovcnt);
#else
    return ::writev(this->fd_, iov, iovcnt);
#endif
  }

  ssize_t sendto(const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return ::sendto(this->fd_, buf, len, flags, to, tolen);  // NOLINT(readability-suspicious-call-argument)
  }

  int setblocking(bool blocking);
  int loop() { return 0; }

  bool ready() const;

  int get_fd() const { return this->fd_; }

 protected:
  int fd_{-1};
#ifdef USE_LWIP_FAST_SELECT
  struct lwip_sock *cached_sock_{nullptr};  // Cached for direct rcvevent read in ready()
#endif
  bool closed_{false};
  bool loop_monitored_{false};
};

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_BSD_SOCKETS
