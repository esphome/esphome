#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_MDNS)

#include <mdns.h>
#include <cstring>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

void MDNSComponent::setup() {
  this->compile_records_();

  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  mdns_hostname_set(this->hostname_.c_str());
  mdns_instance_name_set(this->hostname_.c_str());

  for (const auto &service : this->services_) {
    std::vector<mdns_txt_item_t> txt_records;
    for (const auto &record : service.txt_records) {
      mdns_txt_item_t it{};
      // key is a compile-time string literal in flash, no need to strdup
      it.key = MDNS_STR_ARG(record.key);
      // value is a temporary from TemplatableValue, must strdup to keep it alive
      it.value = strdup(const_cast<TemplatableValue<std::string> &>(record.value).value().c_str());
      txt_records.push_back(it);
    }
    uint16_t port = const_cast<TemplatableValue<uint16_t> &>(service.port).value();
    err = mdns_service_add(nullptr, MDNS_STR_ARG(service.service_type), MDNS_STR_ARG(service.proto), port,
                           txt_records.data(), txt_records.size());

    // free records
    for (const auto &it : txt_records) {
      free((void *) it.value);  // NOLINT(cppcoreguidelines-no-malloc)
    }

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
