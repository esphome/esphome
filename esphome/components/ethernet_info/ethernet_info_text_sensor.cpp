#include "ethernet_info_text_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::ethernet_info {

static const char *const TAG = "ethernet_info";

#ifdef USE_ETHERNET_IP_STATE_LISTENERS
void IPAddressEthernetInfo::setup() { ethernet::global_eth_component->add_ip_state_listener(this); }

void IPAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo IPAddress", this); }

void IPAddressEthernetInfo::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                                        const network::IPAddress &dns2) {
  char buf[network::IP_ADDRESS_BUFFER_SIZE];
  ips[0].str_to(buf);
  this->publish_state(buf);
  uint8_t sensor = 0;
  for (const auto &ip : ips) {
    if (ip.is_set()) {
      if (this->ip_sensors_[sensor] != nullptr) {
        ip.str_to(buf);
        this->ip_sensors_[sensor]->publish_state(buf);
      }
      sensor++;
    }
  }
}

void DNSAddressEthernetInfo::setup() { ethernet::global_eth_component->add_ip_state_listener(this); }

void DNSAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo DNS Address", this); }

void DNSAddressEthernetInfo::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                                         const network::IPAddress &dns2) {
  // IP_ADDRESS_BUFFER_SIZE (40) = max IP (39) + null; space reuses first null's slot
  char buf[network::IP_ADDRESS_BUFFER_SIZE * 2];
  dns1.str_to(buf);
  size_t len1 = strlen(buf);
  buf[len1] = ' ';
  dns2.str_to(buf + len1 + 1);
  this->publish_state(buf);
}
#endif  // USE_ETHERNET_IP_STATE_LISTENERS

void MACAddressEthernetInfo::dump_config() { LOG_TEXT_SENSOR("", "EthernetInfo MAC Address", this); }

}  // namespace esphome::ethernet_info

#endif  // USE_ESP32
