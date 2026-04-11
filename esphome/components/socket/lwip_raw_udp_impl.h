#pragma once
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>

#include "headers.h"
#include "lwip/ip.h"
#include "lwip/udp.h"

namespace esphome::socket {

/// Send-only UDP socket implementation for LWIP raw API.
/// Non-virtual, concrete type. Uses lwip/udp.h raw API.
/// No receive capability — use LWIPRawUDPImpl for sockets that need to receive.
class LWIPRawUDPSendImpl {
 public:
  LWIPRawUDPSendImpl(sa_family_t family);
  ~LWIPRawUDPSendImpl();
  LWIPRawUDPSendImpl(const LWIPRawUDPSendImpl &) = delete;
  LWIPRawUDPSendImpl &operator=(const LWIPRawUDPSendImpl &) = delete;

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
/// Extends LWIPRawUDPSendImpl with a fixed-size ring buffer for incoming packets.
/// The recv callback is registered on bind().
///
/// Note: close() and bind() intentionally hide the base class methods to add
/// recv callback registration/cleanup. This is safe because these classes are
/// never used polymorphically (no virtual dispatch) — callers always use the
/// concrete LWIPRawUDPImpl type via the UDPSocket alias.
class LWIPRawUDPImpl : public LWIPRawUDPSendImpl {
 public:
  using LWIPRawUDPSendImpl::LWIPRawUDPSendImpl;
  ~LWIPRawUDPImpl();

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
