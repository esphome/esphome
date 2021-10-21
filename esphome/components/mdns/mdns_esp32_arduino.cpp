#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "mdns_component.h"
#include "esphome/core/log.h"
#include <ESPmDNS.h>

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

void MDNSComponent::setup() {
  MDNS.begin(compile_hostname_().c_str());

  this->compile_services_();
  for (const auto &service : this->services_) {
    MDNS.addService(service.service_type.c_str(), service.proto.c_str(), service.port);
    for (const auto &record : service.txt_records) {
      MDNS.addServiceTxt(service.service_type.c_str(), service.proto.c_str(), record.key.c_str(), record.value.c_str());
    }
  }
}

void MDNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MDNS:");
  ESP_LOGCONFIG(TAG, "  Hostname: %s", compile_hostname_().c_str());
  ESP_LOGCONFIG(TAG, "  Services:");
  for (const auto &service : this->services_) {
    ESP_LOGCONFIG(TAG, "  - %s, %s, %d", service.service_type.c_str(), service.proto.c_str(), service.port);
    for (const auto &record : service.txt_records) {
      ESP_LOGCONFIG(TAG, "    TXT: %s = %s", record.key.c_str(), record.value.c_str());
    }
  }
}

}  // namespace mdns
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
