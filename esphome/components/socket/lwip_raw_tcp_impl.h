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
#include "lwip/udp.h"

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
  uint8_t recv_timeout_cs_ = 0;  // SO_RCVTIMEO in centiseconds (0 = no timeout, max 2.55s)
};

/// Connected socket implementation for LWIP raw TCP.
/// No virtual methods — callers always use the concrete type.
class LWIPRawImpl : public LWIPRawCommon {
 public:
  using LWIPRawCommon::LWIPRawCommon;
  ~LWIPRawImpl();

  void init(struct pbuf *initial_rx = nullptr, bool initial_rx_closed = false);

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
  // Check if the socket has buffered data ready to read.
  // See the ready() contract in socket.h — callers must drain or track remaining data.
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
    // Raw TCP doesn't use a blocking flag directly. Blocking behavior
    // is provided by SO_RCVTIMEO which makes read() wait via wakeable_delay().
    return 0;
  }
  int loop() { return 0; }

  err_t recv_fn(struct pbuf *pb, err_t err);

  static void s_err_fn(void *arg, err_t err);
  static err_t s_recv_fn(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, err_t err);

 protected:
  // True when the socket could receive data but none has arrived yet.
  // Safe to call without LWIP_LOCK — only null-checks pointers and reads a bool,
  // all atomic on ARM/Xtensa. A stale value is harmless: the caller either does
  // an unnecessary wait (stale true) or skips it (stale false), and the
  // authoritative recheck happens under LWIP_LOCK afterward.
  bool waiting_for_data_() const { return this->rx_buf_ == nullptr && !this->rx_closed_ && this->pcb_ != nullptr; }
  void wait_for_data_();
  ssize_t read_locked_(void *buf, size_t len);
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

  // Temporary callbacks for queued PCBs (between accept_fn_ and accept())
  static void s_queued_err_fn(void *arg, err_t err);
  static err_t s_queued_recv_fn(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, err_t err);

  // Accept queue entry — stores a raw tcp_pcb and any data received while queued.
  // lwip's default tcp_recv_null handler drops data and ACKs it, so we must register
  // a temporary recv callback to buffer any data that arrives between accept_fn_
  // (which stores the PCB) and accept() (which creates the LWIPRawImpl).
  struct QueuedPcb {
    struct tcp_pcb *pcb{nullptr};
    struct pbuf *rx_buf{nullptr};  // Data received while queued (before accept() picks it up)
    bool rx_closed{false};         // Remote sent FIN while queued
  };

  // Accept queue — stores raw tcp_pcb entries instead of heap-allocated LWIPRawImpl objects.
  // LWIPRawImpl creation is deferred to the main-loop accept() call. This avoids:
  // - Heap allocation in the accept callback (unsafe from IRQ context on RP2040)
  // - Dangling LWIPRawImpl if the connection errors before accept() picks it up
  // 2 slots is plenty since the main loop drains the queue every iteration.
  static constexpr size_t MAX_ACCEPTED_SOCKETS = 2;
  std::array<QueuedPcb, MAX_ACCEPTED_SOCKETS> accepted_pcbs_{};
  uint8_t accepted_socket_count_ = 0;  // Number of entries currently in queue
};

/// Send-only UDP socket implementation for LWIP raw API.
/// Non-virtual, concrete type. Uses lwip/udp.h raw API.
/// No receive capability — use LWIPRawUDPRecvImpl for sockets that need to receive.
class LWIPRawUDPImpl {
 public:
  LWIPRawUDPImpl(sa_family_t family);
  ~LWIPRawUDPImpl();
  LWIPRawUDPImpl(const LWIPRawUDPImpl &) = delete;
  LWIPRawUDPImpl &operator=(const LWIPRawUDPImpl &) = delete;

  int bind(const struct sockaddr *name, socklen_t addrlen);
  int close();

  /// Send a UDP packet to the specified destination.
  ssize_t sendto(const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

  int setsockopt(int level, int optname, const void *optval, socklen_t optlen);
  int getsockopt(int level, int optname, void *optval, socklen_t *optlen);

  int setblocking(bool blocking);

  bool is_valid() const { return this->pcb_ != nullptr; }
  bool ready() const { return false; }
  int get_fd() const { return -1; }

 protected:
  /// Convert lwip ip_addr_t and port to sockaddr.
  int ip2sockaddr_(const ip_addr_t *ip, uint16_t port, struct sockaddr *name, socklen_t *addrlen);

  /// Shared bind logic — parses sockaddr and calls udp_bind. Caller must hold LWIP_LOCK.
  int bind_internal_locked_(const struct sockaddr *name, socklen_t addrlen);

  /// Shared close logic — unregisters and removes udp pcb. Caller must hold LWIP_LOCK.
  int close_internal_locked_();

  struct udp_pcb *pcb_{nullptr};
  sa_family_t family_{0};
};

/// UDP socket with receive support for LWIP raw API.
/// Extends LWIPRawUDPImpl with a fixed-size ring buffer for incoming packets.
/// The recv callback is registered on bind().
///
/// Note: close() and bind() intentionally hide the base class methods to add
/// recv callback registration/cleanup. This is safe because these classes are
/// never used polymorphically (no virtual dispatch) — callers always use the
/// concrete LWIPRawUDPRecvImpl type via the UDPRecvSocket alias.
class LWIPRawUDPRecvImpl : public LWIPRawUDPImpl {
 public:
  using LWIPRawUDPImpl::LWIPRawUDPImpl;
  ~LWIPRawUDPRecvImpl();

  /// Close the socket, flushing any queued rx packets first.
  int close();

  /// Bind and register the recv callback for incoming packets.
  int bind(const struct sockaddr *name, socklen_t addrlen);

  /// Read the next queued packet, discarding source address info.
  /// If buf is smaller than the packet, data is silently truncated (returns bytes copied).
  /// Note: unlike POSIX MSG_TRUNC, this does not return the original packet length on truncation.
  ssize_t read(void *buf, size_t len);
  /// Read the next queued packet and return the source address.
  /// If buf is smaller than the packet, data is silently truncated (returns bytes copied).
  /// Note: unlike POSIX MSG_TRUNC, this does not return the original packet length on truncation.
  ssize_t recvfrom(void *buf, size_t len, struct sockaddr *src_addr, socklen_t *addrlen);

  /// Returns true if there are packets available to read.
  /// Intentionally unlocked — same rationale as LWIPRawImpl::ready().
  bool ready() const { return this->rx_count_ > 0; }

 protected:
  static void s_recv_fn(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
  void recv_fn_(struct pbuf *p, const ip_addr_t *addr, u16_t port);

  /// Ring buffer for received UDP packets.
  /// Both producer (recv callback) and consumer (main loop) are serialized by the
  /// lwip lock — the callback runs under lwip core lock, and consumer methods hold
  /// LWIP_LOCK(). All 4 slots are usable (no wasted slot for full/empty distinction).
  /// No heap allocation in the recv callback — packets are dropped if the queue is full.
  static constexpr uint8_t UDP_RX_QUEUE_SIZE = 4;
  static constexpr uint8_t UDP_RX_MASK = UDP_RX_QUEUE_SIZE - 1;
  static_assert((UDP_RX_QUEUE_SIZE & UDP_RX_MASK) == 0, "UDP_RX_QUEUE_SIZE must be power of 2");
  struct UDPRxPacket {
    struct pbuf *pb{nullptr};
    ip_addr_t src_addr{};
    uint16_t src_port{0};
  };
  std::array<UDPRxPacket, UDP_RX_QUEUE_SIZE> rx_queue_{};
  uint8_t rx_read_idx_{0};
  uint8_t rx_count_{0};
};

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_TCP
