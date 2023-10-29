#ifdef USE_LIBRETINY

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

#include <mDNS.h>

namespace esphome {
namespace mdns {

void MDNSComponent::setup() {
  global_mdns = this;
  this->compile_records_();

  MDNS.begin(this->hostname_.c_str());

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

/*
  // Libre tiny doesn't have a "full" mDNS implementation
  network::IPAddress MDNSComponent::resolve(std::string servicename) {
  int n = MDNS.queryService(servicename.c_str(), "tcp");
  if (n > 0) {
    return network::IPAddress(MDNS.IP(0));
  }
  return network::IPAddress();
}*/

void MDNSComponent::on_shutdown() {}

MDNSComponent *global_mdns = nullptr;
}  // namespace mdns
}  // namespace esphome

#endif
