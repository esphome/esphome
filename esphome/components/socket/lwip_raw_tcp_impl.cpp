#include "socket.h"
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include <cerrno>
#include <cstring>
#include <sys/time.h>

#include "esphome/core/helpers.h"
#include "esphome/core/wake.h"
#include "esphome/core/log.h"

#include "lwip/igmp.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#ifdef USE_ESP8266
#include <coredecls.h>  // For esp_schedule()
#elif defined(USE_RP2040)
#include <hardware/sync.h>  // For __sev(), __wfe()
#include <pico/time.h>      // For add_alarm_in_ms(), cancel_alarm()
#endif

namespace esphome::socket {

// ---- LWIP thread safety ----
//
// On RP2040 (Pico W), arduino-pico sets PICO_CYW43_ARCH_THREADSAFE_BACKGROUND=1.
// This means lwip callbacks (recv_fn, accept_fn, err_fn) run from a low-priority
// user IRQ context, not the main loop (see low_priority_irq_handler() in pico-sdk
// async_context_threadsafe_background.c). They can preempt main-loop code at any point.
//
// Without locking, this causes race conditions between recv_fn and read() on the
// shared rx_buf_ pbuf chain — recv_fn calls pbuf_cat() while read() is freeing
// nodes, leading to use-after-free and infinite-loop crashes. See esphome#10681.
//
// On ESP8266, lwip callbacks run from the SYS context which cooperates with user
// code (CONT context) — they never preempt each other, so no locking is needed.
//
// esphome::LwIPLock is the platform-provided RAII guard (see helpers.h/helpers.cpp).
// On RP2040, it acquires cyw43_arch_lwip_begin/end (WiFi) or ethernet_arch_lwip_begin/end
// (Ethernet). On ESP8266, it's a no-op.
#define LWIP_LOCK() esphome::LwIPLock lwip_lock_guard  // NOLINT

static const char *const TAG = "socket.lwip";

// set to 1 to enable verbose lwip logging
#if 0  // NOLINT(readability-avoid-unconditional-preprocessor-if)
#define LWIP_LOG(msg, ...) ESP_LOGVV(TAG, "socket %p: " msg, this, ##__VA_ARGS__)
#else
#define LWIP_LOG(msg, ...)
#endif

// ---- Shared helpers ----

/// Convert lwip ip_addr_t + host-order port to sockaddr, based on the socket's address family.
/// @param port_host Port in host byte order. TCP callers must convert from network order first
///                  (tcp_pcb stores ports in network byte order); UDP callers can pass directly
///                  (lwip udp_recv callback provides port in host byte order).
/// Shared by both TCP (LWIPRawCommon) and UDP (LWIPRawUDPImpl) implementations.
static int lwip_ip_to_sockaddr(sa_family_t family, const ip_addr_t *ip, uint16_t port_host, struct sockaddr *name,
                               socklen_t *addrlen) {
  if (family == AF_INET) {
    if (*addrlen < sizeof(struct sockaddr_in)) {
      errno = EINVAL;
      return -1;
    }
    auto *addr = reinterpret_cast<struct sockaddr_in *>(name);
    addr->sin_family = AF_INET;
    *addrlen = addr->sin_len = sizeof(struct sockaddr_in);
    addr->sin_port = htons(port_host);
    inet_addr_from_ip4addr(&addr->sin_addr, ip_2_ip4(ip));
    return 0;
  }
#if LWIP_IPV6
  if (family == AF_INET6) {
    if (*addrlen < sizeof(struct sockaddr_in6)) {
      errno = EINVAL;
      return -1;
    }
    auto *addr = reinterpret_cast<struct sockaddr_in6 *>(name);
    addr->sin6_family = AF_INET6;
    *addrlen = addr->sin6_len = sizeof(struct sockaddr_in6);
    addr->sin6_port = htons(port_host);
    // AF_INET6 sockets may receive IPv4 packets; convert to IPv4-mapped IPv6.
    if (IP_IS_V4(ip)) {
      ip_addr_t mapped;
      ip4_2_ipv4_mapped_ipv6(ip_2_ip6(&mapped), ip_2_ip4(ip));
      inet6_addr_from_ip6addr(&addr->sin6_addr, ip_2_ip6(&mapped));
    } else {
      inet6_addr_from_ip6addr(&addr->sin6_addr, ip_2_ip6(ip));
    }
    return 0;
  }
#endif
  return -1;
}

// Clear arg, recv, and err callbacks, then abort a connected PCB.
// Only valid for full tcp_pcb (not tcp_pcb_listen).
// Must be called before destroying the object that tcp_arg points to —
// tcp_abort() triggers the err callback synchronously, which would
// otherwise call back into a partially-destroyed object.
// tcp_sent/tcp_poll are not cleared because this implementation
// never registers them.
static void pcb_detach_abort(struct tcp_pcb *pcb) {
  tcp_arg(pcb, nullptr);
  tcp_recv(pcb, nullptr);
  tcp_err(pcb, nullptr);
  tcp_abort(pcb);
}

// Clear arg, recv, and err callbacks, then gracefully close a connected PCB.
// Only valid for full tcp_pcb (not tcp_pcb_listen).
// After tcp_close(), the PCB remains alive during the TCP close handshake
// (FIN_WAIT, TIME_WAIT states). Without clearing callbacks first, LWIP
// would call recv/err on a destroyed socket object, corrupting the heap.
// tcp_sent/tcp_poll are not cleared because this implementation
// never registers them.
// Returns ERR_OK on success; on failure the PCB is aborted instead.
static err_t pcb_detach_close(struct tcp_pcb *pcb) {
  tcp_arg(pcb, nullptr);
  tcp_recv(pcb, nullptr);
  tcp_err(pcb, nullptr);
  err_t err = tcp_close(pcb);
  if (err != ERR_OK) {
    tcp_abort(pcb);
  }
  return err;
}

/// Convert sockaddr to lwip ip_addr_t and host-order port.
/// For IPv6, sets type to IPADDR_TYPE_V6 (callers that need dual-stack should
/// override to IPADDR_TYPE_ANY after calling).
/// Shared by both TCP (LWIPRawCommon) and UDP (LWIPRawUDPImpl) bind/sendto paths.
static bool sockaddr_to_lwip(const struct sockaddr *addr, socklen_t addrlen, ip_addr_t *ip, uint16_t *port) {
  if (addrlen < sizeof(struct sockaddr))
    return false;
#if LWIP_IPV6
  if (addr->sa_family == AF_INET) {
    if (addrlen < sizeof(sockaddr_in))
      return false;
    auto *addr4 = reinterpret_cast<const sockaddr_in *>(addr);
    *port = ntohs(addr4->sin_port);
    ip->type = IPADDR_TYPE_V4;
    ip->u_addr.ip4.addr = addr4->sin_addr.s_addr;
    return true;
  }
  if (addr->sa_family == AF_INET6) {
    if (addrlen < sizeof(sockaddr_in6))
      return false;
    auto *addr6 = reinterpret_cast<const sockaddr_in6 *>(addr);
    *port = ntohs(addr6->sin6_port);
    ip->type = IPADDR_TYPE_V6;
    memcpy(&ip->u_addr.ip6.addr, &addr6->sin6_addr.un.u8_addr, 16);
    return true;
  }
#else
  if (addr->sa_family == AF_INET) {
    if (addrlen < sizeof(sockaddr_in))
      return false;
    auto *addr4 = reinterpret_cast<const sockaddr_in *>(addr);
    *port = ntohs(addr4->sin_port);
    ip->addr = addr4->sin_addr.s_addr;
    return true;
  }
#endif
  return false;
}

/// Map lwip bind error to errno. Returns 0 on success, -1 on error with errno set.
static int lwip_bind_err(err_t err) {
  if (err == ERR_OK)
    return 0;
  if (err == ERR_USE) {
    errno = EADDRINUSE;
  } else if (err == ERR_VAL) {
    errno = EINVAL;
  } else {
    errno = EIO;
  }
  return -1;
}

// ---- LWIPRawCommon methods ----

LWIPRawCommon::~LWIPRawCommon() {
  LWIP_LOCK();
  if (this->pcb_ != nullptr) {
    LWIP_LOG("tcp_abort(%p)", this->pcb_);
    pcb_detach_abort(this->pcb_);
    this->pcb_ = nullptr;
  }
}

int LWIPRawCommon::bind(const struct sockaddr *name, socklen_t addrlen) {
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
  if (!sockaddr_to_lwip(name, addrlen, &ip, &port)) {
    errno = EINVAL;
    return -1;
  }
#if LWIP_IPV6
  // Use IPADDR_TYPE_ANY for dual-stack (accept both IPv4 and IPv6)
  if (this->family_ == AF_INET6) {
    ip.type = IPADDR_TYPE_ANY;
  }
#endif
  err_t err = tcp_bind(this->pcb_, &ip, port);
  LWIP_LOG("  -> err %d", err);
  return lwip_bind_err(err);
}

int LWIPRawCommon::close() {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  LWIP_LOG("tcp_close(%p)", this->pcb_);
  err_t err = pcb_detach_close(this->pcb_);
  this->pcb_ = nullptr;
  if (err != ERR_OK) {
    LWIP_LOG("  -> err %d", err);
    errno = err == ERR_MEM ? ENOMEM : EIO;
    return -1;
  }
  return 0;
}

int LWIPRawCommon::shutdown(int how) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  bool shut_rx = false, shut_tx = false;
  if (how == SHUT_RD) {
    shut_rx = true;
  } else if (how == SHUT_WR) {
    shut_tx = true;
  } else if (how == SHUT_RDWR) {
    shut_rx = shut_tx = true;
  } else {
    errno = EINVAL;
    return -1;
  }
  LWIP_LOG("tcp_shutdown(%p shut_rx=%d shut_tx=%d)", this->pcb_, shut_rx ? 1 : 0, shut_tx ? 1 : 0);
  err_t err = tcp_shutdown(this->pcb_, shut_rx, shut_tx);
  if (err != ERR_OK) {
    LWIP_LOG("  -> err %d", err);
    errno = err == ERR_MEM ? ENOMEM : EIO;
    return -1;
  }
  return 0;
}

int LWIPRawCommon::getpeername(struct sockaddr *name, socklen_t *addrlen) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  if (name == nullptr || addrlen == nullptr) {
    errno = EINVAL;
    return -1;
  }
  return this->ip2sockaddr_(&this->pcb_->remote_ip, this->pcb_->remote_port, name, addrlen);
}

int LWIPRawCommon::getsockname(struct sockaddr *name, socklen_t *addrlen) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  if (name == nullptr || addrlen == nullptr) {
    errno = EINVAL;
    return -1;
  }
  return this->ip2sockaddr_(&this->pcb_->local_ip, this->pcb_->local_port, name, addrlen);
}

size_t LWIPRawCommon::getpeername_to(std::span<char, SOCKADDR_STR_LEN> buf) {
  struct sockaddr_storage storage;
  socklen_t len = sizeof(storage);
  if (this->getpeername(reinterpret_cast<struct sockaddr *>(&storage), &len) != 0) {
    buf[0] = '\0';
    return 0;
  }
  return format_sockaddr_to(reinterpret_cast<struct sockaddr *>(&storage), len, buf);
}

size_t LWIPRawCommon::getsockname_to(std::span<char, SOCKADDR_STR_LEN> buf) {
  struct sockaddr_storage storage;
  socklen_t len = sizeof(storage);
  if (this->getsockname(reinterpret_cast<struct sockaddr *>(&storage), &len) != 0) {
    buf[0] = '\0';
    return 0;
  }
  return format_sockaddr_to(reinterpret_cast<struct sockaddr *>(&storage), len, buf);
}

int LWIPRawCommon::getsockopt(int level, int optname, void *optval, socklen_t *optlen) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  if (optlen == nullptr || optval == nullptr) {
    errno = EINVAL;
    return -1;
  }
  if (level == SOL_SOCKET && optname == SO_REUSEADDR) {
    if (*optlen < 4) {
      errno = EINVAL;
      return -1;
    }
    // lwip doesn't seem to have this feature. Don't send an error
    // to prevent warnings
    *reinterpret_cast<int *>(optval) = 1;
    *optlen = 4;
    return 0;
  }
  if (level == SOL_SOCKET && optname == SO_RCVTIMEO) {
    if (*optlen < sizeof(struct timeval)) {
      errno = EINVAL;
      return -1;
    }
    uint32_t ms = this->recv_timeout_cs_ * 10;
    auto *tv = reinterpret_cast<struct timeval *>(optval);
    tv->tv_sec = ms / 1000;
    tv->tv_usec = (ms % 1000) * 1000;
    *optlen = sizeof(struct timeval);
    return 0;
  }
  if (level == IPPROTO_TCP && optname == TCP_NODELAY) {
    if (*optlen < 4) {
      errno = EINVAL;
      return -1;
    }
    *reinterpret_cast<int *>(optval) = this->nodelay_;
    *optlen = 4;
    return 0;
  }

  errno = EINVAL;
  return -1;
}

int LWIPRawCommon::setsockopt(int level, int optname, const void *optval, socklen_t optlen) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  if (level == SOL_SOCKET && optname == SO_REUSEADDR) {
    if (optlen != 4) {
      errno = EINVAL;
      return -1;
    }
    // lwip doesn't seem to have this feature. Don't send an error
    // to prevent warnings
    return 0;
  }
  if (level == SOL_SOCKET && optname == SO_RCVTIMEO) {
    if (optlen < sizeof(struct timeval)) {
      errno = EINVAL;
      return -1;
    }
    const auto *tv = reinterpret_cast<const struct timeval *>(optval);
    uint32_t ms = tv->tv_sec * 1000 + tv->tv_usec / 1000;
    uint32_t cs = (ms + 9) / 10;  // round up to nearest centisecond
    this->recv_timeout_cs_ = cs > 255 ? 255 : static_cast<uint8_t>(cs);
    return 0;
  }
  if (level == SOL_SOCKET && optname == SO_SNDTIMEO) {
    // Raw TCP writes are non-blocking (tcp_write), so send timeout is a no-op.
    return 0;
  }
  if (level == IPPROTO_TCP && optname == TCP_NODELAY) {
    if (optlen != 4) {
      errno = EINVAL;
      return -1;
    }
    int val = *reinterpret_cast<const int *>(optval);
    this->nodelay_ = val;
    return 0;
  }

  errno = EINVAL;
  return -1;
}

int LWIPRawCommon::ip2sockaddr_(ip_addr_t *ip, uint16_t port, struct sockaddr *name, socklen_t *addrlen) {
  // TCP pcb stores port in network byte order; convert to host order for the shared helper
  return lwip_ip_to_sockaddr(this->family_, ip, ntohs(port), name, addrlen);
}

// ---- LWIPRawImpl methods ----

LWIPRawImpl::~LWIPRawImpl() {
  LWIP_LOCK();
  // Free any received pbufs that LWIP transferred ownership of via recv_fn.
  // tcp_abort() in the base destructor won't free these since LWIP considers
  // ownership transferred once the recv callback accepts them.
  if (this->rx_buf_ != nullptr) {
    pbuf_free(this->rx_buf_);
    this->rx_buf_ = nullptr;
  }
  // Base class destructor handles pcb_ cleanup via tcp_abort
}

void LWIPRawImpl::init(struct pbuf *initial_rx, bool initial_rx_closed) {
  LWIP_LOCK();
  LWIP_LOG("init(%p)", this->pcb_);
  tcp_arg(this->pcb_, this);
  tcp_recv(this->pcb_, LWIPRawImpl::s_recv_fn);
  tcp_err(this->pcb_, LWIPRawImpl::s_err_fn);
  if (initial_rx != nullptr) {
    this->rx_buf_ = initial_rx;
    this->rx_buf_offset_ = 0;
  }
  this->rx_closed_ = initial_rx_closed;
}

void LWIPRawImpl::s_err_fn(void *arg, err_t err) {
  // LWIP CALLBACK — runs from IRQ context on RP2040 (low-priority user IRQ).
  // No heap allocation allowed — malloc is not IRQ-safe (see #14687).
  // No LWIP_LOCK() needed — lwip core already holds the async_context lock.
  //
  // pcb is already freed when this callback is called
  // ERR_RST: connection was reset by remote host
  // ERR_ABRT: aborted through tcp_abort or TCP timer
  auto *arg_this = reinterpret_cast<LWIPRawImpl *>(arg);
  ESP_LOGVV(TAG, "socket %p: err(err=%d)", arg_this, err);
  arg_this->pcb_ = nullptr;
}

err_t LWIPRawImpl::s_recv_fn(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, err_t err) {
  auto *arg_this = reinterpret_cast<LWIPRawImpl *>(arg);
  return arg_this->recv_fn(pb, err);
}

err_t LWIPRawImpl::recv_fn(struct pbuf *pb, err_t err) {
  // LWIP CALLBACK — runs from IRQ context on RP2040 (low-priority user IRQ).
  // No heap allocation allowed — malloc is not IRQ-safe (see #14687).
  LWIP_LOG("recv(pb=%p err=%d)", pb, err);
  if (err != 0) {
    // "An error code if there has been an error receiving Only return ERR_ABRT if you have
    // called tcp_abort from within the callback function!"
    if (pb != nullptr) {
      pbuf_free(pb);
    }
    this->rx_closed_ = true;
    return ERR_OK;
  }
  if (pb == nullptr) {
    this->rx_closed_ = true;
    return ERR_OK;
  }
  if (this->rx_buf_ == nullptr) {
    // no need to copy because lwIP gave control of it to us
    this->rx_buf_ = pb;
    this->rx_buf_offset_ = 0;
  } else {
    pbuf_cat(this->rx_buf_, pb);
  }
  // Wake the main loop immediately so it can process the received data.
  esphome::wake_loop_any_context();
  return ERR_OK;
}

void LWIPRawImpl::wait_for_data_() {
  // Wait for data without holding LWIP_LOCK so recv_fn() can run on RP2040
  // (needs async_context lock).
  //
  // Loop until data arrives, connection closes, or the full timeout elapses.
  // wakeable_delay() may return early due to any wake source,
  // so we re-enter for the remaining time.
  uint32_t timeout_ms = this->recv_timeout_cs_ * 10;
  uint32_t start = millis();
  while (this->waiting_for_data_()) {
    uint32_t elapsed = millis() - start;
    if (elapsed >= timeout_ms)
      break;
    esphome::internal::wakeable_delay(timeout_ms - elapsed);
  }
}

ssize_t LWIPRawImpl::read_locked_(void *buf, size_t len) {
  // Caller must hold LWIP_LOCK. Copies available data from rx_buf_ into buf.
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  if (this->rx_closed_ && this->rx_buf_ == nullptr) {
    return 0;
  }
  if (len == 0) {
    return 0;
  }
  if (this->rx_buf_ == nullptr) {
    errno = EWOULDBLOCK;
    return -1;
  }

  size_t read = 0;
  uint8_t *buf8 = reinterpret_cast<uint8_t *>(buf);
  while (len && this->rx_buf_ != nullptr) {
    size_t pb_len = this->rx_buf_->len;
    size_t pb_left = pb_len - this->rx_buf_offset_;
    if (pb_left == 0)
      break;
    size_t copysize = std::min(len, pb_left);
    memcpy(buf8, reinterpret_cast<uint8_t *>(this->rx_buf_->payload) + this->rx_buf_offset_, copysize);

    if (pb_left == copysize) {
      // full pb copied, free it
      if (this->rx_buf_->next == nullptr) {
        // last buffer in chain
        pbuf_free(this->rx_buf_);
        this->rx_buf_ = nullptr;
        this->rx_buf_offset_ = 0;
      } else {
        auto *old_buf = this->rx_buf_;
        this->rx_buf_ = this->rx_buf_->next;
        pbuf_ref(this->rx_buf_);
        pbuf_free(old_buf);
        this->rx_buf_offset_ = 0;
      }
    } else {
      this->rx_buf_offset_ += copysize;
    }
    LWIP_LOG("tcp_recved(%p %u)", this->pcb_, copysize);
    tcp_recved(this->pcb_, copysize);

    buf8 += copysize;
    len -= copysize;
    read += copysize;
  }

  if (read == 0) {
    errno = EWOULDBLOCK;
    return -1;
  }

  return read;
}

ssize_t LWIPRawImpl::read(void *buf, size_t len) {
  // See waiting_for_data_() for safety of unlocked reads.
  if (this->recv_timeout_cs_ > 0 && this->waiting_for_data_()) {
    this->wait_for_data_();
  }

  LWIP_LOCK();
  return this->read_locked_(buf, len);
}

ssize_t LWIPRawImpl::readv(const struct iovec *iov, int iovcnt) {
  // See waiting_for_data_() for safety of unlocked reads.
  if (this->recv_timeout_cs_ > 0 && this->waiting_for_data_()) {
    this->wait_for_data_();
  }

  LWIP_LOCK();  // Hold for entire scatter-gather operation
  ssize_t ret = 0;
  for (int i = 0; i < iovcnt; i++) {
    ssize_t err = this->read_locked_(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len);
    if (err == -1) {
      if (ret != 0) {
        // if we already read some don't return an error
        break;
      }
      return err;
    }
    ret += err;
    if ((size_t) err != iov[i].iov_len)
      break;
  }
  return ret;
}

ssize_t LWIPRawImpl::internal_write_(const void *buf, size_t len) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  if (len == 0)
    return 0;
  if (buf == nullptr) {
    errno = EINVAL;
    return 0;
  }
  auto space = tcp_sndbuf(this->pcb_);
  if (space == 0) {
    errno = EWOULDBLOCK;
    return -1;
  }
  size_t to_send = std::min((size_t) space, len);
  LWIP_LOG("tcp_write(%p buf=%p %u)", this->pcb_, buf, to_send);
  err_t err = tcp_write(this->pcb_, buf, to_send, TCP_WRITE_FLAG_COPY);
  if (err == ERR_MEM) {
    LWIP_LOG("  -> err ERR_MEM");
    errno = EWOULDBLOCK;
    return -1;
  }
  if (err != ERR_OK) {
    LWIP_LOG("  -> err %d", err);
    errno = ECONNRESET;
    return -1;
  }
  return to_send;
}

int LWIPRawImpl::internal_output_() {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  LWIP_LOG("tcp_output(%p)", this->pcb_);
  err_t err = tcp_output(this->pcb_);
  if (err == ERR_ABRT) {
    // sometimes lwip returns ERR_ABRT for no apparent reason
    // the connection works fine afterwards, and back with ESPAsyncTCP we
    // indirectly also ignored this error
    // FIXME: figure out where this is returned and what it means in this context
    LWIP_LOG("  -> err ERR_ABRT");
    return 0;
  }
  if (err != ERR_OK) {
    LWIP_LOG("  -> err %d", err);
    errno = ECONNRESET;
    return -1;
  }
  return 0;
}

ssize_t LWIPRawImpl::write(const void *buf, size_t len) {
  LWIP_LOCK();  // Hold for write + optional output
  ssize_t written = this->internal_write_(buf, len);
  if (written == -1)
    return -1;
  if (written == 0) {
    // no need to output if nothing written
    return 0;
  }
  if (this->nodelay_) {
    int err = this->internal_output_();
    if (err == -1)
      return -1;
  }
  return written;
}

ssize_t LWIPRawImpl::writev(const struct iovec *iov, int iovcnt) {
  LWIP_LOCK();  // Hold for entire scatter-gather operation
  ssize_t written = 0;
  for (int i = 0; i < iovcnt; i++) {
    ssize_t err = this->internal_write_(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len);
    if (err == -1) {
      if (written != 0) {
        // if we already read some don't return an error
        break;
      }
      return err;
    }
    written += err;
    if ((size_t) err != iov[i].iov_len)
      break;
  }
  if (written == 0) {
    // no need to output if nothing written
    return 0;
  }
  if (this->nodelay_) {
    int err = this->internal_output_();
    if (err == -1)
      return -1;
  }
  return written;
}

// ---- LWIPRawListenImpl methods ----

LWIPRawListenImpl::~LWIPRawListenImpl() {
  LWIP_LOCK();
  // Abort any queued PCBs that were never accepted by the main loop.
  for (uint8_t i = 0; i < this->accepted_socket_count_; i++) {
    auto &entry = this->accepted_pcbs_[i];
    if (entry.pcb != nullptr) {
      pcb_detach_abort(entry.pcb);
      entry.pcb = nullptr;
    }
    if (entry.rx_buf != nullptr) {
      pbuf_free(entry.rx_buf);
      entry.rx_buf = nullptr;
    }
  }
  this->accepted_socket_count_ = 0;
  // Listen PCBs must use tcp_close(), not tcp_abort().
  // tcp_abandon() asserts pcb->state != LISTEN and would access
  // fields that don't exist in the smaller tcp_pcb_listen struct.
  // Don't use pcb_detach_close() here — tcp_recv()/tcp_err() also access
  // fields that only exist in the full tcp_pcb, not tcp_pcb_listen.
  // tcp_close() on a listen PCB is synchronous (frees immediately),
  // so there are no async callbacks to worry about.
  // Close here and null pcb_ so the base destructor skips tcp_abort.
  if (this->pcb_ != nullptr) {
    tcp_close(this->pcb_);
    this->pcb_ = nullptr;
  }
}

void LWIPRawListenImpl::init() {
  LWIP_LOCK();
  LWIP_LOG("init(%p)", this->pcb_);
  tcp_arg(this->pcb_, this);
  tcp_accept(this->pcb_, LWIPRawListenImpl::s_accept_fn);
  tcp_err(this->pcb_, LWIPRawListenImpl::s_err_fn);
}

void LWIPRawListenImpl::s_err_fn(void *arg, err_t err) {
  // LWIP CALLBACK — runs from IRQ context on RP2040 (low-priority user IRQ).
  // No heap allocation allowed — malloc is not IRQ-safe (see #14687).
  auto *arg_this = reinterpret_cast<LWIPRawListenImpl *>(arg);
  ESP_LOGVV(TAG, "socket %p: err(err=%d)", arg_this, err);
  arg_this->pcb_ = nullptr;
}

void LWIPRawListenImpl::s_queued_err_fn(void *arg, err_t err) {
  // LWIP CALLBACK — runs from IRQ context on RP2040 (low-priority user IRQ).
  // No heap allocation allowed — malloc is not IRQ-safe (see #14687).
  // Called when a queued (not yet accepted) PCB errors — e.g., remote sent RST.
  // The PCB is already freed by lwip. Null our pointer so accept() skips it.
  (void) err;
  auto *entry = reinterpret_cast<QueuedPcb *>(arg);
  entry->pcb = nullptr;
  // Don't free rx_buf here — accept() will clean it up when it sees pcb==nullptr
}

err_t LWIPRawListenImpl::s_queued_recv_fn(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, err_t err) {
  // LWIP CALLBACK — runs from IRQ context on RP2040 (low-priority user IRQ).
  // No heap allocation allowed — malloc is not IRQ-safe (see #14687).
  // Temporary recv callback for PCBs queued between accept_fn_ and accept().
  // Without this, lwip's default tcp_recv_null handler would ACK and drop the data,
  // causing the API handshake to silently fail (client sends Hello, server never sees it).
  (void) pcb;
  auto *entry = reinterpret_cast<QueuedPcb *>(arg);
  if (pb == nullptr || err != ERR_OK) {
    // Remote closed or error
    if (pb != nullptr) {
      pbuf_free(pb);
    }
    entry->rx_closed = true;
    return ERR_OK;
  }
  // Buffer the data — tcp_recved() is deferred to read() after accept() creates the socket.
  if (entry->rx_buf == nullptr) {
    entry->rx_buf = pb;
  } else {
    pbuf_cat(entry->rx_buf, pb);
  }
  return ERR_OK;
}

err_t LWIPRawListenImpl::s_accept_fn(void *arg, struct tcp_pcb *newpcb, err_t err) {
  auto *arg_this = reinterpret_cast<LWIPRawListenImpl *>(arg);
  return arg_this->accept_fn_(newpcb, err);
}

std::unique_ptr<LWIPRawImpl> LWIPRawListenImpl::accept(struct sockaddr *addr, socklen_t *addrlen) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return nullptr;
  }
  // Dequeue front entry, skipping any null entries (PCBs freed by lwip while queued).
  // The error callback nulled their pcb pointers; clean up buffered data and discard.
  while (this->accepted_socket_count_ > 0) {
    QueuedPcb entry = this->accepted_pcbs_[0];
    // Shift remaining entries forward, updating tcp_arg pointers as we go.
    // Safe because we hold LWIP_LOCK, so err/recv callbacks can't fire during the update.
    for (uint8_t i = 1; i < this->accepted_socket_count_; i++) {
      this->accepted_pcbs_[i - 1] = this->accepted_pcbs_[i];
      if (this->accepted_pcbs_[i - 1].pcb != nullptr) {
        tcp_arg(this->accepted_pcbs_[i - 1].pcb, &this->accepted_pcbs_[i - 1]);
      }
    }
    this->accepted_pcbs_[this->accepted_socket_count_ - 1] = {};
    this->accepted_socket_count_--;
    if (entry.pcb == nullptr) {
      // PCB was freed by lwip (RST/timeout) while queued — discard and try next
      if (entry.rx_buf != nullptr) {
        pbuf_free(entry.rx_buf);
      }
      continue;
    }
    LWIP_LOG("Connection accepted by application, queue size: %d", this->accepted_socket_count_);
    // Create socket wrapper on the main loop (not in accept callback) to avoid
    // heap allocation in IRQ context on RP2040. Transfer any data received while queued.
    auto sock = make_unique<LWIPRawImpl>(this->family_, entry.pcb);
    sock->init(entry.rx_buf, entry.rx_closed);
    if (addr != nullptr) {
      sock->getpeername(addr, addrlen);
    }
    LWIP_LOG("accept(%p)", sock.get());
    return sock;
  }
  errno = EWOULDBLOCK;
  return nullptr;
}

int LWIPRawListenImpl::listen(int backlog) {
  LWIP_LOCK();
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return -1;
  }
  LWIP_LOG("tcp_listen_with_backlog(%p backlog=%d)", this->pcb_, backlog);
  struct tcp_pcb *listen_pcb = tcp_listen_with_backlog(this->pcb_, backlog);
  if (listen_pcb == nullptr) {
    tcp_abort(this->pcb_);
    this->pcb_ = nullptr;
    errno = EOPNOTSUPP;
    return -1;
  }
  // tcp_listen reallocates the pcb, replace ours
  this->pcb_ = listen_pcb;
  // set callbacks on new pcb
  LWIP_LOG("tcp_arg(%p)", this->pcb_);
  tcp_arg(this->pcb_, this);
  tcp_accept(this->pcb_, LWIPRawListenImpl::s_accept_fn);
  // Note: tcp_err() is NOT re-registered here. tcp_listen_with_backlog() converts the
  // full tcp_pcb to a smaller tcp_pcb_listen struct that lacks the errf field.
  // Calling tcp_err() on a listen PCB writes past the struct boundary (undefined behavior).
  return 0;
}

err_t LWIPRawListenImpl::accept_fn_(struct tcp_pcb *newpcb, err_t err) {
  // LWIP CALLBACK — runs from IRQ context on RP2040 (low-priority user IRQ).
  // No heap allocation allowed — malloc is not IRQ-safe (see #14687).
  LWIP_LOG("accept(newpcb=%p err=%d)", newpcb, err);
  if (err != ERR_OK || newpcb == nullptr) {
    // "An error code if there has been an error accepting. Only return ERR_ABRT if you have
    // called tcp_abort from within the callback function!"
    // https://www.nongnu.org/lwip/2_1_x/tcp_8h.html#a00517abce6856d6c82f0efebdafb734d
    // nothing to do here, we just don't push it to the queue
    return ERR_OK;
  }
  // Check if we've reached the maximum accept queue size
  if (this->accepted_socket_count_ >= MAX_ACCEPTED_SOCKETS) {
    LWIP_LOG("Rejecting connection, queue full (%d)", this->accepted_socket_count_);
    // Abort the connection when queue is full
    tcp_abort(newpcb);
    // Must return ERR_ABRT since we called tcp_abort()
    return ERR_ABRT;
  }
  // Store the raw PCB — LWIPRawImpl creation is deferred to the main-loop accept().
  // This avoids heap allocation in this callback, which is unsafe from IRQ context on RP2040.
  uint8_t idx = this->accepted_socket_count_++;
  this->accepted_pcbs_[idx] = {newpcb, nullptr, false};
  // Register temporary callbacks so that while the PCB is queued:
  // - err: nulls our pointer if the connection errors (RST, timeout)
  // - recv: buffers any data that arrives before accept() creates the LWIPRawImpl
  //   (without this, lwip's default tcp_recv_null would ACK and drop the data)
  // tcp_arg points to our queue entry; accept() updates these pointers after shifting.
  tcp_arg(newpcb, &this->accepted_pcbs_[idx]);
  tcp_err(newpcb, LWIPRawListenImpl::s_queued_err_fn);
  tcp_recv(newpcb, LWIPRawListenImpl::s_queued_recv_fn);
  LWIP_LOG("Accepted connection, queue size: %d", this->accepted_socket_count_);
  // Wake the main loop immediately so it can accept the new connection.
  esphome::wake_loop_any_context();
  return ERR_OK;
}

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

// ---- Factory functions ----

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  if (type != SOCK_STREAM) {
    ESP_LOGE(TAG, "Use socket_udp() for UDP sockets on this platform");
    errno = EPROTOTYPE;
    return nullptr;
  }
  LWIP_LOCK();
  auto *pcb = tcp_new();
  if (pcb == nullptr)
    return nullptr;
  auto *sock = new LWIPRawImpl((sa_family_t) domain, pcb);  // NOLINT(cppcoreguidelines-owning-memory)
  sock->init();
  return std::unique_ptr<Socket>{sock};
}

std::unique_ptr<Socket> socket_loop_monitored(int domain, int type, int protocol) {
  // LWIPRawImpl doesn't use file descriptors, so monitoring is not applicable
  return socket(domain, type, protocol);
}

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

std::unique_ptr<ListenSocket> socket_listen(int domain, int type, int protocol) {
  if (type != SOCK_STREAM) {
    ESP_LOGE(TAG, "Use socket_udp_recv() for UDP sockets on this platform");
    errno = EPROTOTYPE;
    return nullptr;
  }
  LWIP_LOCK();
  auto *pcb = tcp_new();
  if (pcb == nullptr)
    return nullptr;
  auto *sock = new LWIPRawListenImpl((sa_family_t) domain, pcb);  // NOLINT(cppcoreguidelines-owning-memory)
  sock->init();
  return std::unique_ptr<ListenSocket>{sock};
}

std::unique_ptr<ListenSocket> socket_listen_loop_monitored(int domain, int type, int protocol) {
  // LWIPRawImpl doesn't use file descriptors, so monitoring is not applicable
  return socket_listen(domain, type, protocol);
}

#undef LWIP_LOCK

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_TCP
