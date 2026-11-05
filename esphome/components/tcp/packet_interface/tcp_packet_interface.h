#pragma once

#include <cstdint>
#include <span>

#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/socket/tcp_client_link.h"
#ifdef USE_SOCKET_TCP_LISTENER
#include "esphome/components/socket/tcp_listener.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::tcp {

/**
 * A packet interface link over one TCP connection, as a client that connects to a host, or a server that accepts
 * one client at a time.
 *
 * Each packet is sent as a 2 byte length, little endian, then the packet. TCP delivers the stream in order and
 * resends what is lost, and the connection joins exactly two devices, so the link provides the reliability,
 * addressing and session capabilities itself. Its connection state is the session state.
 *
 * Each packet is written to the socket as it is sent, so the link takes packets as fast as the socket does and keeps
 * the default, unlimited, send capacity; a packet is refused only while the connection is down or the socket is full.
 *
 * A zero length packet is a keep-alive, sent when nothing else has been sent for keep_alive. TCP can take minutes to
 * notice a peer that has gone without closing the connection, so the link closes it, and reconnects or accepts again,
 * when nothing has been received for the timeout, which must be longer than the peer's keep-alive interval.
 */
class TcpPacketInterface : public packet_interface::PacketInterface {
 public:
  /// @param rx_buffer Holds the packet being received; its size is the largest packet, and it must outlive the link
  explicit TcpPacketInterface(std::span<uint8_t> rx_buffer);

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  bool send_to_interface(const packet_interface::PacketBuffer &data,
                         const packet_interface::PacketMetaData &meta_data) override;

  void set_host(const char *host) { this->link_.set_host(host); }
  void set_port(uint16_t port) { this->link_.set_port(port); }
  void set_reconnect_interval(uint32_t interval_ms) { this->link_.set_reconnect_interval(interval_ms); }
  void set_keep_alive(uint32_t keep_alive_ms) { this->keep_alive_ms_ = keep_alive_ms; }
  /// The connection is closed when nothing has been received for this long; the peer's keep_alive sets the pace.
  void set_receive_timeout(uint32_t timeout_ms) { this->timeout_ms_ = timeout_ms; }
#ifdef USE_SOCKET_TCP_LISTENER
  void set_server(bool server) { this->server_ = server; }
#ifdef USE_SOCKET_IPV4_ALLOW
  void set_allow(const socket::Ipv4AllowEntry *entries, size_t count) { this->listener_.set_allow(entries, count); }
#endif
#endif
#ifdef USE_BINARY_SENSOR
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }
#endif

  bool is_connected() const { return this->connected_; }

 protected:
  void on_connection_(bool connected);
  void read_();
  bool queue_frame_(const packet_interface::PacketBuffer *data, size_t size);

  socket::TcpClientLink link_;
#ifdef USE_SOCKET_TCP_LISTENER
  socket::TcpListener listener_;
  bool server_{false};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
#endif
  const std::span<uint8_t> rx_buffer_;
  uint32_t keep_alive_ms_{10000};
  uint32_t timeout_ms_{30000};
  uint32_t last_rx_ms_{0};
  uint32_t last_tx_ms_{0};
  uint16_t rx_length_{0};
  // Bytes of the current frame received so far, counting the 2 byte length
  uint16_t rx_have_{0};
  uint8_t rx_header_[2]{};
  bool connected_{false};
};

}  // namespace esphome::tcp
