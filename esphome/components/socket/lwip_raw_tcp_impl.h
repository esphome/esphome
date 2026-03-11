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

/// Non-virtual common base for LWIP raw TCP sockets.
/// Provides shared fields and methods for both connected and listening sockets.
/// No virtual methods — pure code sharing.
class LWIPRawCommon {
 public:
  LWIPRawCommon(sa_family_t family, struct tcp_pcb *pcb) : pcb_(pcb), family_(family) {}
  ~LWIPRawCommon();
  LWIPRawCommon(const LWIPRawCommon &) = delete;
  LWIPRawCommon &operator=(const LWIPRawCommon &) = delete;

  int bind(const struct sockaddr *name, socklen_t addrlen);
  int close();
  int shutdown(int how);

  int getpeername(struct sockaddr *name, socklen_t *addrlen);
  int getsockname(struct sockaddr *name, socklen_t *addrlen);

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

  void init();

  // Non-listening sockets return error
  std::unique_ptr<LWIPRawImpl> accept(struct sockaddr *, socklen_t *) {
    errno = EINVAL;
    return nullptr;
  }
  std::unique_ptr<LWIPRawImpl> accept_loop_monitored(struct sockaddr *addr, socklen_t *addrlen) {
    return this->accept(addr, addrlen);
  }
  // Regular sockets can't be converted to listening - this shouldn't happen
  // as listen() should only be called on sockets created for listening
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
    // return ::sendto(fd_, buf, len, flags, to, tolen);
    errno = ENOSYS;
    return -1;
  }
  // Intentionally unlocked — this is a polling check called every loop iteration.
  // A stale read at worst delays processing by one loop tick; the actual I/O in
  // read() holds the lwip lock and re-checks properly. See esphome#10681.
  bool ready() const { return this->rx_buf_ != nullptr || this->rx_closed_ || this->pcb_ == nullptr; }

  // No lock needed — only called during setup before callbacks are registered.
  // A stale pcb_ read is benign (returns ECONNRESET, which the caller handles).
  int setblocking(bool blocking) {
    if (this->pcb_ == nullptr) {
      errno = ECONNRESET;
      return -1;
    }
    if (blocking) {
      // blocking operation not supported
      errno = EINVAL;
      return -1;
    }
    return 0;
  }
  int loop() { return 0; }

  err_t recv_fn(struct pbuf *pb, err_t err);

  static void s_err_fn(void *arg, err_t err);
  static err_t s_recv_fn(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, err_t err);

 protected:
  ssize_t internal_write_(const void *buf, size_t len);
  int internal_output_();

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

  void init();

  // Intentionally unlocked — polling check, see LWIPRawImpl::ready() comment.
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

  static void s_err_fn(void *arg, err_t err);

 private:
  err_t accept_fn_(struct tcp_pcb *newpcb, err_t err);
  static err_t s_accept_fn(void *arg, struct tcp_pcb *newpcb, err_t err);

  // Accept queue - holds incoming connections briefly until the event loop calls accept()
  // This is NOT a connection pool - just a temporary queue between LWIP callbacks and the main loop
  // 3 slots is plenty since connections are pulled out quickly by the event loop
  //
  // Memory analysis: std::array<3> vs original std::queue implementation:
  // - std::queue uses std::deque internally which on 32-bit systems needs:
  //   24 bytes (deque object) + 32+ bytes (map array) + heap allocations
  //   Total: ~56+ bytes minimum, plus heap fragmentation
  // - std::array<3>: 12 bytes fixed (3 pointers × 4 bytes)
  // Saves ~44+ bytes RAM per listening socket + avoids ALL heap allocations
  // Used on ESP8266 and RP2040 (platforms using LWIP_TCP implementation)
  //
  // By using a separate listening socket class, regular connected sockets save
  // 16 bytes (12 bytes array + 1 byte count + 3 bytes padding) of memory overhead on 32-bit systems
  static constexpr size_t MAX_ACCEPTED_SOCKETS = 3;
  std::array<std::unique_ptr<LWIPRawImpl>, MAX_ACCEPTED_SOCKETS> accepted_sockets_;
  uint8_t accepted_socket_count_ = 0;  // Number of sockets currently in queue
};

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_TCP
