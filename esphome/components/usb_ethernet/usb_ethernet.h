#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"

#include "esp_netif.h"
#include <string>
#include <array>

namespace esphome {
namespace usb_ethernet {

extern const char *const TAG;

struct ManualIP {
  network::IPAddress static_ip;
  network::IPAddress gateway;
  network::IPAddress subnet;
  network::IPAddress dns1;  ///< The first DNS server. 0.0.0.0 for default.
  network::IPAddress dns2;  ///< The second DNS server. 0.0.0.0 for default.
};

class USBEthernetComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  // "Network backend" interface, modeled after EthernetComponent
  bool is_connected();

  /// Return up to 5 IP addresses (same alias as Ethernet uses: std::array<IPAddress, 5>)
  network::IPAddresses get_ip_addresses() const;

  /// Preferred address/hostname (used by API/mDNS)
  const char *get_use_address() const;

  /// Allow overriding the hostname/address string
  void set_use_address(const char *use_address);

  /// Set manual IP configuration
  void set_manual_ip(const ManualIP &manual_ip) { this->manual_ip_ = manual_ip; }

  /// Set fixed MAC address
  void set_fixed_mac(const std::array<uint8_t, 6> &mac) { this->fixed_mac_ = mac; }

  /// Check if manual IP is configured
  bool has_manual_ip() const { return this->manual_ip_.has_value(); }

  // Called from the low-level bridge / event handler
  void set_link_up(bool up) { this->link_up_ = up; }
  void set_has_ip(bool has_ip);
  void set_primary_ip(const network::IPAddress &ip);
  
  /// Apply manual IP configuration (called from event handler)
  void apply_manual_ip();

 protected:
  bool link_up_{false};
  bool has_ip_{false};
  bool connected_{false};

  network::IPAddresses ip_addresses_{};  // all zero-initialized
  std::string use_address_;
  optional<ManualIP> manual_ip_{};
  optional<std::array<uint8_t, 6>> fixed_mac_{};
};

// Global pointer set from C++ setup() so network::util can see us
extern USBEthernetComponent *global_usb_eth_component;

}  // namespace usb_ethernet
}  // namespace esphome
