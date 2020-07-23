#pragma once

#include "esphome/core/defines.h"
#ifdef USE_TCP_UNIX_SOCKET

#include "tcp.h"
#include "ring_buffer.h"

namespace esphome {
namespace tcp {

class UnixSocketImpl : public TCPClient {
 public:
  UnixSocketImpl() = default;
  ~UnixSocketImpl() override;
  size_t available() override;
  void read(uint8_t *buffer, size_t size) override;
  void write(const uint8_t *buffer, size_t size) override;
  void flush() override;
  void close(bool force) override;
  void loop() override;
  State state() override;
  std::string get_host() override;
  bool connect(IPAddress ip, uint16_t port) override;
  size_t available_for_write() override;
  void reserve_at_least(size_t size) override;
  void ensure_capacity(size_t size) override;
  IPAddress get_remote_address() override;
  uint16_t get_remote_port() override;
  IPAddress get_local_address() override;
  uint16_t get_local_port() override;
  void set_no_delay(bool no_delay) override;
  void set_timeout(uint32_t timeout_ms) override;
  bool connect(const std::string &host, uint16_t port) override;
 protected:
  void abort_();
  void check_connecting_finished_();
  void check_disconnected_();
  void update_available_for_write_();
  /// Write data on the socket, the number of bytes written is returned.
  size_t write_internal_(const uint8_t *buffer, size_t size);
  void drain_reserve_buffer_();
  bool can_write_();
  bool can_read_();
  bool connect_(IPAddress ip, uint16_t port);

  friend class UnixServerImpl;
  explicit UnixSocketImpl(int fd);

  int fd_{-1};
  bool is_connecting_{false};
  bool is_connected_{false};
  bool has_aborted_{false};
  std::string host_;
  VectorRingBuffer<uint8_t> reserve_buffer_;
  uint32_t available_for_write_{0};
  uint32_t last_loop_{0};
  uint32_t timeout_{5000};
  uint32_t timeout_started_{0};
};

class UnixServerImpl : public TCPServer {
 public:
  bool bind(uint16_t port) override;
  std::unique_ptr<TCPSocket> accept() override;
  void close(bool force) override;
 protected:
  int fd_{-1};
};

}  // namespace tcp
}  // namespace esphome

#endif  // USE_TCP_UNIX_SOCKET
