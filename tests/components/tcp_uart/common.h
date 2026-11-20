#pragma once

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cstdint>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/core/application.h"
#include "esphome/core/wake.h"

#ifdef USE_HOST

// Loopback sockets and loop passes shared by the tcp_uart host tests.
namespace esphome::tcp_uart::testing {

inline struct sockaddr_in loopback(uint16_t port) {
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  return addr;
}

// Stores a loopback port that is free right now.
inline void find_free_port(uint16_t *port) {
  int probe = ::socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_GE(probe, 0);
  struct sockaddr_in addr = loopback(0);
  ASSERT_EQ(::bind(probe, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
  socklen_t len = sizeof(addr);
  ASSERT_EQ(::getsockname(probe, reinterpret_cast<struct sockaddr *>(&addr), &len), 0);
  ::close(probe);
  *port = ntohs(addr.sin_port);
}

// Stores a client socket connected to port on loopback.
inline void connect_loopback(uint16_t port, int *fd) {
  *fd = ::socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_GE(*fd, 0);
  struct sockaddr_in addr = loopback(port);
  ASSERT_EQ(::connect(*fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
}

inline void send_bytes(int fd, const void *data, size_t len) {
  ASSERT_EQ(::write(fd, data, len), static_cast<ssize_t>(len));
}

// Sets the loop clock.
inline void set_loop_clock(uint32_t ms) { LoopBlockingGuard{nullptr, nullptr, ms}; }

// One main loop pass: select() marks the ready sockets, then the UART runs.
inline void loop_pass(TcpUart &uart) {
  internal::wakeable_delay(5);
  uart.loop();
}

// Runs pass() until the UART has count bytes to read, at most 50 times.
template<typename Pass> void pass_until_available(TcpUart &uart, size_t count, Pass pass) {
  for (int i = 0; i < 50 && uart.available() < count; i++) {
    pass();
  }
}

}  // namespace esphome::tcp_uart::testing

#endif
