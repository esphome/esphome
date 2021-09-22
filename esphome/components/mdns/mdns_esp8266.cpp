#if defined(USE_ESP8266) && defined(USE_ARDUINO)

#include "mdns_component.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include <ESP8266mDNS.h>

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

void MDNSComponent::setup() {
  network::IPAddress addr = network::get_ip_address();
  MDNS.begin(compile_hostname_().c_str(), (uint32_t) addr);

  auto services = compile_services_();
  for (const auto &service : services) {
    MDNS.addService(service.service_type.c_str(), service.proto.c_str(), service.port);
    for (const auto &record : service.txt_records) {
      MDNS.addServiceTxt(service.service_type.c_str(), service.proto.c_str(), record.key.c_str(), record.value.c_str());
    }
  }
}

void MDNSComponent::loop() { MDNS.update(); }

}  // namespace mdns
}  // namespace esphome

#endif
