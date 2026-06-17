#include "esphome/core/defines.h"
#if defined(USE_RP2040) && defined(USE_MDNS)

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "mdns_component.h"
// wifi_component.h / ethernet_component.h are pulled in transitively by
// mdns_component.h when their respective listener defines are active.

// Arduino-Pico's PolledTimeout.h (pulled in by ESP8266mDNS.h) redefines IRAM_ATTR to empty.
#pragma push_macro("IRAM_ATTR")
#undef IRAM_ATTR
#include <ESP8266mDNS.h>
#pragma pop_macro("IRAM_ATTR")

namespace esphome::mdns {

static void register_rp2040(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
  MDNS.begin(App.get_name().c_str());

  for (const auto &service : services) {
    // ESP8266mDNS always adds the leading underscore itself, so strip it here.
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
void MDNSComponent::start_polling_window_() {
  // uint32_t-ID set_interval/set_timeout already does atomic cancel-and-add.
  this->set_interval(MDNS_POLL_ID, MDNS_UPDATE_INTERVAL_MS, []() { MDNS.update(); });
  this->set_timeout(MDNS_POLL_STOP_ID, MDNS_POLL_WINDOW_MS, [this]() { this->cancel_interval(MDNS_POLL_ID); });
}
#endif

void MDNSComponent::setup() {
  // arduino-pico stubs out LwipIntf::stateUpCB (the netif status callback LEAmDNS uses
  // on ESP8266 for auto-restart), so we must drive begin()/notifyAPChange() from our
  // own IP state listener. Both WiFi and Ethernet have the same listener signature —
  // one on_ip_state() override serves both.
#ifdef USE_MDNS_WIFI_LISTENER
  wifi::global_wifi_component->add_ip_state_listener(this);
  // AFTER_CONNECTION priority means the network may already be up; the listener only
  // fires on subsequent changes, so seed the current state.
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
  if (ethernet::global_eth_component->is_connected()) {
    const auto ips = ethernet::global_eth_component->get_ip_addresses();
    if (ips[0].is_set()) {
      this->on_ip_state(ips, network::IPAddress{}, network::IPAddress{});
    }
  }
#endif
}

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
void MDNSComponent::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &,
                                const network::IPAddress &) {
  // Listener only fires on IP acquisition (not loss); every event is a fresh IP.
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
