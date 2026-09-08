#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZIGBEE_PROXY

#include "esphome/components/serial_proxy/serial_proxy.h"
#include "esphome/core/component.h"
#include "ash_detector.h"

namespace esphome::zigbee_proxy {

// Acknowledges the ASH frames of an EZSP NCP on behalf of a remote client, so the NCP's
// ack timeout is measured against this device rather than against the network round trip
// to the client. The client suppresses its own acknowledgements, making these the only
// ones the NCP sees.
//
// It never carries client traffic: the serial proxy owns the port and the bytes, and this
// component only observes them. The sole exception is the acknowledgement itself, and it
// is sent only once the handshake has proven the port really is carrying ASH.
class ZigbeeProxy : public serial_proxy::SerialProxyTap, public Component {
 public:
  explicit ZigbeeProxy(serial_proxy::SerialProxy *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

  // SerialProxyTap
  void on_device_rx(const uint8_t *data, size_t len) override;
  void on_client_tx(const uint8_t *data, size_t len) override;
  // Acknowledging is only ever useful on a client's behalf, so with nobody subscribed
  // there is nothing to do and the port need not be read.
  bool tap_needs_port() const override { return false; }
  void on_protocol_disabled() override;

 protected:
  // The port this component observes. Owns the UART and the bytes; every write we make
  // goes through it.
  serial_proxy::SerialProxy *parent_;

  // Decides when acknowledging on the client's behalf is safe. Armed only by the ASH
  // session handshake, so a bootloader or Thread NCP never triggers it.
  AshDetector detector_;

  // Previous armed state, for logging the transitions
  bool was_armed_{false};
};

}  // namespace esphome::zigbee_proxy

#endif  // USE_ZIGBEE_PROXY
