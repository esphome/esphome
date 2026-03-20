#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include <string>
#include "esphome/core/helpers.h"
#include "ip_address.h"

#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif
#ifdef USE_MODEM
#include "esphome/components/modem/modem_component.h"
#endif
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#endif

namespace esphome::network {

// The order of the components is important: WiFi should come after any possible main interfaces (it may be used as
// an AP that uses a previous interface for NAT).

/// Return whether the node is connected to the network (through wifi, eth, ...)
ESPHOME_ALWAYS_INLINE inline bool is_connected() {
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
  return true;  // Assume it's connected
#endif
  return false;
}

/// Return whether the network is disabled (only wifi for now)
bool is_disabled();
/// Get the active network hostname
const char *get_use_address();
IPAddresses get_ip_addresses();

}  // namespace esphome::network
#endif
