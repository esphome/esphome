#include "esphome/core/defines.h"
#include "socket.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_SOCKET_IMPL_ZEPHYR_SOCKETS

#include <errno.h>
#include <string.h>
#include <vector>

#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>

#ifdef USE_ZEPHYR_OPENTHREAD
#include "esphome/components/zephyr_openthread/dns.h"
#include <openthread/instance.h>
#include <openthread/netdata.h>
#include <openthread/border_router.h>
#include <openthread/ip6.h>
#include "esphome/components/zephyr_openthread/openthread.h"
extern otInstance *openthread_get_default_instance(void);
namespace openthread = esphome::zephyr_openthread;
#endif

// Undefine macros to avoid conflicts with method names
#undef socket
#undef connect
#undef bind
#undef listen
#undef accept
#undef send
#undef recv
#undef sendto
#undef recvfrom
#undef getsockopt
#undef setsockopt
#undef getpeername
#undef getsockname
#undef close
#undef shutdown
#undef fcntl
#undef inet_ntop
#undef inet_pton

// Define missing constants if not provided by Zephyr
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x4000
#endif

namespace esphome {
namespace socket {

// Add Socket destructor implementation
Socket::~Socket() {}

std::string format_sockaddr(const struct sockaddr_storage &storage) {
  char buf[INET6_ADDRSTRLEN];

  if (storage.ss_family == AF_INET) {
    const struct sockaddr_in *addr = reinterpret_cast<const struct sockaddr_in *>(&storage);
    zsock_inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
    return std::string(buf) + ":" + std::to_string(ntohs(addr->sin_port));
  } else if (storage.ss_family == AF_INET6) {
    const struct sockaddr_in6 *addr = reinterpret_cast<const struct sockaddr_in6 *>(&storage);
    zsock_inet_ntop(AF_INET6, &addr->sin6_addr, buf, sizeof(buf));
    return "[" + std::string(buf) + "]:" + std::to_string(ntohs(addr->sin6_port));
  }

  return "Unknown";
}

class ZephyrSocketImpl : public Socket {
 public:
  ZephyrSocketImpl(int fd) : fd_(fd) {
    // Get the socket type
    int type;
    socklen_t len = sizeof(type);
    if (zsock_getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &len) == 0) {
      socket_type_ = type;
      ESP_LOGD("socket", "Socket fd=%d has type %d", fd_, socket_type_);
    } else {
      ESP_LOGW("socket", "Failed to get socket type for fd=%d, errno: %d (%s)", fd_, errno, strerror(errno));
    }
  }

  ~ZephyrSocketImpl() override {
    if (!closed_) {
      close();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }
  }

  int connect(const struct sockaddr *addr, socklen_t addrlen) override {
    ESP_LOGD("socket", "Connecting socket fd=%d (type=%d)", fd_, socket_type_);
    return zsock_connect(fd_, addr, addrlen);
  }

  std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) override {
    if (socket_type_ != SOCK_STREAM) {
      ESP_LOGE("socket", "Cannot accept on non-stream socket fd=%d (type=%d)", fd_, socket_type_);
      return nullptr;
    }

    int fd = zsock_accept(fd_, addr, addrlen);
    if (fd == -1)
      return {};
    return make_unique<ZephyrSocketImpl>(fd);
  }

  int bind(const struct sockaddr *addr, socklen_t addrlen) override {
    ESP_LOGD("socket", "Binding socket fd=%d (type=%d)", fd_, socket_type_);
    return zsock_bind(fd_, addr, addrlen);
  }

  int close() override {
    if (closed_)
      return 0;
    closed_ = true;
    return zsock_close(fd_);
  }

  int shutdown(int how) override {
    if (socket_type_ != SOCK_STREAM) {
      ESP_LOGW("socket", "Shutdown on non-stream socket fd=%d (type=%d)", fd_, socket_type_);
    }
    return zsock_shutdown(fd_, how);
  }

  int getpeername(struct sockaddr *addr, socklen_t *addrlen) override { return zsock_getpeername(fd_, addr, addrlen); }
  std::string getpeername() override {
    struct sockaddr_storage storage;
    socklen_t len = sizeof(storage);
    int err = this->getpeername((struct sockaddr *) &storage, &len);
    if (err != 0)
      return {};
    return format_sockaddr(storage);
  }
  int getsockname(struct sockaddr *addr, socklen_t *addrlen) override { return zsock_getsockname(fd_, addr, addrlen); }
  std::string getsockname() override {
    struct sockaddr_storage storage;
    socklen_t len = sizeof(storage);
    int err = this->getsockname((struct sockaddr *) &storage, &len);
    if (err != 0)
      return {};
    return format_sockaddr(storage);
  }
  int getsockopt(int level, int optname, void *optval, socklen_t *optlen) override {
    return zsock_getsockopt(fd_, level, optname, optval, optlen);
  }
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen) override {
    return zsock_setsockopt(fd_, level, optname, optval, optlen);
  }
  int listen(int backlog) override { return zsock_listen(fd_, backlog); }
  ssize_t read(void *buf, size_t len) override {
    if (socket_type_ == SOCK_STREAM) {
      return zsock_recv(fd_, buf, len, 0);
    } else if (socket_type_ == SOCK_DGRAM) {
      struct sockaddr_storage from;
      socklen_t from_len = sizeof(from);
      return zsock_recvfrom(fd_, buf, len, 0, (struct sockaddr *) &from, &from_len);
    } else {
      ESP_LOGW("socket", "Read on socket fd=%d with unsupported type %d", fd_, socket_type_);
      return -1;
    }
  }
  ssize_t recvfrom(void *buf, size_t len, sockaddr *addr, socklen_t *addr_len) override {
    return zsock_recvfrom(fd_, buf, len, 0, addr, addr_len);
  }
  ssize_t readv(const struct iovec *iov, int iovcnt) override {
    // Zephyr doesn't have readv, so we need to implement it ourselves
    if (iovcnt <= 0)
      return 0;

    // Calculate total length
    size_t total_len = 0;
    for (int i = 0; i < iovcnt; i++) {
      total_len += iov[i].iov_len;
    }

    if (total_len == 0)
      return 0;

    // Create a temporary buffer
    std::vector<uint8_t> temp_buffer(total_len);

    // Read into the temporary buffer
    ssize_t ret = this->read(temp_buffer.data(), total_len);
    if (ret <= 0)
      return ret;

    // Copy data to the iovec buffers
    size_t copied = 0;
    for (int i = 0; i < iovcnt && copied < static_cast<size_t>(ret); i++) {
      size_t to_copy = std::min(iov[i].iov_len, static_cast<size_t>(ret) - copied);
      memcpy(iov[i].iov_base, temp_buffer.data() + copied, to_copy);
      copied += to_copy;
    }

    return ret;
  }
  ssize_t write(const void *buf, size_t len) override {
    if (socket_type_ == SOCK_STREAM) {
      return zsock_send(fd_, buf, len, 0);
    } else if (socket_type_ == SOCK_DGRAM) {
      // For datagram sockets, we need a destination address
      ESP_LOGW("socket", "Write on datagram socket fd=%d without destination address", fd_);
      return -1;
    } else {
      ESP_LOGW("socket", "Write on socket fd=%d with unsupported type %d", fd_, socket_type_);
      return -1;
    }
  }
  ssize_t writev(const struct iovec *iov, int iovcnt) override {
    // Similar to readv, implement using multiple sends or a buffer
    if (iovcnt == 1) {
      return this->write(iov[0].iov_base, iov[0].iov_len);
    }

    // For multiple buffers, we'll need to copy to a temporary buffer and then send
    size_t total_len = 0;
    for (int i = 0; i < iovcnt; i++) {
      total_len += iov[i].iov_len;
    }

    std::vector<uint8_t> temp_buffer(total_len);
    size_t offset = 0;
    for (int i = 0; i < iovcnt; i++) {
      memcpy(temp_buffer.data() + offset, iov[i].iov_base, iov[i].iov_len);
      offset += iov[i].iov_len;
    }

    return this->write(temp_buffer.data(), total_len);
  }

  ssize_t sendto(const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) override {
    ESP_LOGD("socket", "Sending to socket fd=%d (type=%d)", fd_, socket_type_);
    return zsock_sendto(fd_, buf, len, flags, to, tolen);
  }

  int setblocking(bool blocking) override {
    int flags = zsock_fcntl(fd_, F_GETFL, 0);
    if (blocking) {
      flags &= ~O_NONBLOCK;
    } else {
      flags |= O_NONBLOCK;
    }
    return zsock_fcntl(fd_, F_SETFL, flags);
  }

 protected:
  int fd_;
  bool closed_ = false;
  int socket_type_ = SOCK_STREAM;
};

std::unique_ptr<Socket> socket(int domain, int type, int protocol) {
  int ret = zsock_socket(domain, type, protocol);
  if (ret == -1)
    return nullptr;
  return std::unique_ptr<Socket>{new ZephyrSocketImpl(ret)};
}

// Implement socket_ip for Zephyr
std::unique_ptr<Socket> socket_ip(int type, int protocol) {
  std::unique_ptr<Socket> ret = nullptr;
  int sock_fd = -1;

#ifdef USE_ZEPHYR_OPENTHREAD
  // Check if OpenThread is active
  bool openthread_active =
      openthread::global_openthread_component != nullptr && openthread::global_openthread_component->is_connected();

  if (openthread_active) {
    // When OpenThread is active, only use IPv6
    ESP_LOGD("socket", "OpenThread active - creating IPv6 socket only");

#if defined(CONFIG_NET_IPV6)
    sock_fd = zsock_socket(AF_INET6, type, protocol);
    if (sock_fd >= 0) {
      ESP_LOGD("socket", "Successfully created IPv6 socket for OpenThread: fd=%d", sock_fd);
      return std::unique_ptr<Socket>{new ZephyrSocketImpl(sock_fd)};
    }
    ESP_LOGE("socket", "Failed to create IPv6 socket for OpenThread, errno: %d (%s)", errno, strerror(errno));
    return nullptr;  // Don't fall back to IPv4 when OpenThread is active
#else
    ESP_LOGE("socket", "IPv6 not supported but required for OpenThread");
    return nullptr;
#endif
  }
#endif

#if defined(CONFIG_NET_IPV6)
  // Try IPv6 first if not enforced by OpenThread
  ESP_LOGD("socket", "Creating IPv6 socket...");
  sock_fd = zsock_socket(AF_INET6, type, protocol);
  if (sock_fd >= 0) {
    ESP_LOGD("socket", "Successfully created IPv6 socket: fd=%d", sock_fd);

    // Set IPV6_V6ONLY to 0 to allow IPv4-mapped IPv6 addresses
    int off = 0;
    if (zsock_setsockopt(sock_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) < 0) {
      ESP_LOGW("socket", "Failed to set IPV6_V6ONLY=0, errno: %d (%s)", errno, strerror(errno));
      // Continue anyway, as this is not critical
    } else {
      ESP_LOGD("socket", "Set IPV6_V6ONLY=0 to allow IPv4-mapped addresses");
    }

    return std::unique_ptr<Socket>{new ZephyrSocketImpl(sock_fd)};
  }
  ESP_LOGW("socket", "Failed to create IPv6 socket, errno: %d (%s)", errno, strerror(errno));
#endif

#if defined(CONFIG_NET_IPV4)
  // Only try IPv4 if IPv6 failed and IPv4 is enabled
  ESP_LOGD("socket", "Trying to create IPv4 socket...");
  sock_fd = zsock_socket(AF_INET, type, protocol);
  if (sock_fd >= 0) {
    ESP_LOGD("socket", "Successfully created IPv4 socket: fd=%d", sock_fd);
    return std::unique_ptr<Socket>{new ZephyrSocketImpl(sock_fd)};
  }
  ESP_LOGW("socket", "Failed to create IPv4 socket, errno: %d (%s)", errno, strerror(errno));
#endif

  ESP_LOGE("socket", "Failed to create any socket");
  return nullptr;
}

// Update the set_sockaddr function to use DNS resolution for hostnames
socklen_t set_sockaddr(struct sockaddr *addr, socklen_t addrlen, const std::string &ip_address, uint16_t port) {
  // Check if this is an IPv6 address (contains ':')
  bool is_ipv6 = ip_address.find(':') != std::string::npos;

  // Check if this is an IP address or hostname
  bool is_ip_address = false;

  // Simple check for IPv4 (x.x.x.x format)
  if (!is_ipv6) {
    int dots = 0;
    bool valid = true;

    for (char c : ip_address) {
      if (c == '.') {
        dots++;
      } else if (!isdigit(c)) {
        valid = false;
        break;
      }
    }

    is_ip_address = valid && (dots == 3);
  } else {
    // Simple check for IPv6 (contains ':')
    is_ip_address = true;
  }

  // If this is a hostname, resolve it
  if (!is_ip_address) {
    ESP_LOGD("socket", "Resolving hostname: %s", ip_address.c_str());

#ifdef USE_ZEPHYR_OPENTHREAD
    struct sockaddr_storage resolved_addr;

    if (esphome::zephyr_openthread::resolve_hostname(ip_address, &resolved_addr, port)) {
      // Copy the resolved address to the output
      if (resolved_addr.ss_family == AF_INET) {
        if (addrlen < sizeof(struct sockaddr_in)) {
          ESP_LOGE("socket", "Buffer too small for IPv4 address: %u < %u", addrlen, sizeof(struct sockaddr_in));
          return 0;
        }
        memcpy(addr, &resolved_addr, sizeof(struct sockaddr_in));
        return sizeof(struct sockaddr_in);
      } else if (resolved_addr.ss_family == AF_INET6) {
        if (addrlen < sizeof(struct sockaddr_in6)) {
          ESP_LOGE("socket", "Buffer too small for IPv6 address: %u < %u", addrlen, sizeof(struct sockaddr_in6));
          return 0;
        }
        memcpy(addr, &resolved_addr, sizeof(struct sockaddr_in6));
        return sizeof(struct sockaddr_in6);
      }
    } else {
      ESP_LOGE("socket", "Failed to resolve hostname: %s", ip_address.c_str());
      return 0;
    }
#else
    ESP_LOGE("socket", "DNS resolution not available, cannot resolve hostname: %s", ip_address.c_str());
    return 0;
#endif
  }

  if (is_ipv6) {
    // Handle IPv6 address
    if (addrlen < sizeof(struct sockaddr_in6)) {
      ESP_LOGE("socket", "Buffer too small for IPv6 address: %u < %u", addrlen, sizeof(struct sockaddr_in6));
      return 0;
    }

    struct sockaddr_in6 *addr_in6 = reinterpret_cast<struct sockaddr_in6 *>(addr);
    memset(addr_in6, 0, sizeof(*addr_in6));
    addr_in6->sin6_family = AF_INET6;
    addr_in6->sin6_port = htons(port);

    // Convert string to IPv6 address
    if (zsock_inet_pton(AF_INET6, ip_address.c_str(), &addr_in6->sin6_addr) != 1) {
      ESP_LOGE("socket", "Failed to parse IPv6 address: %s", ip_address.c_str());
      return 0;
    }

    ESP_LOGD("socket", "Successfully converted IPv6 address: %s", ip_address.c_str());
    return sizeof(struct sockaddr_in6);
  } else {
    // Handle IPv4 address
    if (addrlen < sizeof(struct sockaddr_in)) {
      ESP_LOGE("socket", "Buffer too small for IPv4 address: %u < %u", addrlen, sizeof(struct sockaddr_in));
      return 0;
    }

    struct sockaddr_in *addr_in = reinterpret_cast<struct sockaddr_in *>(addr);
    memset(addr_in, 0, sizeof(*addr_in));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(port);

    // Convert string to IPv4 address
    if (zsock_inet_pton(AF_INET, ip_address.c_str(), &addr_in->sin_addr) != 1) {
      ESP_LOGE("socket", "Failed to parse IPv4 address: %s", ip_address.c_str());
      return 0;
    }

    ESP_LOGD("socket", "Successfully converted IPv4 address: %s", ip_address.c_str());
    return sizeof(struct sockaddr_in);
  }
}

// Implement set_sockaddr_any for Zephyr
socklen_t set_sockaddr_any(struct sockaddr *addr, socklen_t addrlen, uint16_t port) {
  struct sockaddr_in *addr_in = reinterpret_cast<struct sockaddr_in *>(addr);
  memset(addr_in, 0, sizeof(*addr_in));
  addr_in->sin_family = AF_INET;
  addr_in->sin_port = htons(port);
  addr_in->sin_addr.s_addr = INADDR_ANY;
  return sizeof(*addr_in);
}

}  // namespace socket
}  // namespace esphome

#endif  // USE_SOCKET_IMPL_ZEPHYR_SOCKETS
