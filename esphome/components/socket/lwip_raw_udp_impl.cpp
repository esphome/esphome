#include "socket.h"
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include <cerrno>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/wake.h"
#include "lwip_raw_common_impl.h"

#include "lwip/igmp.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

namespace esphome::socket {

// LWIP thread safety — see lwip_raw_common_impl.h for full explanation.
// esphome::LwIPLock is the platform-provided RAII guard.
// On RP2040, it acquires cyw43_arch_lwip_begin/end. On ESP8266, it's a no-op.
#define LWIP_LOCK() esphome::LwIPLock lwip_lock_guard  // NOLINT

static const char *const TAG = "socket.lwip_udp";

// ---- LWIPRawUDPImpl (send-only) methods ----

LWIPRawUDPImpl::LWIPRawUDPImpl(sa_family_t family) : family_(family) {
  LWIP_LOCK();
#if LWIP_IPV6
  this->pcb_ = udp_new_ip_type(family == AF_INET6 ? IPADDR_TYPE_ANY : IPADDR_TYPE_V4);
#else
  this->pcb_ = udp_new();
#endif
}

LWIPRawUDPImpl::~LWIPRawUDPImpl() {
  // Early return avoids acquiring the lwip lock when pcb_ is already null
  // (e.g., after LWIPRawUDPRecvImpl::close() already cleaned up).
  if (this->pcb_ == nullptr)
    return;
  LWIP_LOCK();
  udp_remove(this->pcb_);
  this->pcb_ = nullptr;
}

int LWIPRawUDPImpl::bind_internal_locked_(const struct sockaddr *name, socklen_t addrlen) {
  // Caller must hold LWIP_LOCK
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return -1;
  }
  if (name == nullptr) {
    errno = EINVAL;
    return -1;
  }
  ip_addr_t ip;
  uint16_t port;
  if (!sockaddr_to_lwip(name, addrlen, &ip, &port)) {
    errno = EINVAL;
    return -1;
  }
#if LWIP_IPV6
  // For bind, use IPADDR_TYPE_ANY on IPv6 sockets to accept both IPv4 and IPv6
  // packets (dual-stack). sockaddr_to_lwip uses IPADDR_TYPE_V6 which is correct
  // for sendto destinations but too restrictive for bind.
  if (this->family_ == AF_INET6) {
    ip.type = IPADDR_TYPE_ANY;
  }
#endif
  return lwip_bind_err(udp_bind(this->pcb_, &ip, port));
}

int LWIPRawUDPImpl::bind(const struct sockaddr *name, socklen_t addrlen) {
  LWIP_LOCK();
  return this->bind_internal_locked_(name, addrlen);
}

int LWIPRawUDPImpl::close() {
  LWIP_LOCK();
  return this->close_internal_locked_();
}

int LWIPRawUDPImpl::close_internal_locked_() {
  // Caller must hold LWIP_LOCK
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return -1;
  }
  udp_remove(this->pcb_);
  this->pcb_ = nullptr;
  return 0;
}

int LWIPRawUDPImpl::ip2sockaddr_(const ip_addr_t *ip, uint16_t port, struct sockaddr *name, socklen_t *addrlen) {
  // UDP recv callback provides port in host byte order
  return lwip_ip_to_sockaddr(this->family_, ip, port, name, addrlen);
}

ssize_t LWIPRawUDPImpl::sendto(const void *buf, size_t len, int flags, const struct sockaddr *dest_addr,
                               socklen_t addrlen) {
  (void) flags;  // Flags (MSG_DONTWAIT, etc.) are ignored; raw lwip is always non-blocking
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return -1;
  }
  if (buf == nullptr || dest_addr == nullptr) {
    errno = EINVAL;
    return -1;
  }

  // pbuf_alloc takes u16_t length; reject oversized packets
  if (len > UINT16_MAX) {
    errno = EMSGSIZE;
    return -1;
  }

  ip_addr_t dst_ip;
  uint16_t dst_port;
  if (!sockaddr_to_lwip(dest_addr, addrlen, &dst_ip, &dst_port)) {
    errno = EINVAL;
    return -1;
  }

  // Allocate pbuf and copy data
  struct pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, (uint16_t) len, PBUF_RAM);
  if (pb == nullptr) {
    errno = ENOMEM;
    return -1;
  }
  memcpy(pb->payload, buf, len);

  err_t err = udp_sendto(this->pcb_, pb, &dst_ip, dst_port);
  pbuf_free(pb);

  if (err != ERR_OK) {
    errno = err == ERR_MEM ? ENOMEM : EIO;
    return -1;
  }
  return (ssize_t) len;
}

int LWIPRawUDPImpl::setsockopt(int level, int optname, const void *optval, socklen_t optlen) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return -1;
  }
  if (level == SOL_SOCKET && optname == SO_REUSEADDR) {
    // lwip raw UDP doesn't enforce port exclusivity the same way,
    // but we accept this silently for compatibility
    return 0;
  }
  if (level == SOL_SOCKET && optname == SO_BROADCAST) {
    if (optval == nullptr || optlen < sizeof(int)) {
      errno = EINVAL;
      return -1;
    }
    int val = *reinterpret_cast<const int *>(optval);
    if (val) {
      ip_set_option(this->pcb_, SOF_BROADCAST);
    } else {
      ip_reset_option(this->pcb_, SOF_BROADCAST);
    }
    return 0;
  }
  if (level == IPPROTO_IP && optname == IP_ADD_MEMBERSHIP) {
    if (optval == nullptr || optlen < sizeof(struct ip_mreq)) {
      errno = EINVAL;
      return -1;
    }
    auto *mreq = reinterpret_cast<const struct ip_mreq *>(optval);
    ip4_addr_t multiaddr;
    multiaddr.addr = mreq->imr_multiaddr.s_addr;
    ip4_addr_t ifaddr;
    ifaddr.addr = mreq->imr_interface.s_addr;
    err_t err = igmp_joingroup(&ifaddr, &multiaddr);
    if (err != ERR_OK) {
      errno = EIO;
      return -1;
    }
    return 0;
  }
  if (level == IPPROTO_IP && optname == IP_DROP_MEMBERSHIP) {
    if (optval == nullptr || optlen < sizeof(struct ip_mreq)) {
      errno = EINVAL;
      return -1;
    }
    auto *mreq = reinterpret_cast<const struct ip_mreq *>(optval);
    ip4_addr_t multiaddr;
    multiaddr.addr = mreq->imr_multiaddr.s_addr;
    ip4_addr_t ifaddr;
    ifaddr.addr = mreq->imr_interface.s_addr;
    err_t err = igmp_leavegroup(&ifaddr, &multiaddr);
    if (err != ERR_OK) {
      errno = EIO;
      return -1;
    }
    return 0;
  }
  errno = ENOPROTOOPT;
  return -1;
}

int LWIPRawUDPImpl::getsockopt(int level, int optname, void *optval, socklen_t *optlen) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return -1;
  }
  if (level == SOL_SOCKET && optname == SO_REUSEADDR) {
    if (optval == nullptr || optlen == nullptr || *optlen < sizeof(int)) {
      errno = EINVAL;
      return -1;
    }
    *reinterpret_cast<int *>(optval) = 1;
    *optlen = sizeof(int);
    return 0;
  }
  errno = ENOPROTOOPT;
  return -1;
}

int LWIPRawUDPImpl::setblocking(bool blocking) {
  if (blocking) {
    // blocking operation not supported on raw lwip
    errno = EINVAL;
    return -1;
  }
  return 0;
}

// ---- LWIPRawUDPRecvImpl methods ----

LWIPRawUDPRecvImpl::~LWIPRawUDPRecvImpl() {
  // Flush rx queue and unregister callback before base destructor removes pcb
  if (this->pcb_ != nullptr)
    this->close();
}

int LWIPRawUDPRecvImpl::close() {
  LWIP_LOCK();
  // Unregister recv callback before removing pcb
  if (this->pcb_ != nullptr) {
    udp_recv(this->pcb_, nullptr, nullptr);
  }
  // Flush any queued rx packets
  while (this->rx_count_ > 0) {
    auto &pkt = this->rx_queue_[this->rx_read_idx_];
    if (pkt.pb != nullptr) {
      pbuf_free(pkt.pb);
      pkt.pb = nullptr;
    }
    this->rx_read_idx_ = (this->rx_read_idx_ + 1) & UDP_RX_MASK;
    this->rx_count_--;
  }
  // close_internal_locked_() returns EBADF if already closed, which is fine from destructor
  return this->close_internal_locked_();
}

int LWIPRawUDPRecvImpl::bind(const struct sockaddr *name, socklen_t addrlen) {
  LWIP_LOCK();
  int ret = this->bind_internal_locked_(name, addrlen);
  if (ret != 0)
    return ret;
  // Register recv callback now that we're bound and ready to receive
  udp_recv(this->pcb_, LWIPRawUDPRecvImpl::s_recv_fn, this);
  return 0;
}

ssize_t LWIPRawUDPRecvImpl::read(void *buf, size_t len) { return this->recvfrom(buf, len, nullptr, nullptr); }

ssize_t LWIPRawUDPRecvImpl::recvfrom(void *buf, size_t len, struct sockaddr *src_addr, socklen_t *addrlen) {
  if (buf == nullptr && len > 0) {
    errno = EINVAL;
    return -1;
  }
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return -1;
  }
  if (this->rx_count_ == 0) {
    errno = EWOULDBLOCK;
    return -1;
  }

  auto &pkt = this->rx_queue_[this->rx_read_idx_];
  size_t pkt_len = pkt.pb->tot_len;
  size_t copy_len = std::min(len, pkt_len);

  // Copy data from pbuf chain
  pbuf_copy_partial(pkt.pb, buf, copy_len, 0);

  // Fill in source address if requested.
  // If ip2sockaddr_ fails (e.g., addrlen too small), fail the entire recvfrom
  // rather than silently returning data without a source address.
  if (src_addr != nullptr && addrlen != nullptr &&
      this->ip2sockaddr_(&pkt.src_addr, pkt.src_port, src_addr, addrlen) != 0) {
    // Don't consume the packet on address conversion failure
    return -1;
  }

  // Free the pbuf and advance the read pointer
  pbuf_free(pkt.pb);
  pkt.pb = nullptr;
  this->rx_read_idx_ = (this->rx_read_idx_ + 1) & UDP_RX_MASK;
  this->rx_count_--;

  return (ssize_t) copy_len;
}

void LWIPRawUDPRecvImpl::s_recv_fn(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  auto *self = reinterpret_cast<LWIPRawUDPRecvImpl *>(arg);
  self->recv_fn_(p, addr, port);
}

// LWIP CALLBACK — runs from IRQ context on RP2040 (low-priority user IRQ).
// No heap allocation allowed — malloc is not IRQ-safe (see #14687).
// No LWIP_LOCK() needed — lwip core already holds the async_context lock.
void LWIPRawUDPRecvImpl::recv_fn_(struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  if (p == nullptr)
    return;

  // Check if queue is full
  if (this->rx_count_ >= UDP_RX_QUEUE_SIZE) {
    // Drop packet — queue full
    pbuf_free(p);
    return;
  }

  // Enqueue the packet
  uint8_t write_idx = (this->rx_read_idx_ + this->rx_count_) & UDP_RX_MASK;
  auto &slot = this->rx_queue_[write_idx];
  slot.pb = p;
  slot.src_addr = *addr;
  slot.src_port = port;
  this->rx_count_++;

#if defined(USE_ESP8266) || defined(USE_RP2040)
  esphome::wake_loop_any_context();
#endif
}

// ---- UDP Factory functions ----

std::unique_ptr<UDPSocket> socket_udp(int domain, int protocol) {
  (void) protocol;  // Raw lwip UDP ignores protocol; kept for API compatibility
  auto sock = make_unique<LWIPRawUDPImpl>((sa_family_t) domain);
  if (!sock->is_valid()) {
    errno = ENOMEM;
    return nullptr;
  }
  return sock;
}

std::unique_ptr<UDPRecvSocket> socket_udp_recv(int domain, int protocol) {
  (void) protocol;  // Raw lwip UDP ignores protocol; kept for API compatibility
  auto sock = make_unique<LWIPRawUDPRecvImpl>((sa_family_t) domain);
  if (!sock->is_valid()) {
    errno = ENOMEM;
    return nullptr;
  }
  return sock;
}

std::unique_ptr<UDPRecvSocket> socket_udp_recv_loop_monitored(int domain, int protocol) {
  // LWIPRawUDPRecvImpl has wake built into the recv callback, so no extra monitoring needed
  return socket_udp_recv(domain, protocol);
}

#undef LWIP_LOCK

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_TCP
