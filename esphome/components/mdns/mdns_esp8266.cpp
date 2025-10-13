#include "esphome/core/defines.h"
#if defined(USE_ESP8266) && defined(USE_ARDUINO) && defined(USE_MDNS)

#include <ESP8266mDNS.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

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
    auto *proto = MDNS_STR_ARG(service.proto);
    while (progmem_read_byte((const uint8_t *) proto) == '_') {
      proto++;
    }
    auto *service_type = MDNS_STR_ARG(service.service_type);
    while (progmem_read_byte((const uint8_t *) service_type) == '_') {
      service_type++;
    }
    uint16_t port = const_cast<TemplatableValue<uint16_t> &>(service.port).value();
    MDNS.addService(FPSTR(service_type), FPSTR(proto), port);
    for (const auto &record : service.txt_records) {
      MDNS.addServiceTxt(FPSTR(service_type), FPSTR(proto), FPSTR(MDNS_STR_ARG(record.key)),
                         FPSTR(MDNS_STR_ARG(record.value)));
    }
  }
}
network::IPAddress MDNSComponent::resolve(const std::string &servicename) {
  if (MDNS.queryService(servicename.c_str(), "tcp") > 0) {
    auto ip_addr = network::IPAddress(MDNS.IP(0));
    ESP_LOGVV(TAG, "Found mDNS %s", ip_addr.str().c_str());
    return ip_addr;
  }
  return network::IPAddress();
}

void MDNSComponent::loop() { MDNS.update(); }

void MDNSComponent::on_shutdown() {
  MDNS.close();
  delay(10);
}

MDNSComponent *global_mdns = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace mdns
}  // namespace esphome

#endif
