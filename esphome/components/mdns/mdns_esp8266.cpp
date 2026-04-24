#include "esphome/core/defines.h"
#if defined(USE_ESP8266) && defined(USE_ARDUINO) && defined(USE_MDNS)

#include <ESP8266mDNS.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"
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

void mdns_pump_update() { MDNS.update(); }

void MDNSComponent::setup() {
  this->setup_buffers_and_register_(register_esp8266);
  // Arduino LEAmDNS registers its own LwipIntf::statusChangeCB that calls _restart()
  // on every netif status change (link up, IP up, etc.), so we don't trigger begin()
  // or restart here — we just cover the probe+announce window with a bounded polling
  // schedule. The listener catches subsequent reconnects and re-arms the window.
  wifi::global_wifi_component->add_ip_state_listener(this);
  this->start_polling_window_();
}

void MDNSComponent::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &,
                                const network::IPAddress &) {
  const bool has_ip = ips[0].is_set();
  if (has_ip && !this->ip_was_up_) {
    // IP came up. LEAmDNS's internal lwIP callback will call _restart() shortly after
    // (if it hasn't already) — arm the polling window so the probe/announce phase is
    // serviced regardless of our relative timing vs the library's callback.
    this->start_polling_window_();
  } else if (!has_ip && this->ip_was_up_) {
    this->cancel_polling_window_();
  }
  this->ip_was_up_ = has_ip;
}

void MDNSComponent::on_shutdown() {
  MDNS.close();
  delay(10);
}

}  // namespace esphome::mdns

#endif
