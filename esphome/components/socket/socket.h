#pragma once
#include <memory>
#include <string>

#include "esphome/core/optional.h"
#include "headers.h"

#if defined(USE_SOCKET_IMPL_LWIP_TCP) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)
namespace esphome {
namespace socket {

class Socket {
 public:
  Socket() = default;
  virtual ~Socket();
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  virtual std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) = 0;
  /// Accept a connection and monitor it in the main loop
  /// NOTE: This function is NOT thread-safe and must only be called from the main loop
  virtual std::unique_ptr<Socket> accept_loop_monitored(struct sockaddr *addr, socklen_t *addrlen) {
    return accept(addr, addrlen);  // Default implementation for backward compatibility
  }
  virtual int bind(const struct sockaddr *addr, socklen_t addrlen) = 0;
  virtual int close() = 0;
  // not supported yet:
  // virtual int connect(const std::string &address) = 0;
#if defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS)
  virtual int connect(const struct sockaddr *addr, socklen_t addrlen) = 0;
#endif
  virtual int shutdown(int how) = 0;

  virtual int getpeername(struct sockaddr *addr, socklen_t *addrlen) = 0;
  virtual std::string getpeername() = 0;
  virtual int getsockname(struct sockaddr *addr, socklen_t *addrlen) = 0;
  virtual std::string getsockname() = 0;
  virtual int getsockopt(int level, int optname, void *optval, socklen_t *optlen) = 0;
  virtual int setsockopt(int level, int optname, const void *optval, socklen_t optlen) = 0;
  virtual int listen(int backlog) = 0;
  virtual ssize_t read(void *buf, size_t len) = 0;
  virtual ssize_t recvfrom(void *buf, size_t len, sockaddr *addr, socklen_t *addr_len) = 0;
  virtual ssize_t readv(const struct iovec *iov, int iovcnt) = 0;
  virtual ssize_t write(const void *buf, size_t len) = 0;
  virtual ssize_t writev(const struct iovec *iov, int iovcnt) = 0;
  virtual ssize_t sendto(const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) = 0;

  virtual int setblocking(bool blocking) = 0;
  virtual int loop() { return 0; };

  /// Get the underlying file descriptor (returns -1 if not supported)
  virtual int get_fd() const { return -1; }

  /// Check if socket has data ready to read
  /// For loop-monitored sockets, checks with the Application's select() results
  /// For non-monitored sockets, always returns true (assumes data may be available)
  bool ready() const;

 protected:
#ifdef USE_SOCKET_SELECT_SUPPORT
  bool loop_monitored_{false};  ///< Whether this socket is monitored by the event loop
#endif
};

/// Create a socket of the given domain, type and protocol.
std::unique_ptr<Socket> socket(int domain, int type, int protocol);
/// Create a socket in the newest available IP domain (IPv6 or IPv4) of the given type and protocol.
std::unique_ptr<Socket> socket_ip(int type, int protocol);

/// Create a socket and monitor it for data in the main loop.
/// Like socket() but also registers the socket with the Application's select() loop.
/// WARNING: These functions are NOT thread-safe. They must only be called from the main loop
/// as they register the socket file descriptor with the global Application instance.
/// NOTE: On ESP platforms, FD_SETSIZE is typically 10, limiting the number of monitored sockets.
/// File descriptors >= FD_SETSIZE will not be monitored and will log an error.
std::unique_ptr<Socket> socket_loop_monitored(int domain, int type, int protocol);
std::unique_ptr<Socket> socket_ip_loop_monitored(int type, int protocol);

/// Set a sockaddr to the specified address and port for the IP version used by socket_ip().
socklen_t set_sockaddr(struct sockaddr *addr, socklen_t addrlen, const std::string &ip_address, uint16_t port);

/// Set a sockaddr to the any address and specified port for the IP version used by socket_ip().
socklen_t set_sockaddr_any(struct sockaddr *addr, socklen_t addrlen, uint16_t port);

#if defined(USE_ESP8266) && defined(USE_SOCKET_IMPL_LWIP_TCP)
/// Delay that can be woken early by socket activity.
/// On ESP8266, lwip callbacks set a flag and call esp_schedule() to wake the delay.
void socket_delay(uint32_t ms);

/// Called by lwip callbacks to signal socket activity and wake delay.
void socket_wake();
#endif

}  // namespace socket
}  // namespace esphome
#endif
