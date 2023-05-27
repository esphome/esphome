#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/wireguard/wireguard.h"

namespace esphome {
namespace wireguard_status {

/// Binary sensor to report if the remote WireGuard peer is up.
class WireguardStatus : public binary_sensor::BinarySensor, public PollingComponent {
 public:
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void set_wireguard(wireguard::Wireguard *wireguard) { this->wireguard_ = wireguard; }

 protected:
  /// Pointer to a configured wireguard::Wireguard component.
  wireguard::Wireguard *wireguard_ = nullptr;
};

}  // namespace wireguard_status
}  // namespace esphome

// vim: tabstop=2 shiftwidth=2 expandtab
