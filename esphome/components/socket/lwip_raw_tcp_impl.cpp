#include "socket.h"
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#include <queue>
#include <string.h>
#include "lwip/opt.h"
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "errno.h"

namespace esphome {
namespace socket {

class LWIPRawImpl : public Socket {
 public:
  LWIPRawImpl(struct tcp_pcb *pcb) : pcb_(pcb) {}
  ~LWIPRawImpl() override {
    if (pcb_ != nullptr) {
      tcp_abort(pcb_);
      pcb_ = nullptr;
    }
  }

  void init() {
    tcp_arg(pcb_, arg);
    tcp_accept(pcb_, accept_fn);
    tcp_recv(pcb_, recv_fn);
  }

  std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) override {
    if (accepted_sockets_.empty())
      return nullptr;
    std::unique_ptr<LWIPRawImpl> sock = std::move(accepted_sockets_.front());
    accepted_sockets_.pop();
    if (addr != nullptr) {
      sock->getpeername(addr, addrlen);
    }
    return std::unique_ptr<Socket>(sock);
  }
  int bind(const struct sockaddr *name, socklen_t addrlen) {
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
    err_t err = tcp_bind(pcb_, &ip, port);
    if (err == ERR_USE) {
      errno = EADDRINUSE;
      return -1;
    }
    if (err == ERR_VAL || err != ERR_OK) {
      errno = EINVAL;
      return -1;
    }
    return 0;
  }
  int close() {
    // TODO
    return -1;
  }
  int connect(const std::string &address) {
    // TODO
    return -1;
  }
  int connect(const struct sockaddr *addr, socklen_t addrlen) {
    // TODO
    return -1;
  }
  int shutdown(int how) {
    // TODO
    return -1;
  }

  int getpeername(struct sockaddr *addr, socklen_t *addrlen) {
    // TODO
    return -1;
  }
  std::string getpeername() {
    // TODO
    return "";
  }
  int getsockname(struct sockaddr *addr, socklen_t *addrlen) {
    // TODO
    return -1;
  }
  std::string getsockname() {
    // TODO
    return "";
  }
  int getsockopt(int level, int optname, void *optval, socklen_t *optlen) {
    // TODO
    return -1;
  }
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen) {
    // TODO
    return -1;
  }
  int listen(int backlog) {
    struct tcp_pcb *listen_pcb = tcp_listen_with_backlog(pcb_, backlog);
    if (listen_pcb == nullptr) {
      tcp_abort(pcb_);
      errno = EOPNOTSUPP;
      return -1;
    }
    // tcp_listen reallocates the pcb, replace ours
    pcb_ = listen_pcb;
    return 0;
  }
  ssize_t read(void *buf, size_t len) {
    if (len == 0) {
      return 0;
    }
    if (rx_buf_ == nullptr) {
      errno = EWOULDBLOCK;
      return -1;
    }

    size_t read = 0;
    while (len) {
      size_t pb_len = rx_buf_->len;
      size_t pb_left = pb_len - rx_buf_offset_;
      if (pb_left == 0)
        break;
      size_t copysize = std::min(len, pb_left);
      memcpy(buf, rx_buf_->payload + rx_buf_offset_, copysize);

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
      tcp_recved(pcb_, copysize);

      buf += copysize;
      len -= copysize;
      read += copysize;
    }

    return read;
  }
  // virtual ssize_t readv(const struct iovec *iov, int iovcnt) = 0;
  ssize_t write(const void *buf, size_t len) {
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
    err_t err = tcp_write(pcb_, buf, to_send, TCP_WRITE_FLAG_COPY);
    if (err == ERR_MEM) {
      errno = EWOULDBLOCK;
      return -1;
    }
    if (err != ERR_OK) {
      errno = EIO;
      return -1;
    }
    err = tcp_output(pcb_);
    if (err != ERR_OK) {
      errno = EIO;
      return -1;
    }
    return to_send;
  }
  // virtual ssize_t writev(const struct iovec *iov, int iovcnt) = 0;
  int setblocking(bool blocking) {
    if (blocking) {
      // blocking operation not supported
      errno = EINVAL;
      return -1;
    }
    return 0;
  }

  err_t accept_fn(struct tcp_pcb *newpcb, err_t err) {
    // TODO: check err
    accepted_sockets_.emplace(new LWIPRawImpl(newpcb));
  }
  void err_fn(err_t err) {

  }
  err_t recv_fn(struct pbuf *pb, err_t err) {
    // TODO: check err
    if (pb == nullptr) {
      // remote host has closed the connection
      // TODO
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

  }

  static void s_err_fn(void *arg, err_t err) {

  }

  static err_t s_recv_fn(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, err_t err) {

  }

 protected:
  struct tcp_pcb *pcb_;
  std::queue<std::unique_ptr<LWIPRawImpl>> accepted_sockets_;
  pbuf *rx_buf_ = nullptr;
  size_t rx_buf_offset_ = 0;
};

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  auto *pcb = tcp_new();
  /*if (ret == nullptr)
    return nullptr;
  return std::unique_ptr<Socket>{new LWIPRawImpl(ret)};*/
  return nullptr;
}

}  // namespace socket
}  // namespace esphome

#endif // USE_SOCKET_IMPL_LWIP_TCP
