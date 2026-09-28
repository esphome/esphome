#include "ethernet_component.h"

#ifdef USE_ETHERNET

#include "esphome/core/log.h"

namespace esphome::ethernet {

EthernetComponent *global_eth_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

EthernetComponent::EthernetComponent() { global_eth_component = this; }

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
