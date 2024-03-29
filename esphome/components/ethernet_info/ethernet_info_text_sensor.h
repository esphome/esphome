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
    auto ips = ethernet::global_eth_component->get_ip_addresses();
    if (ips != this->last_ips_) {
      this->last_ips_ = ips;
      this->publish_state(ips[0].str());
      uint8_t sensor = 0;
      for (auto &ip : ips) {
        if (ip.is_set()) {
          if (this->ip_sensors_[sensor] != nullptr) {
            this->ip_sensors_[sensor]->publish_state(ip.str());
          }
          sensor++;
        }
      }
    }
  }

  float get_setup_priority() const override { return setup_priority::ETHERNET; }
  std::string unique_id() override { return get_mac_address() + "-ethernetinfo"; }
  void dump_config() override;
  void add_ip_sensors(uint8_t index, text_sensor::TextSensor *s) { this->ip_sensors_[index] = s; }

 protected:
  network::IPAddresses last_ips_;
  std::array<text_sensor::TextSensor *, 5> ip_sensors_;
};

class DNSAddressEthernetInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    auto dns_one = ethernet::global_eth_component->get_dns_address(0);
    auto dns_two = ethernet::global_eth_component->get_dns_address(1);

    std::string dns_results = dns_one.str() + " " + dns_two.str();

    if (dns_results != this->last_results_) {
      this->last_results_ = dns_results;
      this->publish_state(dns_results);
    }
  }
  float get_setup_priority() const override { return setup_priority::ETHERNET; }
  std::string unique_id() override { return get_mac_address() + "-ethernetinfo-dns"; }
  void dump_config() override;

 protected:
  std::string last_results_;
};

}  // namespace ethernet_info
}  // namespace esphome

#endif  // USE_ESP32
