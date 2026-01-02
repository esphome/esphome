#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/ethernet/ethernet_component.h"

#ifdef USE_ESP32

namespace esphome::ethernet_info {

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
  void dump_config() override;
  void add_ip_sensors(uint8_t index, text_sensor::TextSensor *s) { this->ip_sensors_[index] = s; }

 protected:
  network::IPAddresses last_ips_;
  std::array<text_sensor::TextSensor *, 5> ip_sensors_;
};

class DNSAddressEthernetInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    auto dns1 = ethernet::global_eth_component->get_dns_address(0);
    auto dns2 = ethernet::global_eth_component->get_dns_address(1);

    if (dns1 != this->last_dns1_ || dns2 != this->last_dns2_) {
      this->last_dns1_ = dns1;
      this->last_dns2_ = dns2;
      // IP_ADDRESS_BUFFER_SIZE (40) = max IP (39) + null; space reuses first null's slot
      char buf[network::IP_ADDRESS_BUFFER_SIZE * 2];
      dns1.str_to(buf);
      size_t len1 = strlen(buf);
      buf[len1] = ' ';
      dns2.str_to(buf + len1 + 1);
      this->publish_state(buf);
    }
  }
  float get_setup_priority() const override { return setup_priority::ETHERNET; }
  void dump_config() override;

 protected:
  network::IPAddress last_dns1_;
  network::IPAddress last_dns2_;
};

class MACAddressEthernetInfo : public Component, public text_sensor::TextSensor {
 public:
  void setup() override { this->publish_state(ethernet::global_eth_component->get_eth_mac_address_pretty()); }
  float get_setup_priority() const override { return setup_priority::ETHERNET; }
  void dump_config() override;
};

}  // namespace esphome::ethernet_info

#endif  // USE_ESP32
