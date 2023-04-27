#if defined(USE_RP2040) || defined(USE_LIBRETINY)

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

#ifndef USE_LIBRETINY
#include <ESP8266mDNS.h>
#else
#include <mDNS.h>
#endif

namespace esphome {
namespace mdns {

void MDNSComponent::setup() {
  this->compile_records_();

#ifndef USE_LIBRETINY
  network::IPAddress addr = network::get_ip_address();
  MDNS.begin(this->hostname_.c_str(), (uint32_t) addr);
#else
  MDNS.begin(this->hostname_.c_str());
#endif

  for (const auto &service : this->services_) {
    // Strip the leading underscore from the proto and service_type. While it is
    // part of the wire protocol to have an underscore, and for example ESP-IDF
    // expects the underscore to be there, the ESP8266 implementation always adds
    // the underscore itself.
    auto *proto = service.proto.c_str();
    while (*proto == '_') {
      proto++;
    }
    auto *service_type = service.service_type.c_str();
    while (*service_type == '_') {
      service_type++;
    }
    MDNS.addService(service_type, proto, service.port);
    for (const auto &record : service.txt_records) {
      MDNS.addServiceTxt(service_type, proto, record.key.c_str(), record.value.c_str());
    }
  }
}

#ifndef USE_LIBRETINY
void MDNSComponent::loop() { MDNS.update(); }
#endif

void MDNSComponent::on_shutdown() {
#ifndef USE_LIBRETINY
  MDNS.close();
  delay(40);
#endif
}

}  // namespace mdns
}  // namespace esphome

#endif
