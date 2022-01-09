#include "socket.h"
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include "lwip/ip.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/tcp.h"
#include <cerrno>
#include <cstring>
#include <queue>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace socket {

static const char *const TAG = "socket.lwip";

// set to 1 to enable verbose lwip logging
#if 0
#define LWIP_LOG(msg, ...) ESP_LOGVV(TAG, "socket %p: " msg, this, ##__VA_ARGS__)
#else
#define LWIP_LOG(msg, ...)
#endif

class LWIPRawImpl : public Socket {
 public:
  LWIPRawImpl(struct tcp_pcb *pcb) : pcb_(pcb) {}
  ~LWIPRawImpl() override {
    if (pcb_ != nullptr) {
      LWIP_LOG("tcp_abort(%p)", pcb_);
      tcp_abort(pcb_);
      pcb_ = nullptr;
    }
  }

  void init() {
    LWIP_LOG("init(%p)", pcb_);
    tcp_arg(pcb_, this);
    tcp_accept(pcb_, LWIPRawImpl::s_accept_fn);
    tcp_recv(pcb_, LWIPRawImpl::s_recv_fn);
    tcp_err(pcb_, LWIPRawImpl::s_err_fn);
  }

  std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) override {
    if (pcb_ == nullptr) {
      errno = EBADF;
      return nullptr;
    }
    if (accepted_sockets_.empty()) {
      errno = EWOULDBLOCK;
      return nullptr;
    }
    std::unique_ptr<LWIPRawImpl> sock = std::move(accepted_sockets_.front());
    accepted_sockets_.pop();
    if (addr != nullptr) {
      sock->getpeername(addr, addrlen);
    }
    LWIP_LOG("accept(%p)", sock.get());
    return std::unique_ptr<Socket>(std::move(sock));
  }
  int bind(const struct sockaddr *name, socklen_t addrlen) override {
    if (pcb_ == nullptr) {
      errno = EBADF;
      return -1;
    }
    if (name == nullptr) {
      errno = EINVAL;
      return 0;
    }
    ip_addr_t ip;
    in_port_t port;
    auto family = name->sa_family;
#if LWIP_IPV6
    if (family == AF_INET) {
      if (addrlen < sizeof(sockaddr_in6)) {
        errno = EINVAL;
        return -1;
      }
      auto *addr4 = reinterpret_cast<const sockaddr_in *>(name);
      port = ntohs(addr4->sin_port);
      ip.type = IPADDR_TYPE_V4;
      ip.u_addr.ip4.addr = addr4->sin_addr.s_addr;

    } else if (family == AF_INET6) {
      if (addrlen < sizeof(sockaddr_in)) {
        errno = EINVAL;
        return -1;
      }
      auto *addr6 = reinterpret_cast<const sockaddr_in6 *>(name);
      port = ntohs(addr6->sin6_port);
      ip.type = IPADDR_TYPE_V6;
      memcpy(&ip.u_addr.ip6.addr, &addr6->sin6_addr.un.u8_addr, 16);
    } else {
      errno = EINVAL;
      return -1;
    }
#else
    if (family != AF_INET) {
      errno = EINVAL;
      return -1;
    }
    auto *addr4 = reinterpret_cast<const sockaddr_in *>(name);
    port = ntohs(addr4->sin_port);
    ip.addr = addr4->sin_addr.s_addr;
#endif
    LWIP_LOG("tcp_bind(%p ip=%u port=%u)", pcb_, ip.addr, port);
    err_t err = tcp_bind(pcb_, &ip, port);
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
  int close() override {
    if (pcb_ == nullptr) {
      errno = ECONNRESET;
      return -1;
    }
    LWIP_LOG("tcp_close(%p)", pcb_);
    err_t err = tcp_close(pcb_);
    if (err != ERR_OK) {
      LWIP_LOG("  -> err %d", err);
      tcp_abort(pcb_);
      pcb_ = nullptr;
      errno = err == ERR_MEM ? ENOMEM : EIO;
      return -1;
    }
    pcb_ = nullptr;
    return 0;
  }
  int shutdown(int how) override {
    if (pcb_ == nullptr) {
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
    LWIP_LOG("tcp_shutdown(%p shut_rx=%d shut_tx=%d)", pcb_, shut_rx ? 1 : 0, shut_tx ? 1 : 0);
    err_t err = tcp_shutdown(pcb_, shut_rx, shut_tx);
    if (err != ERR_OK) {
      LWIP_LOG("  -> err %d", err);
      errno = err == ERR_MEM ? ENOMEM : EIO;
      return -1;
    }
    return 0;
  }

  int getpeername(struct sockaddr *name, socklen_t *addrlen) override {
    if (pcb_ == nullptr) {
      errno = ECONNRESET;
      return -1;
    }
    if (name == nullptr || addrlen == nullptr) {
      errno = EINVAL;
      return -1;
    }
    if (*addrlen < sizeof(struct sockaddr_in)) {
      errno = EINVAL;
      return -1;
    }
    struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in *>(name);
    addr->sin_family = AF_INET;
    *addrlen = addr->sin_len = sizeof(struct sockaddr_in);
    addr->sin_port = pcb_->remote_port;
    addr->sin_addr.s_addr = pcb_->remote_ip.addr;
    return 0;
  }
  std::string getpeername() override {
    if (pcb_ == nullptr) {
      errno = ECONNRESET;
      return "";
    }
    char buffer[24];
    uint32_t ip4 = pcb_->remote_ip.addr;
    snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", (ip4 >> 0) & 0xFF, (ip4 >> 8) & 0xFF, (ip4 >> 16) & 0xFF,
             (ip4 >> 24) & 0xFF);
    return std::string(buffer);
  }
  int getsockname(struct sockaddr *name, socklen_t *addrlen) override {
    if (pcb_ == nullptr) {
      errno = ECONNRESET;
      return -1;
    }
    if (name == nullptr || addrlen == nullptr) {
      errno = EINVAL;
      return -1;
    }
    if (*addrlen < sizeof(struct sockaddr_in)) {
      errno = EINVAL;
      return -1;
    }
    struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in *>(name);
    addr->sin_family = AF_INET;
    *addrlen = addr->sin_len = sizeof(struct sockaddr_in);
    addr->sin_port = pcb_->local_port;
    addr->sin_addr.s_addr = pcb_->local_ip.addr;
    return 0;
  }
  std::string getsockname() override {
    if (pcb_ == nullptr) {
      errno = ECONNRESET;
      return "";
    }
    char buffer[24];
    uint32_t ip4 = pcb_->local_ip.addr;
    snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", (ip4 >> 0) & 0xFF, (ip4 >> 8) & 0xFF, (ip4 >> 16) & 0xFF,
             (ip4 >> 24) & 0xFF);
    return std::string(buffer);
  }
  int getsockopt(int level, int optname, void *optval, socklen_t *optlen) override {
    if (pcb_ == nullptr) {
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
      *reinterpret_cast<int *>(optval) = nodelay_;
      *optlen = 4;
      return 0;
    }

    errno = EINVAL;
    return -1;
  }
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen) override {
    if (pcb_ == nullptr) {
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
      nodelay_ = val;
      return 0;
    }

    errno = EINVAL;
    return -1;
  }
  int listen(int backlog) override {
    if (pcb_ == nullptr) {
      errno = EBADF;
      return -1;
    }
    LWIP_LOG("tcp_listen_with_backlog(%p backlog=%d)", pcb_, backlog);
    struct tcp_pcb *listen_pcb = tcp_listen_with_backlog(pcb_, backlog);
    if (listen_pcb == nullptr) {
      tcp_abort(pcb_);
      pcb_ = nullptr;
      errno = EOPNOTSUPP;
      return -1;
    }
    // tcp_listen reallocates the pcb, replace ours
    pcb_ = listen_pcb;
    // set callbacks on new pcb
    LWIP_LOG("tcp_arg(%p)", pcb_);
    tcp_arg(pcb_, this);
    tcp_accept(pcb_, LWIPRawImpl::s_accept_fn);
    return 0;
  }
  ssize_t read(void *buf, size_t len) override {
    if (pcb_ == nullptr) {
      errno = ECONNRESET;
      return -1;
    }
    if (rx_closed_ && rx_buf_ == nullptr) {
      return 0;
    }
    if (len == 0) {
      return 0;
    }
    if (rx_buf_ == nullptr) {
      errno = EWOULDBLOCK;
      return -1;
    }

    size_t read = 0;
    uint8_t *buf8 = reinterpret_cast<uint8_t *>(buf);
    while (len && rx_buf_ != nullptr) {
      size_t pb_len = rx_buf_->len;
      size_t pb_left = pb_len - rx_buf_offset_;
      if (pb_left == 0)
        break;
      size_t copysize = std::min(len, pb_left);
      memcpy(buf8, reinterpret_cast<uint8_t *>(rx_buf_->payload) + rx_buf_offset_, copysize);

      if (pb_left == copysize) {
        // full pb copied, free it
        if (rx_buf_->next == nullptr) {
          // last buffer in chain
          pbuf_free(rx_buf_);
          rx_buf_ = nullptr;
          rx_buf_offset_ = 0;
        } else {
          auto *old_buf = rx_buf_;
          rx_buf_ = rx_buf_->next;
          pbuf_ref(rx_buf_);
          pbuf_free(old_buf);
          rx_buf_offset_ = 0;
        }
      } else {
        rx_buf_offset_ += copysize;
      }
      LWIP_LOG("tcp_recved(%p %u)", pcb_, copysize);
      tcp_recved(pcb_, copysize);

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
  ssize_t readv(const struct iovec *iov, int iovcnt) override {
    ssize_t ret = 0;
    for (int i = 0; i < iovcnt; i++) {
      ssize_t err = read(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len);
      if (err == -1) {
        if (ret != 0)
          // if we already read some don't return an error
          break;
        return err;
      }
      ret += err;
      if ((size_t) err != iov[i].iov_len)
        break;
    }
    return ret;
  }
  ssize_t internal_write(const void *buf, size_t len) {
    if (pcb_ == nullptr) {
      errno = ECONNRESET;
      return -1;
    }
    if (len == 0)
      return 0;
    if (buf == nullptr) {
      errno = EINVAL;
      return 0;
    }
    auto space = tcp_sndbuf(pcb_);
    if (space == 0) {
      errno = EWOULDBLOCK;
      return -1;
    }
    size_t to_send = std::min((size_t) space, len);
    LWIP_LOG("tcp_write(%p buf=%p %u)", pcb_, buf, to_send);
    err_t err = tcp_write(pcb_, buf, to_send, TCP_WRITE_FLAG_COPY);
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
  int internal_output() {
    LWIP_LOG("tcp_output(%p)", pcb_);
    err_t err = tcp_output(pcb_);
    if (err == ERR_ABRT) {
      LWIP_LOG("  -> err ERR_ABRT");
      // sometimes lwip returns ERR_ABRT for no apparent reason
      // the connection works fine afterwards, and back with ESPAsyncTCP we
      // indirectly also ignored this error
      // FIXME: figure out where this is returned and what it means in this context
      return 0;
    }
    if (err != ERR_OK) {
      LWIP_LOG("  -> err %d", err);
      errno = ECONNRESET;
      return -1;
    }
    return 0;
  }
  ssize_t write(const void *buf, size_t len) override {
    ssize_t written = internal_write(buf, len);
    if (written == -1)
      return -1;
    if (written == 0)
      // no need to output if nothing written
      return 0;
    if (nodelay_) {
      int err = internal_output();
      if (err == -1)
        return -1;
    }
    return written;
  }
  ssize_t writev(const struct iovec *iov, int iovcnt) override {
    ssize_t written = 0;
    for (int i = 0; i < iovcnt; i++) {
      ssize_t err = internal_write(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len);
      if (err == -1) {
        if (written != 0)
          // if we already read some don't return an error
          break;
        return err;
      }
      written += err;
      if ((size_t) err != iov[i].iov_len)
        break;
    }
    if (written == 0)
      // no need to output if nothing written
      return 0;
    if (nodelay_) {
      int err = internal_output();
      if (err == -1)
        return -1;
    }
    return written;
  }
  int setblocking(bool blocking) override {
    if (pcb_ == nullptr) {
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

  err_t accept_fn(struct tcp_pcb *newpcb, err_t err) {
    LWIP_LOG("accept(newpcb=%p err=%d)", newpcb, err);
    if (err != ERR_OK || newpcb == nullptr) {
      // "An error code if there has been an error accepting. Only return ERR_ABRT if you have
      // called tcp_abort from within the callback function!"
      // https://www.nongnu.org/lwip/2_1_x/tcp_8h.html#a00517abce6856d6c82f0efebdafb734d
      // nothing to do here, we just don't push it to the queue
      return ERR_OK;
    }
    auto sock = make_unique<LWIPRawImpl>(newpcb);
    sock->init();
    accepted_sockets_.push(std::move(sock));
    return ERR_OK;
  }
  void err_fn(err_t err) {
    LWIP_LOG("err(err=%d)", err);
    // "If a connection is aborted because of an error, the application is alerted of this event by
    // the err callback."
    // pcb is already freed when this callback is called
    // ERR_RST: connection was reset by remote host
    // ERR_ABRT: aborted through tcp_abort or TCP timer
    pcb_ = nullptr;
  }
  err_t recv_fn(struct pbuf *pb, err_t err) {
    LWIP_LOG("recv(pb=%p err=%d)", pb, err);
    if (err != 0) {
      // "An error code if there has been an error receiving Only return ERR_ABRT if you have
      // called tcp_abort from within the callback function!"
      rx_closed_ = true;
      return ERR_OK;
    }
    if (pb == nullptr) {
      rx_closed_ = true;
      return ERR_OK;
    }
    if (rx_buf_ == nullptr) {
      // no need to copy because lwIP gave control of it to us
      rx_buf_ = pb;
      rx_buf_offset_ = 0;
    } else {
      pbuf_cat(rx_buf_, pb);
    }
    return ERR_OK;
  }

  static err_t s_accept_fn(void *arg, struct tcp_pcb *newpcb, err_t err) {
    LWIPRawImpl *arg_this = reinterpret_cast<LWIPRawImpl *>(arg);
    return arg_this->accept_fn(newpcb, err);
  }

  static void s_err_fn(void *arg, err_t err) {
    LWIPRawImpl *arg_this = reinterpret_cast<LWIPRawImpl *>(arg);
    arg_this->err_fn(err);
  }

  static err_t s_recv_fn(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, err_t err) {
    LWIPRawImpl *arg_this = reinterpret_cast<LWIPRawImpl *>(arg);
    return arg_this->recv_fn(pb, err);
  }

 protected:
  struct tcp_pcb *pcb_;
  std::queue<std::unique_ptr<LWIPRawImpl>> accepted_sockets_;
  bool rx_closed_ = false;
  pbuf *rx_buf_ = nullptr;
  size_t rx_buf_offset_ = 0;
  // don't use lwip nodelay flag, it sometimes causes reconnect
  // instead use it for determining whether to call lwip_output
  bool nodelay_ = false;
};

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  auto *pcb = tcp_new();
  if (pcb == nullptr)
    return nullptr;
  auto *sock = new LWIPRawImpl(pcb);  // NOLINT(cppcoreguidelines-owning-memory)
  sock->init();
  return std::unique_ptr<Socket>{sock};
}

}  // namespace socket
}  // namespace esphome

#endif  // USE_SOCKET_IMPL_LWIP_TCP
