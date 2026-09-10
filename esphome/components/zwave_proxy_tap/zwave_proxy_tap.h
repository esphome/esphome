#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZWAVE_PROXY_TAP

#include "esphome/components/serial_proxy/serial_proxy.h"
#include "esphome/core/component.h"
#include "zwave_detector.h"

namespace esphome::zwave_proxy_tap {

// Acknowledges the frames of a Z-Wave controller on behalf of a remote client, so the
// controller's ack timeout is measured against this device rather than against the
// network round trip to the client. The client suppresses its own acknowledgements,
// making these the only ones the controller sees.
//
// This is the serial_proxy counterpart of the zwave_proxy component. Where that one owns
// the UART, parses the Serial API in full and carries frames over its own API messages,
// this one only observes: the serial proxy owns the port and the bytes, and carries them
// like those of any other serial device. The sole exception is the acknowledgement
// itself, and it is sent only once a completed request/response exchange has proven the
// port really is carrying the Serial API.
class ZWaveProxyTap : public serial_proxy::SerialProxyTap, public Component {
 public:
  explicit ZWaveProxyTap(serial_proxy::SerialProxy *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

  // SerialProxyTap
  void on_device_rx(const uint8_t *data, size_t len) override;
  void on_client_tx(const uint8_t *data, size_t len) override;
  // Acknowledging is only ever useful on a client's behalf, so with nobody subscribed
  // there is nothing to do and the port need not be read.
  bool tap_needs_port() const override { return false; }
  void on_protocol_disabled() override;
  void on_device_disconnected() override;

 protected:
  // The port this component observes. Owns the UART and the bytes; every write we make
  // goes through it.
  serial_proxy::SerialProxy *parent_;

  // Decides when acknowledging on the client's behalf is safe. Armed only by a completed
  // request/response exchange, so a bootloader or a firmware upload never triggers it.
  ZWaveDetector detector_;

  // Previous armed state, for logging the transitions
  bool was_armed_{false};
};

}  // namespace esphome::zwave_proxy_tap

#endif  // USE_ZWAVE_PROXY_TAP
