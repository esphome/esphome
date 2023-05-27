#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/wireguard/wireguard.h"

namespace esphome {
namespace wireguard_handshake {

/// Track the timestamp of the latest completed handshake.
class WireguardHandshake : public sensor::Sensor, public PollingComponent {
 public:
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void set_wireguard(wireguard::Wireguard *wireguard) { this->wireguard_ = wireguard; }

 protected:
  /// Pointer to a configured wireguard::Wireguard component.
  wireguard::Wireguard *wireguard_ = nullptr;
};

}  // namespace wireguard_handshake
}  // namespace esphome

// vim: tabstop=2 shiftwidth=2 expandtab
