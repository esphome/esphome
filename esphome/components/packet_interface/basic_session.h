#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PACKET_INTERFACE_BASIC_SESSION
#include "packet_interface.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::packet_interface {

/**
 * A session layer that reports whether the peer can be reached.
 *
 * Every packet starts with a type byte. A data packet carries the payload of the layer above. A keep-alive packet
 * carries a random number chosen at boot, so that a restart of the peer is noticed even if it is quicker than the
 * timeout. A keep-alive is sent every keep_alive interval, and at once when a new peer is heard. The peer is
 * considered gone when nothing has been received from it for the timeout; this is checked every keep_alive interval.
 * A restart of the peer is reported as a disconnect followed by a connect. So is a reset of a reliability layer below,
 * which discards data: the session is reported lost, and connected again when the peer is next heard.
 */
class BasicSession : public PacketInterface {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(connected)
#endif

 public:
  /// The packet type byte.
  static constexpr size_t HEADER_SIZE = 1;

  explicit BasicSession(PacketInterface *lower);

  void setup() override;
  void dump_config() override;
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) override;

  void set_keep_alive(uint32_t keep_alive_ms) { this->keep_alive_ms_ = keep_alive_ms; }
  void set_session_timeout(uint32_t timeout_ms) { this->timeout_ms_ = timeout_ms; }

  bool is_connected() const { return this->connected_; }

 protected:
  void receive_(const PacketBuffer &data, const PacketMetaData &meta_data);
  void on_interval_();
  void send_keep_alive_();
  void set_connected_(bool connected);

  PacketInterface *lower_;
  uint32_t keep_alive_ms_{10000};
  uint32_t timeout_ms_{30000};
  uint32_t last_receive_ms_{0};
  uint32_t boot_id_{0};
  uint32_t peer_boot_id_{0};
  bool peer_known_{false};
  bool connected_{false};
};

}  // namespace esphome::packet_interface
#endif
