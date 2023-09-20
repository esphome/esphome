#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/ethernet/ethernet_component.h"

#ifdef USE_ESP32

namespace esphome {
namespace ethernet_info {

class IPAddressEthernetInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    std::string address_results;
    auto ips = ethernet::global_eth_component->get_ip_address();
    if (ips != this->last_ips_) {
      this->last_ips_ = ips;
      for (auto& ip: ips) {
        if (ip.is_set()) {
          address_results += ip.is_ip4() ? "IPv4: " : "IPv6: ";
          address_results += ip.str();
          address_results += "\n";
        }
      }
      this->publish_state(address_results);
    }
  }

  float get_setup_priority() const override { return setup_priority::ETHERNET; }
  std::string unique_id() override { return get_mac_address() + "-ethernetinfo"; }
  void dump_config() override;

 protected:
  network::IPAddresses last_ips_;
};

}  // namespace ethernet_info
}  // namespace esphome

#endif  // USE_ESP32
