#pragma once

#include "esphome/core/defines.h"
#ifdef USE_EZSP_PROXY_TAP

#include "esphome/components/serial_proxy/serial_proxy.h"
#include "esphome/core/component.h"
#include "ash_acknowledger.h"

namespace esphome::ezsp_proxy_tap {

// Acknowledges the ASH frames of an EZSP NCP on behalf of a remote client, so the NCP's
// ack timeout is measured against this device rather than against the network round trip
// to the client. The client suppresses its own acknowledgements, making these the only
// ones the NCP sees.
//
// It never carries client traffic: the serial proxy owns the port and the bytes, and this
// component only observes them. The sole exception is the acknowledgement itself, sent
// only while a client has put the port in PROTOCOL mode, which only an EZSP client does.
class EzspProxyTap : public serial_proxy::SerialProxyTap, public Component {
 public:
  explicit EzspProxyTap(serial_proxy::SerialProxy *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

  // SerialProxyTap
  void on_device_rx(const uint8_t *data, size_t len) override;
  // ACKs depend only on what the NCP sends
  void on_client_tx(const uint8_t *data, size_t len) override {}
  // Acknowledging is only ever useful on a client's behalf, so with nobody subscribed
  // there is nothing to do and the port need not be read.
  bool tap_needs_port() const override { return false; }
  void on_protocol_enabled() override;
  // Nothing is acknowledged outside PROTOCOL mode, and entering it starts afresh
  void on_protocol_disabled() override {}
  void on_device_disconnected() override;

 protected:
  // The port this component observes. Owns the UART and the bytes; every write we make
  // goes through it.
  serial_proxy::SerialProxy *parent_;

  AshAcknowledger acknowledger_;
};

}  // namespace esphome::ezsp_proxy_tap

#endif  // USE_EZSP_PROXY_TAP
