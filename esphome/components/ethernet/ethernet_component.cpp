#include "ethernet_component.h"

#ifdef USE_ETHERNET

#include "esphome/core/log.h"

namespace esphome::ethernet {

EthernetComponent *global_eth_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

EthernetComponent::EthernetComponent() { global_eth_component = this; }

float EthernetComponent::get_setup_priority() const { return setup_priority::WIFI; }

void EthernetComponent::set_type(EthernetType type) { this->type_ = type; }

#ifdef USE_ETHERNET_MANUAL_IP
void EthernetComponent::set_manual_ip(const ManualIP &manual_ip) { this->manual_ip_ = manual_ip; }
#endif

#ifdef USE_ETHERNET_IP_STATE_LISTENERS
void EthernetComponent::notify_ip_state_listeners_() {
  auto ips = this->get_ip_addresses();
  auto dns1 = this->get_dns_address(0);
  auto dns2 = this->get_dns_address(1);
  for (auto *listener : this->ip_state_listeners_) {
    listener->on_ip_state(ips, dns1, dns2);
  }
}
#endif  // USE_ETHERNET_IP_STATE_LISTENERS

}  // namespace esphome::ethernet

#endif  // USE_ETHERNET
