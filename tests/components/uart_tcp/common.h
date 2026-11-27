#pragma once

#ifdef USE_HOST

#include <gtest/gtest.h>

#include <algorithm>
#include <arpa/inet.h>
#include <cstdint>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>

#include "esphome/components/uart/uart_component.h"

namespace esphome::uart_tcp::testing {

// A UART the test fills and drains directly; available_for_write() is settable.
class FakeUart : public uart::UARTComponent {
 public:
  FakeUart() { this->set_baud_rate(9600); }
  void write_array(const uint8_t *data, size_t len) override {
    this->tx.insert(this->tx.end(), data, data + len);
    this->writes.push_back(len);
  }
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty())
      return false;
    *data = this->rx.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx.size())
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }
  size_t available_for_write() override { return this->room; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

  void feed(const char *text) {
    for (const char *p = text; *p != '\0'; p++)
      this->rx.push_back(static_cast<uint8_t>(*p));
  }

  std::vector<uint8_t> rx;
  std::vector<uint8_t> tx;
  std::vector<size_t> writes;
  size_t room{SIZE_MAX};
};

/// Opens a non-blocking listener on a free loopback port for the bridge to dial; returns the port.
inline uint16_t listen_on_loopback(int &fd) {
  fd = ::socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GE(fd, 0);
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  EXPECT_EQ(::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
  EXPECT_EQ(::listen(fd, 1), 0);
  EXPECT_EQ(::fcntl(fd, F_SETFL, O_NONBLOCK), 0);
  socklen_t len = sizeof(addr);
  EXPECT_EQ(::getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr), &len), 0);
  return ntohs(addr.sin_port);
}

}  // namespace esphome::uart_tcp::testing

#endif  // USE_HOST
