#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/ethernet/ethernet_component.h"

#ifdef USE_ESP32

namespace esphome::ethernet_info {

#ifdef USE_ETHERNET_IP_STATE_LISTENERS
class IPAddressEthernetInfo final : public Component,
                                    public text_sensor::TextSensor,
                                    public ethernet::EthernetIPStateListener {
 public:
  void setup() override;
  void dump_config() override;
  void add_ip_sensors(uint8_t index, text_sensor::TextSensor *s) { this->ip_sensors_[index] = s; }

  // EthernetIPStateListener interface
  void on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                   const network::IPAddress &dns2) override;

 protected:
  std::array<text_sensor::TextSensor *, 5> ip_sensors_{};
};

class DNSAddressEthernetInfo final : public Component,
                                     public text_sensor::TextSensor,
                                     public ethernet::EthernetIPStateListener {
 public:
  void setup() override;
  void dump_config() override;

  // EthernetIPStateListener interface
  void on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                   const network::IPAddress &dns2) override;
};
#endif  // USE_ETHERNET_IP_STATE_LISTENERS

class MACAddressEthernetInfo final : public Component, public text_sensor::TextSensor {
 public:
  void setup() override {
    char buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    this->publish_state(ethernet::global_eth_component->get_eth_mac_address_pretty_into_buffer(buf));
  }
  float get_setup_priority() const override { return setup_priority::ETHERNET; }
  void dump_config() override;
};

}  // namespace esphome::ethernet_info

#endif  // USE_ESP32
