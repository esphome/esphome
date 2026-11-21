#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZWAVE_PROXY_TAP

#include "esphome/components/serial_proxy/serial_proxy.h"
#include "esphome/core/component.h"
#include "zwave_frame_scanner.h"

namespace esphome::zwave_proxy_tap {

// ACKs a Z-Wave controller's data frames on behalf of a remote client, so the controller's
// ACK timeout does not include the network round trip. The client sends only its NAKs.
// ACKs are sent only while a client has put the port in PROTOCOL mode.
class ZWaveProxyTap : public serial_proxy::SerialProxyTap, public Component {
 public:
  explicit ZWaveProxyTap(serial_proxy::SerialProxy *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;

  void on_device_rx(const uint8_t *data, size_t len) override;
  void on_client_tx(const uint8_t *data, size_t len) override {}
  bool tap_needs_port() const override { return false; }
  void on_protocol_enabled() override;
  void on_protocol_disabled() override {}
  void on_device_disconnected() override;

 protected:
  serial_proxy::SerialProxy *parent_;

  ZWaveFrameScanner scanner_;
};

}  // namespace esphome::zwave_proxy_tap

#endif  // USE_ZWAVE_PROXY_TAP
