#pragma once
#include <memory>
#include <string>

#include "esphome/core/optional.h"
#include "esphome/components/socket/headers.h"

namespace esphome {
namespace socket_shd {

class Socket {
 public:
  Socket() = default;
  virtual ~Socket();
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  virtual std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) = 0;
  virtual int bind(const struct sockaddr *addr, socklen_t addrlen) = 0;
  virtual int close() = 0;
  // not supported yet:
  // virtual int connect(const std::string &address) = 0;
  // virtual int connect(const struct sockaddr *addr, socklen_t addrlen) = 0;
  virtual int shutdown(int how) = 0;

  virtual int getpeername(struct sockaddr *addr, socklen_t *addrlen) = 0;
  virtual std::string getpeername() = 0;
  virtual int getsockname(struct sockaddr *addr, socklen_t *addrlen) = 0;
  virtual std::string getsockname() = 0;
  virtual int getsockopt(int level, int optname, void *optval, socklen_t *optlen) = 0;
  virtual int setsockopt(int level, int optname, const void *optval, socklen_t optlen) = 0;
  virtual int listen(int backlog) = 0;
  virtual ssize_t read(void *buf, size_t len) = 0;
  virtual ssize_t readv(const struct iovec *iov, int iovcnt) = 0;
  virtual ssize_t recvfrom(void *buf, size_t len, int flags, struct sockaddr *src, socklen_t *srclen) = 0;
  virtual ssize_t recvfrom(void *buf, size_t len, int flags, std::string &addr) = 0;
  virtual ssize_t write(const void *buf, size_t len) = 0;
  virtual ssize_t writev(const struct iovec *iov, int iovcnt) = 0;
  virtual ssize_t sendto(const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) = 0;

  virtual int setblocking(bool blocking) = 0;
  virtual int loop() { return 0; };
};

/// Create a socket of the given domain, type and protocol.
std::unique_ptr<Socket> socket(int domain, int type, int protocol);

/// Create a socket in the newest available IP domain (IPv6 or IPv4) of the given type and protocol.
std::unique_ptr<Socket> socket_ip(int type, int protocol);

}  // namespace socket_shd
}  // namespace esphome
