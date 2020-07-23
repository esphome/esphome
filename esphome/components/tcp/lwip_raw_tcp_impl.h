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

/** An implementation of our TCPClient using raw LWIP calls.
 *
 * See also https://www.nongnu.org/lwip/2_0_x/tcp_8c.html and https://www.nongnu.org/lwip/2_0_x/raw_api.html
 */
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
  bool read(uint8_t *destination_buffer, size_t size) override;
  size_t sendbuf_size();
  size_t available_for_write() override;
  bool write(const uint8_t *buffer, size_t size) override;

  bool flush() override;
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
  size_t write_internal_(const uint8_t *buffer, size_t size, bool psh_flag);
  bool drain_reserve_buffer_();
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

  /// The lwip raw protocol control block
  tcp_pcb *pcb_{nullptr};
  /// Packet buffer containing all packets that we received but the user
  /// has not processed yet.
  pbuf *rx_buffer_{nullptr};
  /// Offset in the first rx_buffer_ segment where we're currently at.
  size_t rx_buffer_offset_{0};
  /// If this is true, the PCB may be deallocated
  bool aborted_{false};

  /// If the pcb has been initialized yet
  bool initialized_{true};
  /// If a connect() call result is pending.
  bool pending_connect_{false};
  /// If an error occured and is pending to notify the user
  bool pending_error_{false};

  /// Value set by the DNS callback when DNS finished successfully.
  bool dns_resolved_success_{false};
  /// Value set by the DNS callback when the DNS lookup failed.
  bool dns_resolved_error_{false};
  /// If we're still waiting for the DNS callback.
  bool pending_dns_result_{false};

  /// The host we're connecting to.
  std::string host_;
  /// A buffer in which we store TX data that didn't fit in lwIP's buffers.
  RingBuffer reserve_buffer_;
  /// The result of the DNS callback
  IPAddress dns_callback_result_;
  /// Port stored for DNS callback
  uint16_t port_;
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

  /// A queue of accepted TCP client connections waiting to be processed by the user.
  std::queue<LWIPRawTCPImpl *> accepted_;
  tcp_pcb *pcb_{nullptr};
};

}  // namespace tcp
}  // namespace esphome

#endif  // USE_TCP_LWIP_RAW_TCP
