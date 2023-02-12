#include "socket.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_SOCKET_IMPL_BSD_SOCKETS

#include <cstring>

#ifdef USE_ESP32
#include <esp_idf_version.h>
#include <lwip/sockets.h>
#endif

namespace esphome {
namespace socket {

std::string format_sockaddr(const struct sockaddr_storage &storage) {
  if (storage.ss_family == AF_INET) {
    const struct sockaddr_in *addr = reinterpret_cast<const struct sockaddr_in *>(&storage);
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf)) != nullptr)
      return std::string{buf};
  } else if (storage.ss_family == AF_INET6) {
    const struct sockaddr_in6 *addr = reinterpret_cast<const struct sockaddr_in6 *>(&storage);
    char buf[INET6_ADDRSTRLEN];
    // Format IPv4-mapped IPv6 addresses as regular IPv4 addresses
    if (addr->sin6_addr.un.u32_addr[0] == 0 && addr->sin6_addr.un.u32_addr[1] == 0 &&
        addr->sin6_addr.un.u32_addr[2] == htonl(0xFFFF) &&
        inet_ntop(AF_INET, &addr->sin6_addr.un.u32_addr[3], buf, sizeof(buf)) != nullptr) {
      return std::string{buf};
    }
    if (inet_ntop(AF_INET6, &addr->sin6_addr, buf, sizeof(buf)) != nullptr)
      return std::string{buf};
  }
  return {};
}

class BSDSocketImpl : public Socket {
 public:
  BSDSocketImpl(int fd) : fd_(fd) {}
  ~BSDSocketImpl() override {
    if (!closed_) {
      close();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }
  }
  std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) override {
    int fd = ::accept(fd_, addr, addrlen);
    if (fd == -1)
      return {};
    return make_unique<BSDSocketImpl>(fd);
  }
  int bind(const struct sockaddr *addr, socklen_t addrlen) override { return ::bind(fd_, addr, addrlen); }
  int close() override {
    int ret = ::close(fd_);
    closed_ = true;
    return ret;
  }
  int shutdown(int how) override { return ::shutdown(fd_, how); }

  int getpeername(struct sockaddr *addr, socklen_t *addrlen) override { return ::getpeername(fd_, addr, addrlen); }
  std::string getpeername() override {
    struct sockaddr_storage storage;
    socklen_t len = sizeof(storage);
    int err = this->getpeername((struct sockaddr *) &storage, &len);
    if (err != 0)
      return {};
    return format_sockaddr(storage);
  }
  int getsockname(struct sockaddr *addr, socklen_t *addrlen) override { return ::getsockname(fd_, addr, addrlen); }
  std::string getsockname() override {
    struct sockaddr_storage storage;
    socklen_t len = sizeof(storage);
    int err = this->getsockname((struct sockaddr *) &storage, &len);
    if (err != 0)
      return {};
    return format_sockaddr(storage);
  }
  int getsockopt(int level, int optname, void *optval, socklen_t *optlen) override {
    return ::getsockopt(fd_, level, optname, optval, optlen);
  }
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen) override {
    return ::setsockopt(fd_, level, optname, optval, optlen);
  }
  int listen(int backlog) override { return ::listen(fd_, backlog); }
  ssize_t read(void *buf, size_t len) override { return ::read(fd_, buf, len); }
  ssize_t readv(const struct iovec *iov, int iovcnt) override {
#if defined(USE_ESP32) && ESP_IDF_VERSION_MAJOR < 4
    // esp-idf v3 doesn't have readv, emulate it
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
      if (err != iov[i].iov_len)
        break;
    }
    return ret;
#elif defined(USE_ESP32)
    // ESP-IDF v4 only has symbol lwip_readv
    return ::lwip_readv(fd_, iov, iovcnt);
#else
    return ::readv(fd_, iov, iovcnt);
#endif
  }
  ssize_t write(const void *buf, size_t len) override { return ::write(fd_, buf, len); }
  ssize_t send(void *buf, size_t len, int flags) { return ::send(fd_, buf, len, flags); }
  ssize_t writev(const struct iovec *iov, int iovcnt) override {
#if defined(USE_ESP32) && ESP_IDF_VERSION_MAJOR < 4
    // esp-idf v3 doesn't have writev, emulate it
    ssize_t ret = 0;
    for (int i = 0; i < iovcnt; i++) {
      ssize_t err =
          this->send(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len, i == iovcnt - 1 ? 0 : MSG_MORE);
      if (err == -1) {
        if (ret != 0) {
          // if we already wrote some don't return an error
          break;
        }
        return err;
      }
      ret += err;
      if (err != iov[i].iov_len)
        break;
    }
    return ret;
#elif defined(USE_ESP32)
    // ESP-IDF v4 only has symbol lwip_writev
    return ::lwip_writev(fd_, iov, iovcnt);
#else
    return ::writev(fd_, iov, iovcnt);
#endif
  }
  int setblocking(bool blocking) override {
    int fl = ::fcntl(fd_, F_GETFL, 0);
    if (blocking) {
      fl &= ~O_NONBLOCK;
    } else {
      fl |= O_NONBLOCK;
    }
    ::fcntl(fd_, F_SETFL, fl);
    return 0;
  }

 protected:
  int fd_;
  bool closed_ = false;
};

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  int ret = ::socket(domain, type, protocol);
  if (ret == -1)
    return nullptr;
  return std::unique_ptr<Socket>{new BSDSocketImpl(ret)};
}

}  // namespace socket
}  // namespace esphome

#endif  // USE_SOCKET_IMPL_BSD_SOCKETS
