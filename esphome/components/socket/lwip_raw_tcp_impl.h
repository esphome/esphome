#pragma once
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <span>

#include "esphome/core/helpers.h"
#include "headers.h"
#include "lwip/ip.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/tcp.h"

namespace esphome::socket {

// Forward declaration
class LWIPRawImpl;

// set to 1 to enable verbose lwip logging
#if 0  // NOLINT(readability-avoid-unconditional-preprocessor-if)
#define LWIP_LOG(msg, ...) ESP_LOGVV("socket.lwip", "socket %p: " msg, this, ##__VA_ARGS__)
#else
#define LWIP_LOG(msg, ...)
#endif

/// Non-virtual common base for LWIP raw TCP sockets.
/// Provides shared fields and methods for both connected and listening sockets.
/// No virtual methods — pure code sharing.
class LWIPRawCommon {
 public:
  LWIPRawCommon(sa_family_t family, struct tcp_pcb *pcb) : pcb_(pcb), family_(family) {}
  ~LWIPRawCommon() {
    if (this->pcb_ != nullptr) {
      LWIP_LOG("tcp_abort(%p)", this->pcb_);
      tcp_abort(this->pcb_);
      this->pcb_ = nullptr;
    }
  }
  LWIPRawCommon(const LWIPRawCommon &) = delete;
  LWIPRawCommon &operator=(const LWIPRawCommon &) = delete;

  int bind(const struct sockaddr *name, socklen_t addrlen);
  int close();
  int shutdown(int how);

  int getpeername(struct sockaddr *addr, socklen_t *addrlen);
  int getsockname(struct sockaddr *addr, socklen_t *addrlen);

  /// Format peer address into a fixed-size buffer (no heap allocation)
  size_t getpeername_to(std::span<char, SOCKADDR_STR_LEN> buf);
  /// Format local address into a fixed-size buffer (no heap allocation)
  size_t getsockname_to(std::span<char, SOCKADDR_STR_LEN> buf);

  int getsockopt(int level, int optname, void *optval, socklen_t *optlen);
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen);

  int get_fd() const { return -1; }

 protected:
  int ip2sockaddr_(ip_addr_t *ip, uint16_t port, struct sockaddr *name, socklen_t *addrlen);

  // Member ordering optimized to minimize padding on 32-bit systems
  struct tcp_pcb *pcb_;
  // don't use lwip nodelay flag, it sometimes causes reconnect
  // instead use it for determining whether to call lwip_output
  bool nodelay_ = false;
  sa_family_t family_ = 0;
};

/// Connected socket implementation for LWIP raw TCP.
/// No virtual methods — callers always use the concrete type.
class LWIPRawImpl : public LWIPRawCommon {
 public:
  using LWIPRawCommon::LWIPRawCommon;
  ~LWIPRawImpl();

  void init() {
    LWIP_LOG("init(%p)", this->pcb_);
    tcp_arg(this->pcb_, this);
    tcp_recv(this->pcb_, LWIPRawImpl::s_recv_fn);
    tcp_err(this->pcb_, LWIPRawImpl::s_err_fn);
  }

  std::unique_ptr<LWIPRawImpl> accept(struct sockaddr *, socklen_t *) {
    errno = EINVAL;
    return nullptr;
  }
  std::unique_ptr<LWIPRawImpl> accept_loop_monitored(struct sockaddr *addr, socklen_t *addrlen) {
    return this->accept(addr, addrlen);
  }
  int listen(int) {
    errno = EOPNOTSUPP;
    return -1;
  }
  ssize_t read(void *buf, size_t len);
  ssize_t readv(const struct iovec *iov, int iovcnt);
  ssize_t recvfrom(void *, size_t, sockaddr *, socklen_t *) {
    errno = ENOTSUP;
    return -1;
  }
  ssize_t write(const void *buf, size_t len);
  ssize_t writev(const struct iovec *iov, int iovcnt);
  ssize_t sendto(const void *, size_t, int, const struct sockaddr *, socklen_t) {
    errno = ENOSYS;
    return -1;
  }
  bool ready() const { return this->rx_buf_ != nullptr || this->rx_closed_ || this->pcb_ == nullptr; }

  int setblocking(bool blocking) {
    if (this->pcb_ == nullptr) {
      errno = ECONNRESET;
      return -1;
    }
    if (blocking) {
      errno = EINVAL;
      return -1;
    }
    return 0;
  }
  int loop() { return 0; }

  void err_fn(err_t err) {
    LWIP_LOG("err(err=%d)", err);
    this->pcb_ = nullptr;
  }
  err_t recv_fn(struct pbuf *pb, err_t err);

  static void s_err_fn(void *arg, err_t err) {
    auto *arg_this = reinterpret_cast<LWIPRawImpl *>(arg);
    arg_this->err_fn(err);
  }

  static err_t s_recv_fn(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, err_t err) {
    auto *arg_this = reinterpret_cast<LWIPRawImpl *>(arg);
    return arg_this->recv_fn(pb, err);
  }

 protected:
  ssize_t internal_write(const void *buf, size_t len);
  int internal_output();

  pbuf *rx_buf_ = nullptr;
  size_t rx_buf_offset_ = 0;
  bool rx_closed_ = false;
};

/// Listening socket implementation for LWIP raw TCP.
/// Separate from LWIPRawImpl — no virtual dispatch needed.
class LWIPRawListenImpl : public LWIPRawCommon {
 public:
  using LWIPRawCommon::LWIPRawCommon;
  ~LWIPRawListenImpl();

  void init() {
    LWIP_LOG("init(%p)", this->pcb_);
    tcp_arg(this->pcb_, this);
    tcp_accept(this->pcb_, LWIPRawListenImpl::s_accept_fn);
    tcp_err(this->pcb_, LWIPRawListenImpl::s_err_fn);
  }

  bool ready() const { return this->accepted_socket_count_ > 0; }

  std::unique_ptr<LWIPRawImpl> accept(struct sockaddr *addr, socklen_t *addrlen);
  std::unique_ptr<LWIPRawImpl> accept_loop_monitored(struct sockaddr *addr, socklen_t *addrlen) {
    return this->accept(addr, addrlen);
  }
  int listen(int backlog);

  // Listening sockets don't do I/O
  ssize_t read(void *, size_t) {
    errno = ENOTSUP;
    return -1;
  }
  ssize_t write(const void *, size_t) {
    errno = ENOTSUP;
    return -1;
  }
  ssize_t readv(const struct iovec *, int) {
    errno = ENOTSUP;
    return -1;
  }
  ssize_t writev(const struct iovec *, int) {
    errno = ENOTSUP;
    return -1;
  }
  ssize_t recvfrom(void *, size_t, sockaddr *, socklen_t *) {
    errno = ENOTSUP;
    return -1;
  }
  ssize_t sendto(const void *, size_t, int, const struct sockaddr *, socklen_t) {
    errno = ENOTSUP;
    return -1;
  }
  int setblocking(bool) { return 0; }
  int loop() { return 0; }

  void err_fn(err_t err) {
    LWIP_LOG("err(err=%d)", err);
    this->pcb_ = nullptr;
  }

  static void s_err_fn(void *arg, err_t err) {
    auto *arg_this = reinterpret_cast<LWIPRawListenImpl *>(arg);
    arg_this->err_fn(err);
  }

 private:
  err_t accept_fn_(struct tcp_pcb *newpcb, err_t err);

  static err_t s_accept_fn(void *arg, struct tcp_pcb *newpcb, err_t err) {
    auto *arg_this = reinterpret_cast<LWIPRawListenImpl *>(arg);
    return arg_this->accept_fn_(newpcb, err);
  }

  static constexpr size_t MAX_ACCEPTED_SOCKETS = 3;
  std::array<std::unique_ptr<LWIPRawImpl>, MAX_ACCEPTED_SOCKETS> accepted_sockets_;
  uint8_t accepted_socket_count_ = 0;
};

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_TCP
