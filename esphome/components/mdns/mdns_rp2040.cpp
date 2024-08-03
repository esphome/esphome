#include "esphome/core/defines.h"
#if defined(USE_RP2040) && defined(USE_MDNS)

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

#include <ESP8266mDNS.h>

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

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

std::vector<network::IPAddress> MDNSComponent::resolve(const std::string &servicename) {
  std::vector<network::IPAddress> resolved;
  uint8_t n = MDNS.queryService(servicename.c_str(), "tcp");
  for (uint8_t i = 0; i < n; i++) {
    network::IPAddress ip_addr_ = network::IPAddress(MDNS.IP(i));
    if (std::count(resolved.begin(), resolved.end(), ip_addr_) == 0) {
      resolved.push_back(ip_addr_);
    }
    ESP_LOGVV(TAG, "Found mDNS %s", ip_addr_.str().c_str());
  }
  return resolved;
}

void MDNSComponent::loop() { MDNS.update(); }

void MDNSComponent::on_shutdown() {
  MDNS.close();
  delay(40);
}

MDNSComponent *global_mdns = nullptr;
}  // namespace mdns
}  // namespace esphome

#endif
