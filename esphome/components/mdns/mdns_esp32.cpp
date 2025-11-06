#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_MDNS)

#include <mdns.h>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

void MDNSComponent::setup() {
#ifdef USE_MDNS_STORE_SERVICES
  this->compile_records_(this->services_);
  const auto &services = this->services_;
#else
  StaticVector<MDNSService, MDNS_SERVICE_COUNT> services;
  this->compile_records_(services);
#endif

  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  const char *hostname = App.get_name().c_str();
  mdns_hostname_set(hostname);
  mdns_instance_name_set(hostname);

  for (const auto &service : services) {
    auto txt_records = std::make_unique<mdns_txt_item_t[]>(service.txt_records.size());
    for (size_t i = 0; i < service.txt_records.size(); i++) {
      const auto &record = service.txt_records[i];
      // key and value are either compile-time string literals in flash or pointers to dynamic_txt_values_
      // Both remain valid for the lifetime of this function, and ESP-IDF makes internal copies
      txt_records[i].key = MDNS_STR_ARG(record.key);
      txt_records[i].value = MDNS_STR_ARG(record.value);
    }
    uint16_t port = const_cast<TemplatableValue<uint16_t> &>(service.port).value();
    err = mdns_service_add(nullptr, MDNS_STR_ARG(service.service_type), MDNS_STR_ARG(service.proto), port,
                           txt_records.get(), service.txt_records.size());

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to register service %s: %s", MDNS_STR_ARG(service.service_type), esp_err_to_name(err));
    }
  }
}

void MDNSComponent::on_shutdown() {
  mdns_free();
  delay(40);  // Allow the mdns packets announcing service removal to be sent
}

}  // namespace mdns
}  // namespace esphome

#endif  // USE_ESP32
