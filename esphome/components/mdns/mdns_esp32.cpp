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
  global_mdns = this;
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

std::vector<network::IPAddress> MDNSComponent::resolve(const std::string &servicename) {
  std::vector<network::IPAddress> resolved;
  mdns_result_t *results = nullptr;
  mdns_ip_addr_t *a = nullptr;
  esp_err_t err = mdns_query_ptr(("_" + servicename).c_str(), "_tcp", 3000, 20, &results);
  if (err) {
    ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
    return {};
  }
  if (!results) {
    ESP_LOGW(TAG, "No results found!");
    return {};
  }
  while (results) {
    a = results->addr;
    while (a) {
      network::IPAddress ip_addr = network::IPAddress(&a->addr);
      if (std::count(resolved.begin(), resolved.end(), ip_addr) == 0) {
        resolved.push_back(ip_addr);
      }
      ESP_LOGVV(TAG, "Found mDNS %s", ip_addr.str().c_str());
      a = a->next;
    }
    results = results->next;
  }

  mdns_query_results_free(results);

  return resolved;
}

void MDNSComponent::on_shutdown() {
  mdns_free();
  delay(40);  // Allow the mdns packets announcing service removal to be sent
}

MDNSComponent *global_mdns = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace mdns
}  // namespace esphome

#endif  // USE_ESP32
