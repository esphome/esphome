#pragma once

#include "esphome/core/defines.h"
#ifdef USE_TCP_LWIP_RAW_TCP

#include "tcp.h"
#include "esphome/core/optional.h"
#include "ring_buffer.h"
#include "lwip/err.h"
#include <queue>

extern "C" {
  #include "include/wl_definitions.h"

  struct tcp_pcb;
}

namespace esphome {
namespace tcp {

class LWIPRawTCPImpl : public TCPClient {
 public:
  LWIPRawTCPImpl() = default;
  ~LWIPRawTCPImpl() override;
  bool connect(IPAddress ip, uint16_t port) override;
  void abort();
  void close(bool force) override;
  void set_no_delay(bool no_delay) override;
  IPAddress get_remote_address() override;
  uint16_t get_remote_port() override;
  IPAddress get_local_address() override;
  uint16_t get_local_port() override;

  size_t available() override;
  void read(uint8_t *destination_buffer, size_t size) override;
  size_t sendbuf_size();
  size_t available_for_write() override;
  void write(const uint8_t *buffer, size_t size) override;

  void skip(size_t size) override;
  void flush() override;
  bool connect(const std::string &host, uint16_t port) override;
  void set_timeout(uint32_t timeout_ms) override;
  void reserve_at_least(size_t size) override;
  void ensure_capacity(size_t size) override;
  void loop() override;
  State state() override;
  std::string get_host() override;

 protected:
  void setup_callbacks_();
  void remove_callbacks_();
  void write_internal_(const uint8_t *buffer, size_t size, bool has_more);
  void drain_reserve_buffer_();
  bool connect_(IPAddress address, uint16_t port);

  // Callbacks:
  err_t on_tcp_recv_(tcp_pcb *pcb, pbuf *packet_buffer, err_t err);
  err_t on_tcp_sent_(tcp_pcb *pcb, uint16_t len);
  void on_tcp_err_(err_t err);
  err_t on_tcp_connected_(tcp_pcb *pcb, err_t err);
  void on_dns_found_(const char *name, const ip_addr_t *ipaddr);

  // Static methods for  callbacks:
  static err_t on_tcp_recv_static(void *arg, tcp_pcb *pcb, pbuf *pb, err_t err) {
    return reinterpret_cast<LWIPRawTCPImpl *>(arg)->on_tcp_recv_(pcb, pb, err);
  }
  static err_t on_tcp_sent_static(void *arg, tcp_pcb *pcb, uint16_t len) {
    return reinterpret_cast<LWIPRawTCPImpl *>(arg)->on_tcp_sent_(pcb, len);
  }
  static void on_tcp_err_static(void *arg, err_t err) {
    reinterpret_cast<LWIPRawTCPImpl *>(arg)->on_tcp_err_(err);
  }
  static err_t on_tcp_connected_static(void *arg, tcp_pcb *pcb, err_t err) {
    return reinterpret_cast<LWIPRawTCPImpl *>(arg)->on_tcp_connected_(pcb, err);
  }
  static void on_dns_found_static_(const char *name, const ip_addr_t *ipaddr, void *arg) {
    reinterpret_cast<LWIPRawTCPImpl *>(arg)->on_dns_found_(name, ipaddr);
  }

  friend class LWIPRawTCPServerImpl;
  LWIPRawTCPImpl(tcp_pcb *pcb);

  tcp_pcb *pcb_{nullptr};
  pbuf *rx_buffer_{nullptr};
  size_t rx_buffer_offset_{0};
  /// If this is true, the PCB may be deallocated
  bool aborted_{false};

  bool initialized_{true};
  bool pending_connect_{false};
  bool pending_error_{false};

  bool dns_resolved_success_{false};
  bool dns_resolved_error_{false};
  bool pending_dns_result_{false};

  uint32_t connect_started_{0};
  std::string host_;
  VectorRingBuffer<uint8_t> reserve_buffer_;
  IPAddress dns_callback_result_;
  uint16_t port_;  /// Port stored for DNS callback
};

class LWIPRawTCPServerImpl : public TCPServer {
 public:
  bool bind(uint16_t port) override;
  std::unique_ptr<TCPSocket> accept() override;
  void close(bool force) override;
 protected:
  err_t on_tcp_accept_(tcp_pcb *new_pcb, err_t err);
  static err_t on_tcp_accept_static(void *arg, tcp_pcb *new_pcb, err_t err) {
    return reinterpret_cast<LWIPRawTCPServerImpl *>(arg)->on_tcp_accept_(new_pcb, err);
  }

  std::queue<LWIPRawTCPImpl *> accepted_;
  tcp_pcb *pcb_{nullptr};
};

}  // namespace tcp
}  // namespace esphome

#endif  // USE_TCP_LWIP_RAW_TCP
