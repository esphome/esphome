#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_MDNS)

#include <mdns.h>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

namespace esphome::mdns {

static const char *const TAG = "mdns";

#if !defined(USE_OPENTHREAD) || defined(USE_OPENTHREAD_BORDER_ROUTER)
static esp_err_t add_service(const MDNSService &service) {
  // Stack buffer for up to 16 txt records, heap fallback for more
  SmallBufferWithHeapFallback<16, mdns_txt_item_t> txt_records(service.txt_records.size());
  for (size_t i = 0; i < service.txt_records.size(); i++) {
    const auto &record = service.txt_records[i];
    // key and value are either compile-time string literals in flash or pointers to dynamic_txt_values_
    // Both remain valid for the lifetime of this function, and ESP-IDF makes internal copies
    txt_records.get()[i].key = MDNS_STR_ARG(record.key);
    txt_records.get()[i].value = MDNS_STR_ARG(record.value);
  }
  uint16_t port = service.port.value();
  return mdns_service_add(nullptr, MDNS_STR_ARG(service.service_type), MDNS_STR_ARG(service.proto), port,
                          txt_records.get(), service.txt_records.size());
}
#endif

static void register_esp32(MDNSComponent *comp, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
#if defined(USE_OPENTHREAD) && !defined(USE_OPENTHREAD_BORDER_ROUTER)
  // OpenThread handles service registration via SRP client
  // Services are compiled by MDNSComponent::compile_records_() and consumed by OpenThreadSrpComponent
#else
  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Init failed: %s", esp_err_to_name(err));
    comp->mark_failed();
    return;
  }

  const char *hostname = App.get_name().c_str();
  mdns_hostname_set(hostname);
  mdns_instance_name_set(hostname);

  for (auto &service : services) {
#ifdef USE_MDNS_SUPPORTS_ENABLE_DISABLE
    if (!service.enabled)
      continue;
#endif
    err = add_service(service);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to register service %s: %s", MDNS_STR_ARG(service.service_type), esp_err_to_name(err));
#ifdef USE_MDNS_SUPPORTS_ENABLE_DISABLE
      // Let a later enable call retry
      service.enabled = false;
#endif
    }
  }
#endif
}

#if defined(USE_MDNS_SUPPORTS_ENABLE_DISABLE) && (!defined(USE_OPENTHREAD) || defined(USE_OPENTHREAD_BORDER_ROUTER))
bool MDNSComponent::set_service_enabled(const char *service_type, const char *proto, bool enabled) {
  // services_ is compiled in setup()
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "Cannot %s service %s before setup", enabled ? "enable" : "disable", service_type);
    return false;
  }
  for (auto &service : this->services_) {
    if (strcmp(MDNS_STR_ARG(service.service_type), service_type) != 0 ||
        strcmp(MDNS_STR_ARG(service.proto), proto) != 0) {
      continue;
    }
    if (service.enabled == enabled)
      return true;
    esp_err_t err = enabled ? add_service(service) : mdns_service_remove(service_type, proto);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to %s service %s: %s", enabled ? "enable" : "disable", service_type, esp_err_to_name(err));
      return false;
    }
    service.enabled = enabled;
    return true;
  }
  ESP_LOGW(TAG, "Service %s not found", service_type);
  return false;
}
#endif

void MDNSComponent::setup() { this->setup_buffers_and_register_(register_esp32); }

void MDNSComponent::on_shutdown() {
#if !defined(USE_OPENTHREAD) || defined(USE_OPENTHREAD_BORDER_ROUTER)
  mdns_free();
  delay(40);  // Allow the mdns packets announcing service removal to be sent
#endif
}

}  // namespace esphome::mdns

#endif  // USE_ESP32
