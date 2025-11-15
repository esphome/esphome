#include "util.h"
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif

#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#endif

#ifdef USE_MODEM
#include "esphome/components/modem/modem_component.h"
#endif

namespace esphome {
namespace network {

// The order of the components is important: WiFi should come after any possible main interfaces (it may be used as
// an AP that use a previous interface for NAT).

bool is_connected() {
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr && ethernet::global_eth_component->is_connected())
    return true;
#endif

#ifdef USE_MODEM
  if (modem::global_modem_component != nullptr)
    return modem::global_modem_component->is_connected();
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->is_connected();
#endif

#ifdef USE_OPENTHREAD
  if (openthread::global_openthread_component != nullptr)
    return openthread::global_openthread_component->is_connected();
#endif

#ifdef USE_HOST
  return true;  // Assume its connected
#endif
  return false;
}

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

const char *get_use_address() {
  // Global component pointers are guaranteed to be set by component constructors when USE_* is defined
#ifdef USE_ETHERNET
  return ethernet::global_eth_component->get_use_address();
#endif

#ifdef USE_MODEM
  return modem::global_modem_component->get_use_address();
#endif

#ifdef USE_WIFI
  return wifi::global_wifi_component->get_use_address();
#endif

#ifdef USE_OPENTHREAD
  return openthread::global_openthread_component->get_use_address();
#endif

#if !defined(USE_ETHERNET) && !defined(USE_MODEM) && !defined(USE_WIFI) && !defined(USE_OPENTHREAD)
  // Fallback when no network component is defined (e.g., host platform)
  return "";
#endif
}

}  // namespace network
}  // namespace esphome
#endif
