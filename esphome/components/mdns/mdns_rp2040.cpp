#include "esphome/core/defines.h"
#if defined(USE_RP2040) && defined(USE_MDNS)

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

#include <ESP8266mDNS.h>

namespace esphome {
namespace mdns {

void MDNSComponent::setup() {
  StaticVector<MDNSService, MDNS_SERVICE_COUNT> services;
  this->compile_records_(services);

  MDNS.begin(this->hostname_.c_str());

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
    uint16_t port = const_cast<TemplatableValue<uint16_t> &>(service.port).value();
    MDNS.addService(service_type, proto, port);
    for (const auto &record : service.txt_records) {
      MDNS.addServiceTxt(service_type, proto, MDNS_STR_ARG(record.key), MDNS_STR_ARG(record.value));
    }
  }
}

void MDNSComponent::loop() { MDNS.update(); }

void MDNSComponent::on_shutdown() {
  MDNS.close();
  delay(40);
}

}  // namespace mdns
}  // namespace esphome

#endif
