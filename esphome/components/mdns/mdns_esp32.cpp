#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_MDNS)

#include <mdns.h>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

void MDNSComponent::setup() {
  StaticVector<MDNSService, MDNS_SERVICE_COUNT> services;
  this->compile_records_(services);

  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  mdns_hostname_set(this->hostname_.c_str());
  mdns_instance_name_set(this->hostname_.c_str());

  for (const auto &service : services) {
    std::vector<mdns_txt_item_t> txt_records;
    for (const auto &record : service.txt_records) {
      mdns_txt_item_t it{};
      // key and value are either compile-time string literals in flash or pointers to dynamic_txt_values_
      // Both remain valid for the lifetime of this function, and ESP-IDF makes internal copies
      it.key = MDNS_STR_ARG(record.key);
      it.value = MDNS_STR_ARG(record.value);
      txt_records.push_back(it);
    }
    uint16_t port = const_cast<TemplatableValue<uint16_t> &>(service.port).value();
    err = mdns_service_add(nullptr, MDNS_STR_ARG(service.service_type), MDNS_STR_ARG(service.proto), port,
                           txt_records.data(), txt_records.size());

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
