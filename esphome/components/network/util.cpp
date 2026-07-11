#include "util.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
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

const char *get_use_address_to(std::span<char, USE_ADDRESS_BUFFER_SIZE> buf) {
  // Global component pointers are guaranteed to be set by component constructors when USE_* is defined
  const char *addr = nullptr;
#if defined(USE_ETHERNET)
  addr = ethernet::global_eth_component->get_use_address();
#elif defined(USE_MODEM)
  addr = modem::global_modem_component->get_use_address();
#elif defined(USE_WIFI)
  addr = wifi::global_wifi_component->get_use_address();
#elif defined(USE_OPENTHREAD)
  addr = openthread::global_openthread_component->get_use_address();
#endif
  if (addr != nullptr && addr[0] != '\0')
    return addr;
  // No explicit use_address configured: the address is the runtime device name
  // (which includes the MAC suffix when name_add_mac_suffix is enabled) plus ".local"
  const auto &name = App.get_name();
  make_name_with_suffix_to(buf.data(), buf.size(), name.c_str(), name.size(), '.', "local", 5);
  return buf.data();
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
