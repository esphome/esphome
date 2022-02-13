#ifdef USE_ESP_IDF

#include "mdns_component.h"
#include "esphome/core/log.h"
#include <mdns.h>
#include <cstring>

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

void MDNSComponent::setup() {
  this->compile_records_();

  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  mdns_hostname_set(this->hostname_.c_str());
  mdns_instance_name_set(this->hostname_.c_str());

  for (const auto &service : this->services_) {
    std::vector<mdns_txt_item_t> txt_records;
    for (const auto &record : service.txt_records) {
      mdns_txt_item_t it{};
      // dup strings to ensure the pointer is valid even after the record loop
      it.key = strdup(record.key.c_str());
      it.value = strdup(record.value.c_str());
      txt_records.push_back(it);
    }
    err = mdns_service_add(nullptr, service.service_type.c_str(), service.proto.c_str(), service.port,
                           txt_records.data(), txt_records.size());

    // free records
    for (const auto &it : txt_records) {
      delete it.key;    // NOLINT(cppcoreguidelines-owning-memory)
      delete it.value;  // NOLINT(cppcoreguidelines-owning-memory)
    }

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to register mDNS service %s: %s", service.service_type.c_str(), esp_err_to_name(err));
    }
  }
}

}  // namespace mdns
}  // namespace esphome

#endif
