#include "esphome/core/defines.h"
#if defined(USE_ESP8266) && defined(USE_ARDUINO) && defined(USE_MDNS)

#include <ESP8266mDNS.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
#include "esphome/components/wifi/wifi_component.h"
#endif
#include "mdns_component.h"

namespace esphome::mdns {

static void register_esp8266(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
  MDNS.begin(App.get_name().c_str());

  for (const auto &service : services) {
    // Strip the leading underscore from the proto and service_type. While it is
    // part of the wire protocol to have an underscore, and for example ESP-IDF
    // expects the underscore to be there, the ESP8266 implementation always adds
    // the underscore itself.
    auto *proto = MDNS_STR_ARG(service.proto);
    while (progmem_read_byte((const uint8_t *) proto) == '_') {
      proto++;
    }
    auto *service_type = MDNS_STR_ARG(service.service_type);
    while (progmem_read_byte((const uint8_t *) service_type) == '_') {
      service_type++;
    }
    uint16_t port = service.port.value();
    MDNS.addService(FPSTR(service_type), FPSTR(proto), port);
    for (const auto &record : service.txt_records) {
      MDNS.addServiceTxt(FPSTR(service_type), FPSTR(proto), FPSTR(MDNS_STR_ARG(record.key)),
                         FPSTR(MDNS_STR_ARG(record.value)));
    }
  }
}

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
void mdns_pump_update() { MDNS.update(); }
#endif

void MDNSComponent::setup() {
  this->setup_buffers_and_register_(register_esp8266);
#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
  // Arduino LEAmDNS registers its own LwipIntf::statusChangeCB that calls _restart()
  // on every netif status change (link up, IP up, etc.), so we don't trigger begin()
  // or restart here — we just cover the probe+announce window with a bounded polling
  // schedule. The listener catches subsequent reconnects and re-arms the window.
  wifi::global_wifi_component->add_ip_state_listener(this);
  this->start_polling_window_();
#else
  // Fallback for builds without a WiFi IP state listener (e.g. clang-tidy without
  // codegen defines). Matches the pre-PR behaviour: poll forever at 50ms.
  this->set_interval(MDNS_UPDATE_INTERVAL_MS, []() { MDNS.update(); });
#endif
}

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
void MDNSComponent::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &,
                                const network::IPAddress &) {
  // ESPHome's WiFiIPStateListener only notifies on IP acquisition (GOT_IP events on
  // ESP8266 — see wifi_component_esp8266.cpp), not on IP loss, so every notification
  // represents a fresh IP that the LEAmDNS library's lwIP callback will trigger a
  // _restart() for. Always re-arm the polling window — start_polling_window_() is
  // idempotent (scheduler does atomic cancel-and-add on matching IDs).
  if (ips[0].is_set()) {
    this->start_polling_window_();
  }
}
#endif

void MDNSComponent::on_shutdown() {
  MDNS.close();
  delay(10);
}

}  // namespace esphome::mdns

#endif
