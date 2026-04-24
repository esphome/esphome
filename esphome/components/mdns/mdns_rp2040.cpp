#include "esphome/core/defines.h"
#if defined(USE_RP2040) && defined(USE_MDNS)

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "mdns_component.h"
#ifdef USE_MDNS_WIFI_LISTENER
#include "esphome/components/wifi/wifi_component.h"
#endif
#ifdef USE_MDNS_ETHERNET_LISTENER
#include "esphome/components/ethernet/ethernet_component.h"
#endif

// Arduino-Pico's PolledTimeout.h (pulled in by ESP8266mDNS.h) redefines IRAM_ATTR to empty.
// Save and restore our definition around the include to avoid a redefinition warning.
#pragma push_macro("IRAM_ATTR")
#undef IRAM_ATTR
#include <ESP8266mDNS.h>
#pragma pop_macro("IRAM_ATTR")

namespace esphome::mdns {

static void register_rp2040(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
  MDNS.begin(App.get_name().c_str());

  for (const auto &service : services) {
    // Strip the leading underscore from the proto and service_type. While it is
    // part of the wire protocol to have an underscore, and for example ESP-IDF
    // expects the underscore to be there, the ESP8266 implementation always adds
    // the underscore itself.
    auto *proto = MDNS_STR_ARG(service.proto);
    while (*proto == '_') {
      proto++;
    }
    auto *service_type = MDNS_STR_ARG(service.service_type);
    while (*service_type == '_') {
      service_type++;
    }
    uint16_t port = service.port.value();
    MDNS.addService(service_type, proto, port);
    for (const auto &record : service.txt_records) {
      MDNS.addServiceTxt(service_type, proto, MDNS_STR_ARG(record.key), MDNS_STR_ARG(record.value));
    }
  }
}

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
void mdns_pump_update() { MDNS.update(); }
#endif

void MDNSComponent::setup() {
  // RP2040's LEAmDNS library registers a LwipIntf::stateUpCB() callback to restart
  // mDNS when the network interface reconnects. However, stateUpCB() is stubbed out
  // in arduino-pico's LwipIntfCB.cpp because the original ESP8266 implementation used
  // schedule_function() which doesn't exist in arduino-pico, and the callback can't
  // safely run directly since netif status callbacks fire from IRQ context
  // (PICO_CYW43_ARCH_THREADSAFE_BACKGROUND) while _restart() allocates UDP sockets.
  //
  // Workaround: defer MDNS.begin() and service registration until the network has an IP,
  // then call notifyAPChange() on subsequent reconnects to restart mDNS probing and
  // announcing — all from main loop context via the IP state listener callback(s) so
  // it's thread-safe. We subscribe to any listener interface that's active (WiFi and/or
  // Ethernet); the same on_ip_state() override serves both because the signatures match.
#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
#ifdef USE_MDNS_WIFI_LISTENER
  wifi::global_wifi_component->add_ip_state_listener(this);
  // AFTER_CONNECTION priority means the network may already be up when setup() runs;
  // the listener only fires on subsequent state changes, so seed the current state.
  {
    const auto ips = wifi::global_wifi_component->wifi_sta_ip_addresses();
    if (ips[0].is_set()) {
      this->on_ip_state(ips, wifi::global_wifi_component->get_dns_address(0),
                        wifi::global_wifi_component->get_dns_address(1));
    }
  }
#endif
#ifdef USE_MDNS_ETHERNET_LISTENER
  ethernet::global_eth_component->add_ip_state_listener(this);
  // Seed current Ethernet state for the same reason — if the interface is already up
  // when mdns setup() runs, we need to kick off begin() + polling here.
  if (ethernet::global_eth_component->is_connected()) {
    const auto ips = ethernet::global_eth_component->get_ip_addresses();
    if (ips[0].is_set()) {
      this->on_ip_state(ips, network::IPAddress{}, network::IPAddress{});
    }
  }
#endif
#else
  // Fallback (no IP state listener available): poll forever, checking connection state
  // each tick.
  this->set_interval(MDNS_UPDATE_INTERVAL_MS, [this]() {
    bool connected = network::is_connected();
    if (connected && !this->was_connected_) {
      if (!this->initialized_) {
        this->setup_buffers_and_register_(register_rp2040);
        this->initialized_ = true;
      } else {
        MDNS.notifyAPChange();
      }
    }
    this->was_connected_ = connected;
    if (this->initialized_) {
      MDNS.update();
    }
  });
#endif
}

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
void MDNSComponent::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &,
                                const network::IPAddress &) {
  // Both WiFi and Ethernet IP state listeners only notify on IP acquisition (see
  // wifi_component_pico_w.cpp and ethernet_component.cpp), not on IP loss, so every
  // notification represents a fresh IP that needs a probe/announce cycle. The library's
  // internal LwipIntf::stateUpCB is stubbed out on arduino-pico (see setup()), so we
  // drive begin/restart ourselves from this callback.
  if (!ips[0].is_set()) {
    return;
  }
  if (!this->initialized_) {
    this->setup_buffers_and_register_(register_rp2040);
    this->initialized_ = true;
  } else {
    MDNS.notifyAPChange();
  }
  this->start_polling_window_();
}
#endif

void MDNSComponent::on_shutdown() {
  MDNS.close();
  delay(40);
}

}  // namespace esphome::mdns

#endif
