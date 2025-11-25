#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"

#include "esp_netif.h"
#include <string>

namespace esphome {
namespace usb_ethernet {

extern const char *const TAG;

class USBEthernetComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // “Network backend” interface, modeled after EthernetComponent
  bool is_connected();

  /// Return up to 5 IP addresses (same alias as Ethernet uses: std::array<IPAddress, 5>)
  network::IPAddresses get_ip_addresses() const;

  /// Preferred address/hostname (used by API/mDNS). For now we’ll use "<node_name>.local".
  const char *get_use_address() const;

  /// Allow overriding the hostname/address string from elsewhere if needed.
  void set_use_address(const char *use_address);

  // Called from the low-level bridge / event handler
  void set_link_up(bool up) { this->link_up_ = up; }
  void set_has_ip(bool has_ip);
  void set_primary_ip(const network::IPAddress &ip);

 protected:
  bool link_up_{false};
  bool has_ip_{false};
  bool connected_{false};

  network::IPAddresses ip_addresses_{};  // all zero-initialized
  std::string use_address_;
};

// Global pointer set from C++ setup() so network::util can see us
extern USBEthernetComponent *global_usb_eth_component;

}  // namespace usb_ethernet
}  // namespace esphome
