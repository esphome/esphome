#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/ethernet/ethernet_component.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

namespace esphome {
namespace ethernet_info {

class IPAddressEthernetInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    auto ip = ethernet::global_eth_component->get_ip_address();
    if (ip != this->last_ip_) {
      this->last_ip_ = ip;
      this->publish_state(network::IPAddress(ip).str());
    }
  }

  float get_setup_priority() const override { return setup_priority::ETHERNET; }
  std::string unique_id() override { return get_mac_address() + "-ethernetinfo"; }
  void dump_config() override;

 protected:
  network::IPAddress last_ip_;
};

}  // namespace ethernet_info
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
