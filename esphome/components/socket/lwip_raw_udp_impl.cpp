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

// ---- LWIPRawUDPSendImpl (send-only) methods ----

LWIPRawUDPSendImpl::~LWIPRawUDPSendImpl() {
  // Guard avoids acquiring the lwip lock when already closed
  if (this->pcb_ != nullptr)
    this->close();
}

int LWIPRawUDPSendImpl::bind(const struct sockaddr *name, socklen_t addrlen) {
  LWIP_LOCK();
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
  if (!sockaddr_to_lwip_bind(this->family_, name, addrlen, &ip, &port)) {
    errno = EINVAL;
    return -1;
  }
  return lwip_bind_err(udp_bind(this->pcb_, &ip, port));
}

int LWIPRawUDPSendImpl::close() {
  LWIP_LOCK();
  return this->close_internal_locked_();
}

int LWIPRawUDPSendImpl::close_internal_locked_() {
  // Caller must hold LWIP_LOCK
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return -1;
  }
  udp_remove(this->pcb_);
  this->pcb_ = nullptr;
  return 0;
}

int LWIPRawUDPSendImpl::ip2sockaddr_(const ip_addr_t *ip, uint16_t port, struct sockaddr *name, socklen_t *addrlen) {
  // UDP recv callback provides port in host byte order
  return lwip_ip_to_sockaddr(this->family_, ip, port, name, addrlen);
}

ssize_t LWIPRawUDPSendImpl::sendto(const void *buf, size_t len, int flags, const struct sockaddr *dest_addr,
                                   socklen_t addrlen) {
  (void) flags;  // Flags (MSG_DONTWAIT, etc.) are ignored; raw lwip is always non-blocking
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

  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = EBADF;
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

int LWIPRawUDPSendImpl::setsockopt(int level, int optname, const void *optval, socklen_t optlen) {
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
  if (level == IPPROTO_IP && (optname == IP_ADD_MEMBERSHIP || optname == IP_DROP_MEMBERSHIP)) {
    if (optval == nullptr || optlen < sizeof(struct ip_mreq)) {
      errno = EINVAL;
      return -1;
    }
    auto *mreq = reinterpret_cast<const struct ip_mreq *>(optval);
    ip4_addr_t multiaddr{mreq->imr_multiaddr.s_addr};
    ip4_addr_t ifaddr{mreq->imr_interface.s_addr};
    err_t err =
        optname == IP_ADD_MEMBERSHIP ? igmp_joingroup(&ifaddr, &multiaddr) : igmp_leavegroup(&ifaddr, &multiaddr);
    if (err != ERR_OK) {
      errno = EIO;
      return -1;
    }
    return 0;
  }
  errno = ENOPROTOOPT;
  return -1;
}

int LWIPRawUDPSendImpl::getsockopt(int level, int optname, void *optval, socklen_t *optlen) {
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

int LWIPRawUDPSendImpl::setblocking(bool blocking) {
  if (blocking) {
    // blocking operation not supported on raw lwip
    errno = EINVAL;
    return -1;
  }
  return 0;
}

// ---- LWIPRawUDPImpl methods ----

LWIPRawUDPImpl::LWIPRawUDPImpl(sa_family_t family, struct udp_pcb *pcb) : LWIPRawUDPSendImpl(family, pcb) {
  // Registered here (not in bind) so unbound client sockets can receive replies
  udp_recv(this->pcb_, LWIPRawUDPImpl::s_recv_fn, this);
}

LWIPRawUDPImpl::~LWIPRawUDPImpl() {
  // Flush rx queue and unregister callback before base destructor removes pcb
  if (this->pcb_ != nullptr)
    this->close();
}

int LWIPRawUDPImpl::close() {
  LWIP_LOCK();
  // Unregister recv callback before removing pcb
  if (this->pcb_ != nullptr) {
    udp_recv(this->pcb_, nullptr, nullptr);
  }
  // Flush queued rx packets; slots within rx_count_ always hold a live pbuf
  for (; this->rx_count_ > 0; this->rx_count_--) {
    pbuf_free(this->rx_queue_[this->rx_read_idx_].pb);
    this->rx_read_idx_ = (this->rx_read_idx_ + 1) & UDP_RX_MASK;
  }
  // close_internal_locked_() returns EBADF if already closed, which is fine from destructor
  return this->close_internal_locked_();
}

ssize_t LWIPRawUDPImpl::read(void *buf, size_t len) { return this->recvfrom(buf, len, nullptr, nullptr); }

ssize_t LWIPRawUDPImpl::recvfrom(void *buf, size_t len, struct sockaddr *src_addr, socklen_t *addrlen) {
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
  // On address conversion failure, still consume the packet — the failure is
  // deterministic (family_ and *addrlen), so keeping it would wedge the queue
  ssize_t ret = -1;
  if (src_addr == nullptr || addrlen == nullptr ||
      this->ip2sockaddr_(&pkt.src_addr, pkt.src_port, src_addr, addrlen) == 0) {
    ret = (ssize_t) std::min(len, (size_t) pkt.pb->tot_len);
    pbuf_copy_partial(pkt.pb, buf, ret, 0);
  }
  pbuf_free(pkt.pb);
  this->rx_read_idx_ = (this->rx_read_idx_ + 1) & UDP_RX_MASK;
  this->rx_count_--;
  return ret;
}

void LWIPRawUDPImpl::s_recv_fn(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  auto *self = reinterpret_cast<LWIPRawUDPImpl *>(arg);
  self->recv_fn_(p, addr, port);
}

// LWIP CALLBACK — runs from IRQ context on RP2040 (low-priority user IRQ).
// No heap allocation allowed — malloc is not IRQ-safe (see #14687).
// No LWIP_LOCK() needed — lwip core already holds the async_context lock.
void LWIPRawUDPImpl::recv_fn_(struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  if (p == nullptr)
    return;

  // Check if queue is full
  if (this->rx_count_ >= UDP_RX_QUEUE_SIZE) {
    // Drop packet — queue full. Can't log from IRQ context, so count it
    // (saturating) for consumers to surface via get_rx_dropped().
    if (this->rx_dropped_ != UINT16_MAX)
      this->rx_dropped_++;
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

  esphome::wake_loop_any_context();
}

// ---- UDP Factory functions ----

static struct udp_pcb *new_udp_pcb(int domain) {
#if LWIP_IPV6
  return udp_new_ip_type(domain == AF_INET6 ? IPADDR_TYPE_ANY : IPADDR_TYPE_V4);
#else
  return udp_new();
#endif
}

std::unique_ptr<UDPSendSocket> socket_udp_send(int domain, int protocol) {
  (void) protocol;  // Raw lwip UDP ignores protocol; kept for API compatibility
  LWIP_LOCK();
  auto *pcb = new_udp_pcb(domain);
  if (pcb == nullptr) {
    errno = ENOMEM;
    return nullptr;
  }
  return make_unique<LWIPRawUDPSendImpl>((sa_family_t) domain, pcb);
}

std::unique_ptr<UDPSocket> socket_udp(int domain, int protocol) {
  (void) protocol;  // Raw lwip UDP ignores protocol; kept for API compatibility
  LWIP_LOCK();
  auto *pcb = new_udp_pcb(domain);
  if (pcb == nullptr) {
    errno = ENOMEM;
    return nullptr;
  }
  // Ctor registers the recv callback under the lock held here
  return make_unique<LWIPRawUDPImpl>((sa_family_t) domain, pcb);
}

#undef LWIP_LOCK

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_TCP
