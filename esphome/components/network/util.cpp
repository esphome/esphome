#include "util.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#ifdef USE_NETWORK

namespace esphome::network {

#if defined(USE_ESP32) && USE_NETWORK_IPV6
ESPHOME_LOG_TAG(TAG, "network");

void fill_ip6_addresses(esp_netif_t *netif, IPAddresses &addresses) {
  esp_ip6_addr_t ip6s[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  size_t count = esp_netif_get_all_ip6(netif, ip6s);
  // Slot 0 holds the IPv4 address
  if (count > addresses.size() - 1) {
    // Warn once per interface
    static esp_netif_t *warned[3]{};
    for (auto &slot : warned) {
      if (slot == netif) {
        break;
      }
      if (slot == nullptr) {
        slot = netif;
        ESP_LOGW(TAG, "%s has %zu IPv6 addresses, only %zu are reported", esp_netif_get_desc(netif), count,
                 addresses.size() - 1);
        break;
      }
    }
    count = addresses.size() - 1;
  }
  for (size_t i = 0; i < count; i++) {
    addresses[i + 1] = IPAddress(&ip6s[i]);
  }
}
#endif

// The order of the components is important: WiFi should come after any possible main interfaces (it may be used as
// an AP that uses a previous interface for NAT).

bool is_disabled() {
  // The network is disabled only when every configured interface with a
  // disable() lifecycle is disabled; one enabled interface means traffic can flow.
  bool disabled = false;
#ifdef USE_MODEM
  if (modem::global_modem_component != nullptr) {
    if (!modem::global_modem_component->is_disabled())
      return false;
    disabled = true;
  }
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr) {
    if (!wifi::global_wifi_component->is_disabled())
      return false;
    disabled = true;
  }
#endif

#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr) {
    if (!ethernet::global_eth_component->is_disabled())
      return false;
    disabled = true;
  }
#endif
  return disabled;
}

const char *get_use_address_to(std::span<char, USE_ADDRESS_BUFFER_SIZE> buf) {
  // Global component pointers are guaranteed to be set by component constructors when USE_* is defined.
  // A wifi-first network: priority: list sets USE_NETWORK_PRIMARY_INTERFACE_WIFI to lift
  // wifi ahead of the fixed ethernet-first order below; an ethernet-first list already
  // matches that order, so no define exists for it.
  const char *addr = nullptr;
#if defined(USE_NETWORK_PRIMARY_INTERFACE_WIFI) && defined(USE_WIFI)
  addr = wifi::global_wifi_component->get_use_address();
#elif defined(USE_ETHERNET)
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
  // With a wifi-first network: priority: list, prefer wifi while it has a valid IP;
  // otherwise fall through to the fixed ethernet-first order below. Selection based
  // on the runtime-active interface is a planned follow-up.
#if defined(USE_NETWORK_PRIMARY_INTERFACE_WIFI) && defined(USE_WIFI)
  if (wifi::global_wifi_component != nullptr) {
    auto ips = wifi::global_wifi_component->get_ip_addresses();
    for (const auto &ip : ips) {
      if (ip.is_set())
        return ips;
    }
  }
#endif

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
