#include "util.h"
#include "esphome/core/defines.h"
#ifdef USE_NETWORK

namespace esphome::network {

// The order of the components is important: WiFi should come after any possible main interfaces (it may be used as
// an AP that uses a previous interface for NAT).

bool is_disabled() {
#ifdef USE_MODEM
  if (modem::global_modem_component != nullptr)
    return modem::global_modem_component->is_disabled();
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->is_disabled();
#endif
  return false;
}

network::IPAddresses get_ip_addresses() {
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr)
    return ethernet::global_eth_component->get_ip_addresses();
#endif

#ifdef USE_MODEM
  if (modem::global_modem_component != nullptr)
    return modem::global_modem_component->get_ip_addresses();
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->get_ip_addresses();
#endif
#ifdef USE_OPENTHREAD
  if (openthread::global_openthread_component != nullptr)
    return openthread::global_openthread_component->get_ip_addresses();
#endif
  return {};
}

}  // namespace esphome::network
#endif
