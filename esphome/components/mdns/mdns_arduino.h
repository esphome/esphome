#pragma once

// Common Arduino mDNS registration for RP2040 and LibreTiny
// NOTE: The platform's mDNS header (e.g., <mDNS.h> or <ESP8266mDNS.h>) must be
// included BEFORE this header to make the MDNS global available.

#include "esphome/core/application.h"
#include "mdns_component.h"

namespace esphome::mdns {

/// Register mDNS services using Arduino-style mDNS library (RP2040/LibreTiny)
/// @param services The services to register
inline void register_arduino_mdns(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
  MDNS.begin(App.get_name().c_str());

  for (const auto &service : services) {
    // Strip the leading underscore from the proto and service_type. While it is
    // part of the wire protocol to have an underscore, and for example ESP-IDF
    // expects the underscore to be there, the RP2040/LibreTiny mDNS
    // implementations always add the underscore themselves.
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

}  // namespace esphome::mdns
