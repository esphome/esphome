#include "esphome/core/util.h"
#include "esphome/core/defines.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/core/log.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif

#ifdef ARDUINO_ARCH_ESP32
#include <ESPmDNS.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266mDNS.h>
#endif

namespace esphome {

bool network_is_connected() {
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr && ethernet::global_eth_component->is_connected())
    return true;
#endif

#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->is_connected();
#endif

  return false;
}

#ifdef ARDUINO_ARCH_ESP8266
bool mdns_setup;
#endif

#ifdef ARDUINO_ARCH_ESP8266
void network_setup_mdns(IPAddress address, int interface) {
  // Latest arduino framework breaks mDNS for AP interface
  // see https://github.com/esp8266/Arduino/issues/6114
  if (interface == 1)
    return;
  MDNS.begin(App.get_name().c_str(), address);
  mdns_setup = true;
#endif
#ifdef ARDUINO_ARCH_ESP32
  void network_setup_mdns() {
    MDNS.begin(App.get_name().c_str());
#endif
#ifdef USE_API
    if (api::global_api_server != nullptr) {
      MDNS.addService("esphomelib", "tcp", api::global_api_server->get_port());
      // DNS-SD (!=mDNS !) requires at least one TXT record for service discovery - let's add version
      MDNS.addServiceTxt("esphomelib", "tcp", "version", ESPHOME_VERSION);
      MDNS.addServiceTxt("esphomelib", "tcp", "address", network_get_address().c_str());
    } else {
#endif
      // Publish "http" service if not using native API.
      // This is just to have *some* mDNS service so that .local resolution works
      MDNS.addService("http", "tcp", 80);
      MDNS.addServiceTxt("http", "tcp", "version", ESPHOME_VERSION);
#ifdef USE_API
    }
#endif
  }
  void network_tick_mdns() {
#ifdef ARDUINO_ARCH_ESP8266
    if (mdns_setup)
      MDNS.update();
#endif
  }

  std::string network_get_address() {
#ifdef USE_ETHERNET
    if (ethernet::global_eth_component != nullptr)
      return ethernet::global_eth_component->get_use_address();
#endif
#ifdef USE_WIFI
    if (wifi::global_wifi_component != nullptr)
      return wifi::global_wifi_component->get_use_address();
#endif
    return "";
  }

}  // namespace esphome
