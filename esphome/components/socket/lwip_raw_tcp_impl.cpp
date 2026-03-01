#include "socket.h"
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include <cerrno>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP8266
#include <coredecls.h>  // For esp_schedule()
#endif

namespace esphome::socket {

#ifdef USE_ESP8266
// Flag to signal socket activity - checked by socket_delay() to exit early
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile bool s_socket_woke = false;

void socket_delay(uint32_t ms) {
  // Use esp_delay with a callback that checks if socket data arrived.
  // This allows the delay to exit early when socket_wake() is called by
  // lwip recv_fn/accept_fn callbacks, reducing socket latency.
  //
  // When ms is 0, we must use delay(0) because esp_delay(0, callback)
  // exits immediately without yielding, which can cause watchdog timeouts
  // when the main loop runs in high-frequency mode (e.g., during light effects).
  if (ms == 0) {
    delay(0);
    return;
  }
  s_socket_woke = false;
  esp_delay(ms, []() { return !s_socket_woke; });
}

void socket_wake() {
  s_socket_woke = true;
  esp_schedule();
}
#endif

static const char *const TAG = "socket.lwip";

// set to 1 to enable verbose lwip logging
#if 0  // NOLINT(readability-avoid-unconditional-preprocessor-if)
#define LWIP_LOG(msg, ...) ESP_LOGVV(TAG, "socket %p: " msg, this, ##__VA_ARGS__)
#else
#define LWIP_LOG(msg, ...)
#endif

// ---- LWIPRawCommon methods ----

LWIPRawCommon::~LWIPRawCommon() {
  if (this->pcb_ != nullptr) {
    LWIP_LOG("tcp_abort(%p)", this->pcb_);
    tcp_abort(this->pcb_);
    this->pcb_ = nullptr;
  }
}

int LWIPRawCommon::bind(const struct sockaddr *name, socklen_t addrlen) {
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return -1;
  }
  if (name == nullptr) {
    errno = EINVAL;
    return -1;
  }
  ip_addr_t ip;
  in_port_t port;
#if LWIP_IPV6
  if (this->family_ == AF_INET) {
    if (addrlen < sizeof(sockaddr_in)) {
      errno = EINVAL;
      return -1;
    }
    auto *addr4 = reinterpret_cast<const sockaddr_in *>(name);
    port = ntohs(addr4->sin_port);
    ip.type = IPADDR_TYPE_V4;
    ip.u_addr.ip4.addr = addr4->sin_addr.s_addr;
    LWIP_LOG("tcp_bind(%p ip=%s port=%u)", this->pcb_, ip4addr_ntoa(&ip.u_addr.ip4), port);
  } else if (this->family_ == AF_INET6) {
    if (addrlen < sizeof(sockaddr_in6)) {
      errno = EINVAL;
      return -1;
    }
    auto *addr6 = reinterpret_cast<const sockaddr_in6 *>(name);
    port = ntohs(addr6->sin6_port);
    ip.type = IPADDR_TYPE_ANY;
    memcpy(&ip.u_addr.ip6.addr, &addr6->sin6_addr.un.u8_addr, 16);
    LWIP_LOG("tcp_bind(%p ip=%s port=%u)", this->pcb_, ip6addr_ntoa(&ip.u_addr.ip6), port);
  } else {
    errno = EINVAL;
    return -1;
  }
#else
  if (this->family_ != AF_INET) {
    errno = EINVAL;
    return -1;
  }
  auto *addr4 = reinterpret_cast<const sockaddr_in *>(name);
  port = ntohs(addr4->sin_port);
  ip.addr = addr4->sin_addr.s_addr;
  LWIP_LOG("tcp_bind(%p ip=%u port=%u)", this->pcb_, ip.addr, port);
#endif
  err_t err = tcp_bind(this->pcb_, &ip, port);
  if (err == ERR_USE) {
    LWIP_LOG("  -> err ERR_USE");
    errno = EADDRINUSE;
    return -1;
  }
  if (err == ERR_VAL) {
    LWIP_LOG("  -> err ERR_VAL");
    errno = EINVAL;
    return -1;
  }
  if (err != ERR_OK) {
    LWIP_LOG("  -> err %d", err);
    errno = EIO;
    return -1;
  }
  return 0;
}

int LWIPRawCommon::close() {
  if (this->pcb_ == nullptr) {
    errno = ECONNRESET;
    return -1;
  }
  LWIP_LOG("tcp_close(%p)", this->pcb_);
  err_t err = tcp_close(this->pcb_);
  if (err != ERR_OK) {
    LWIP_LOG("  -> err %d", err);
    tcp_abort(this->pcb_);
    this->pcb_ = nullptr;
    errno = err == ERR_MEM ? ENOMEM : EIO;
    return -1;
  }
  this->pcb_ = nullptr;
  return 0;
}

int LWIPRawCommon::shutdown(int how) {
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
  if (this->family_ == AF_INET) {
    if (*addrlen < sizeof(struct sockaddr_in)) {
      errno = EINVAL;
      return -1;
    }

    struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in *>(name);
    addr->sin_family = AF_INET;
    *addrlen = addr->sin_len = sizeof(struct sockaddr_in);
    addr->sin_port = port;
    inet_addr_from_ip4addr(&addr->sin_addr, ip_2_ip4(ip));
    return 0;
  }
#if LWIP_IPV6
  else if (this->family_ == AF_INET6) {
    if (*addrlen < sizeof(struct sockaddr_in6)) {
      errno = EINVAL;
      return -1;
    }

    struct sockaddr_in6 *addr = reinterpret_cast<struct sockaddr_in6 *>(name);
    addr->sin6_family = AF_INET6;
    *addrlen = addr->sin6_len = sizeof(struct sockaddr_in6);
    addr->sin6_port = port;

    // AF_INET6 sockets are bound to IPv4 as well, so we may encounter IPv4 addresses that must be converted to IPv6.
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

// ---- LWIPRawImpl methods ----

LWIPRawImpl::~LWIPRawImpl() {
  // Free any received pbufs that LWIP transferred ownership of via recv_fn.
  // tcp_abort() in the base destructor won't free these since LWIP considers
  // ownership transferred once the recv callback accepts them.
  if (this->rx_buf_ != nullptr) {
    pbuf_free(this->rx_buf_);
    this->rx_buf_ = nullptr;
  }
  // Base class destructor handles pcb_ cleanup via tcp_abort
}

void LWIPRawImpl::init() {
  LWIP_LOG("init(%p)", this->pcb_);
  tcp_arg(this->pcb_, this);
  tcp_recv(this->pcb_, LWIPRawImpl::s_recv_fn);
  tcp_err(this->pcb_, LWIPRawImpl::s_err_fn);
}

void LWIPRawImpl::s_err_fn(void *arg, err_t err) {
  // "If a connection is aborted because of an error, the application is alerted of this event by
  // the err callback."
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
  LWIP_LOG("recv(pb=%p err=%d)", pb, err);
  if (err != 0) {
    // "An error code if there has been an error receiving Only return ERR_ABRT if you have
    // called tcp_abort from within the callback function!"
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
#ifdef USE_ESP8266
  // Wake the main loop immediately so it can process the received data.
  socket_wake();
#endif
  return ERR_OK;
}

ssize_t LWIPRawImpl::read(void *buf, size_t len) {
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

ssize_t LWIPRawImpl::readv(const struct iovec *iov, int iovcnt) {
  ssize_t ret = 0;
  for (int i = 0; i < iovcnt; i++) {
    ssize_t err = this->read(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len);
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
  // Listen PCBs must use tcp_close(), not tcp_abort().
  // tcp_abandon() asserts pcb->state != LISTEN and would access
  // fields that don't exist in the smaller tcp_pcb_listen struct.
  // Close here and null pcb_ so the base destructor skips tcp_abort.
  if (this->pcb_ != nullptr) {
    tcp_close(this->pcb_);
    this->pcb_ = nullptr;
  }
}

void LWIPRawListenImpl::init() {
  LWIP_LOG("init(%p)", this->pcb_);
  tcp_arg(this->pcb_, this);
  tcp_accept(this->pcb_, LWIPRawListenImpl::s_accept_fn);
  tcp_err(this->pcb_, LWIPRawListenImpl::s_err_fn);
}

void LWIPRawListenImpl::s_err_fn(void *arg, err_t err) {
  auto *arg_this = reinterpret_cast<LWIPRawListenImpl *>(arg);
  ESP_LOGVV(TAG, "socket %p: err(err=%d)", arg_this, err);
  arg_this->pcb_ = nullptr;
}

err_t LWIPRawListenImpl::s_accept_fn(void *arg, struct tcp_pcb *newpcb, err_t err) {
  auto *arg_this = reinterpret_cast<LWIPRawListenImpl *>(arg);
  return arg_this->accept_fn_(newpcb, err);
}

std::unique_ptr<LWIPRawImpl> LWIPRawListenImpl::accept(struct sockaddr *addr, socklen_t *addrlen) {
  if (this->pcb_ == nullptr) {
    errno = EBADF;
    return nullptr;
  }
  if (this->accepted_socket_count_ == 0) {
    errno = EWOULDBLOCK;
    return nullptr;
  }
  // Take from front for FIFO ordering
  std::unique_ptr<LWIPRawImpl> sock = std::move(this->accepted_sockets_[0]);
  // Shift remaining sockets forward
  for (uint8_t i = 1; i < this->accepted_socket_count_; i++) {
    this->accepted_sockets_[i - 1] = std::move(this->accepted_sockets_[i]);
  }
  this->accepted_socket_count_--;
  LWIP_LOG("Connection accepted by application, queue size: %d", this->accepted_socket_count_);
  if (addr != nullptr) {
    sock->getpeername(addr, addrlen);
  }
  LWIP_LOG("accept(%p)", sock.get());
  return sock;
}

int LWIPRawListenImpl::listen(int backlog) {
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
  auto sock = make_unique<LWIPRawImpl>(this->family_, newpcb);
  sock->init();
  this->accepted_sockets_[this->accepted_socket_count_++] = std::move(sock);
  LWIP_LOG("Accepted connection, queue size: %d", this->accepted_socket_count_);
#ifdef USE_ESP8266
  // Wake the main loop immediately so it can accept the new connection.
  socket_wake();
#endif
  return ERR_OK;
}

// ---- Factory functions ----

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  if (type != SOCK_STREAM) {
    ESP_LOGE(TAG, "UDP sockets not supported on this platform, use WiFiUDP");
    errno = EPROTOTYPE;
    return nullptr;
  }
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

std::unique_ptr<ListenSocket> socket_listen(int domain, int type, int protocol) {
  if (type != SOCK_STREAM) {
    ESP_LOGE(TAG, "UDP sockets not supported on this platform, use WiFiUDP");
    errno = EPROTOTYPE;
    return nullptr;
  }
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

}  // namespace esphome::socket

#endif  // USE_SOCKET_IMPL_LWIP_TCP
