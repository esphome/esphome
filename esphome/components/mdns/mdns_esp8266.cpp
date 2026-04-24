#include "esphome/core/defines.h"
#if defined(USE_ESP8266) && defined(USE_ARDUINO) && defined(USE_MDNS)

#include <ESP8266mDNS.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "mdns_component.h"
#include "esphome/components/wifi/wifi_component.h"

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
  // LEAmDNS's own LwipIntf::statusChangeCB drives _restart() on netif changes; we only
  // need to arm the polling window around the initial probe/announce and each reconnect.
  wifi::global_wifi_component->add_ip_state_listener(this);
  this->start_polling_window_();
}

void MDNSComponent::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &,
                                const network::IPAddress &) {
  // IP listener only fires on acquisition (not loss), so any notification is a fresh
  // IP worth re-arming for. start_polling_window_() is idempotent.
  if (ips[0].is_set()) {
    this->start_polling_window_();
  }
}

void MDNSComponent::on_shutdown() {
  MDNS.close();
  delay(10);
}

}  // namespace esphome::mdns

#endif
