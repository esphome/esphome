#include "mdns_component.h"
#include "esphome/core/defines.h"
#include "esphome/core/version.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_DASHBOARD_IMPORT
#include "esphome/components/dashboard_import/dashboard_import.h"
#endif

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

#ifndef WEBSERVER_PORT
#define WEBSERVER_PORT 80  // NOLINT
#endif

void MDNSComponent::compile_records_() {
  this->hostname_ = App.get_name();

  this->services_.clear();
#ifdef USE_API
  if (api::global_api_server != nullptr) {
    MDNSService service{};
    service.service_type = "_esphomelib";
    service.proto = "_tcp";
    service.port = api::global_api_server->get_port();
    service.txt_records.push_back({"version", ESPHOME_VERSION});
    service.txt_records.push_back({"mac", get_mac_address()});
    const char *platform = nullptr;
#ifdef USE_ESP8266
    platform = "ESP8266";
#endif
#ifdef USE_ESP32
    platform = "ESP32";
#endif
    if (platform != nullptr) {
      service.txt_records.push_back({"platform", platform});
    }

    service.txt_records.push_back({"board", ESPHOME_BOARD});

#ifdef ESPHOME_PROJECT_NAME
    service.txt_records.push_back({"project_name", ESPHOME_PROJECT_NAME});
    service.txt_records.push_back({"project_version", ESPHOME_PROJECT_VERSION});
#endif  // ESPHOME_PROJECT_NAME

#ifdef USE_DASHBOARD_IMPORT
    service.txt_records.push_back({"package_import_url", dashboard_import::get_package_import_url()});
#endif

    this->services_.push_back(service);
  }
#endif  // USE_API

#ifdef USE_PROMETHEUS
  {
    MDNSService service{};
    service.service_type = "_prometheus-http";
    service.proto = "_tcp";
    service.port = WEBSERVER_PORT;
    this->services_.push_back(service);
  }
#endif

  if (this->services_.empty()) {
    // Publish "http" service if not using native API
    // This is just to have *some* mDNS service so that .local resolution works
    MDNSService service{};
    service.service_type = "_http";
    service.proto = "_tcp";
    service.port = WEBSERVER_PORT;
    service.txt_records.push_back({"version", ESPHOME_VERSION});
    this->services_.push_back(service);
  }
}

void MDNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "mDNS:");
  ESP_LOGCONFIG(TAG, "  Hostname: %s", this->hostname_.c_str());
  ESP_LOGV(TAG, "  Services:");
  for (const auto &service : this->services_) {
    ESP_LOGV(TAG, "  - %s, %s, %d", service.service_type.c_str(), service.proto.c_str(), service.port);
    for (const auto &record : service.txt_records) {
      ESP_LOGV(TAG, "    TXT: %s = %s", record.key.c_str(), record.value.c_str());
    }
  }
}

}  // namespace mdns
}  // namespace esphome
