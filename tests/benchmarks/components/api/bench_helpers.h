#pragma once

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <utility>

#include "esphome/components/socket/socket.h"

namespace esphome::api::benchmarks {

// Helper to drain accumulated data from the read side of a socket
// to prevent the write side from blocking.
inline void drain_socket(int fd) {
  char buf[65536];
  while (::read(fd, buf, sizeof(buf)) > 0) {
  }
}

// Create a TCP loopback socket pair. Returns the write-side Socket
// (wrapped for ESPHome) and the raw read-side fd for draining.
// Both ends are non-blocking with 16MB buffers.
inline std::pair<std::unique_ptr<socket::Socket>, int> create_tcp_loopback() {
  // Create a TCP listener on loopback
  int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;  // OS-assigned port
  ::bind(listen_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  ::listen(listen_fd, 1);

  // Get the assigned port
  socklen_t addr_len = sizeof(addr);
  ::getsockname(listen_fd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len);

  // Connect from client side
  int write_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  ::connect(write_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));

  // Accept on server side (this is our read fd)
  int read_fd = ::accept(listen_fd, nullptr, nullptr);
  ::close(listen_fd);

  // Make both ends non-blocking
  int flags = ::fcntl(write_fd, F_GETFL, 0);
  ::fcntl(write_fd, F_SETFL, flags | O_NONBLOCK);
  flags = ::fcntl(read_fd, F_GETFL, 0);
  ::fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);

  // Use large socket buffers so benchmarks never hit WOULD_BLOCK
  // during a single outer iteration (2000 × ~15B messages = ~30KB).
  int bufsize = 16 * 1024 * 1024;
  ::setsockopt(write_fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
  ::setsockopt(read_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));

  return {std::make_unique<socket::Socket>(write_fd), read_fd};
}

}  // namespace esphome::api::benchmarks
