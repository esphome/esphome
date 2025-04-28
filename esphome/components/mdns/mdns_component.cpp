#include "esphome/core/defines.h"
#ifdef USE_MDNS
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "mdns_component.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_DASHBOARD_IMPORT
#include "esphome/components/dashboard_import/dashboard_import.h"
#endif

namespace esphome {
namespace mdns {

static const char *const TAG = "mdns";

#ifndef USE_WEBSERVER_PORT
#define USE_WEBSERVER_PORT 80  // NOLINT
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
    if (!App.get_friendly_name().empty()) {
      service.txt_records.push_back({"friendly_name", App.get_friendly_name()});
    }
    service.txt_records.push_back({"version", ESPHOME_VERSION});
    service.txt_records.push_back({"mac", get_mac_address()});
    const char *platform = nullptr;
#ifdef USE_ESP8266
    platform = "ESP8266";
#endif
#ifdef USE_ESP32
    platform = "ESP32";
#endif
#ifdef USE_RP2040
    platform = "RP2040";
#endif
#ifdef USE_LIBRETINY
    platform = lt_cpu_get_model_name();
#endif
    if (platform != nullptr) {
      service.txt_records.push_back({"platform", platform});
    }

    service.txt_records.push_back({"board", ESPHOME_BOARD});

#if defined(USE_WIFI)
    service.txt_records.push_back({"network", "wifi"});
#elif defined(USE_ETHERNET)
    service.txt_records.push_back({"network", "ethernet"});
#endif

#ifdef USE_API_NOISE
    if (api::global_api_server->get_noise_ctx()->has_psk()) {
      service.txt_records.push_back({"api_encryption", "Noise_NNpsk0_25519_ChaChaPoly_SHA256"});
    } else {
      service.txt_records.push_back({"api_encryption_supported", "Noise_NNpsk0_25519_ChaChaPoly_SHA256"});
    }
#endif

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
    service.port = USE_WEBSERVER_PORT;
    this->services_.push_back(service);
  }
#endif

#ifdef USE_WEBSERVER
  {
    MDNSService service{};
    service.service_type = "_http";
    service.proto = "_tcp";
    service.port = USE_WEBSERVER_PORT;
    this->services_.push_back(service);
  }
#endif

  this->services_.insert(this->services_.end(), this->services_extra_.begin(), this->services_extra_.end());

  if (this->services_.empty()) {
    // Publish "http" service if not using native API
    // This is just to have *some* mDNS service so that .local resolution works
    MDNSService service{};
    service.service_type = "_http";
    service.proto = "_tcp";
    service.port = USE_WEBSERVER_PORT;
    service.txt_records.push_back({"version", ESPHOME_VERSION});
    this->services_.push_back(service);
  }
}

void MDNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "mDNS:");
  ESP_LOGCONFIG(TAG, "  Hostname: %s", this->hostname_.c_str());
  ESP_LOGV(TAG, "  Services:");
  for (const auto &service : this->services_) {
    ESP_LOGV(TAG, "  - %s, %s, %d", service.service_type.c_str(), service.proto.c_str(),
             const_cast<TemplatableValue<uint16_t> &>(service.port).value());
    for (const auto &record : service.txt_records) {
      ESP_LOGV(TAG, "    TXT: %s = %s", record.key.c_str(),
               const_cast<TemplatableValue<std::string> &>(record.value).value().c_str());
    }
  }
}

}  // namespace mdns
}  // namespace esphome
#endif
