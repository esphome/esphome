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

  // Calculate exact capacity needed for services vector
  size_t services_count = 0;
#ifdef USE_API
  if (api::global_api_server != nullptr) {
    services_count++;
  }
#endif
#ifdef USE_PROMETHEUS
  services_count++;
#endif
#ifdef USE_WEBSERVER
  services_count++;
#endif
#ifdef USE_MDNS_EXTRA_SERVICES
  services_count += this->services_extra_.size();
#endif
  // Reserve for fallback service if needed
  if (services_count == 0) {
    services_count = 1;
  }
  this->services_.reserve(services_count);

#ifdef USE_API
  if (api::global_api_server != nullptr) {
    this->services_.emplace_back();
    auto &service = this->services_.back();
    service.service_type = "_esphomelib";
    service.proto = "_tcp";
    service.port = api::global_api_server->get_port();

    const std::string &friendly_name = App.get_friendly_name();
    bool friendly_name_empty = friendly_name.empty();

    // Calculate exact capacity for txt_records
    size_t txt_count = 3;  // version, mac, board (always present)
    if (!friendly_name_empty) {
      txt_count++;  // friendly_name
    }
#if defined(USE_ESP8266) || defined(USE_ESP32) || defined(USE_RP2040) || defined(USE_LIBRETINY)
    txt_count++;  // platform
#endif
#if defined(USE_WIFI) || defined(USE_ETHERNET) || defined(USE_OPENTHREAD)
    txt_count++;  // network
#endif
#ifdef USE_API_NOISE
    txt_count++;  // api_encryption or api_encryption_supported
#endif
#ifdef ESPHOME_PROJECT_NAME
    txt_count += 2;  // project_name and project_version
#endif
#ifdef USE_DASHBOARD_IMPORT
    txt_count++;  // package_import_url
#endif

    auto &txt_records = service.txt_records;
    txt_records.reserve(txt_count);

    if (!friendly_name_empty) {
      txt_records.emplace_back(MDNSTXTRecord{"friendly_name", friendly_name});
    }
    txt_records.emplace_back(MDNSTXTRecord{"version", ESPHOME_VERSION});
    txt_records.emplace_back(MDNSTXTRecord{"mac", get_mac_address()});

#ifdef USE_ESP8266
    txt_records.emplace_back(MDNSTXTRecord{"platform", "ESP8266"});
#elif defined(USE_ESP32)
    txt_records.emplace_back(MDNSTXTRecord{"platform", "ESP32"});
#elif defined(USE_RP2040)
    txt_records.emplace_back(MDNSTXTRecord{"platform", "RP2040"});
#elif defined(USE_LIBRETINY)
    txt_records.emplace_back(MDNSTXTRecord{"platform", lt_cpu_get_model_name()});
#endif

    txt_records.emplace_back(MDNSTXTRecord{"board", ESPHOME_BOARD});

#if defined(USE_WIFI)
    txt_records.emplace_back(MDNSTXTRecord{"network", "wifi"});
#elif defined(USE_ETHERNET)
    txt_records.emplace_back(MDNSTXTRecord{"network", "ethernet"});
#elif defined(USE_OPENTHREAD)
    txt_records.emplace_back(MDNSTXTRecord{"network", "thread"});
#endif

#ifdef USE_API_NOISE
    static constexpr const char *NOISE_ENCRYPTION = "Noise_NNpsk0_25519_ChaChaPoly_SHA256";
    if (api::global_api_server->get_noise_ctx()->has_psk()) {
      txt_records.emplace_back(MDNSTXTRecord{"api_encryption", NOISE_ENCRYPTION});
    } else {
      txt_records.emplace_back(MDNSTXTRecord{"api_encryption_supported", NOISE_ENCRYPTION});
    }
#endif

#ifdef ESPHOME_PROJECT_NAME
    txt_records.emplace_back(MDNSTXTRecord{"project_name", ESPHOME_PROJECT_NAME});
    txt_records.emplace_back(MDNSTXTRecord{"project_version", ESPHOME_PROJECT_VERSION});
#endif  // ESPHOME_PROJECT_NAME

#ifdef USE_DASHBOARD_IMPORT
    txt_records.emplace_back(MDNSTXTRecord{"package_import_url", dashboard_import::get_package_import_url()});
#endif
  }
#endif  // USE_API

#ifdef USE_PROMETHEUS
  this->services_.emplace_back();
  auto &prom_service = this->services_.back();
  prom_service.service_type = "_prometheus-http";
  prom_service.proto = "_tcp";
  prom_service.port = USE_WEBSERVER_PORT;
#endif

#ifdef USE_WEBSERVER
  this->services_.emplace_back();
  auto &web_service = this->services_.back();
  web_service.service_type = "_http";
  web_service.proto = "_tcp";
  web_service.port = USE_WEBSERVER_PORT;
#endif

#ifdef USE_MDNS_EXTRA_SERVICES
  this->services_.insert(this->services_.end(), this->services_extra_.begin(), this->services_extra_.end());
#endif

#if !defined(USE_API) && !defined(USE_PROMETHEUS) && !defined(USE_WEBSERVER) && !defined(USE_MDNS_EXTRA_SERVICES)
  // Publish "http" service if not using native API or any other services
  // This is just to have *some* mDNS service so that .local resolution works
  this->services_.emplace_back();
  auto &fallback_service = this->services_.back();
  fallback_service.service_type = "_http";
  fallback_service.proto = "_tcp";
  fallback_service.port = USE_WEBSERVER_PORT;
  fallback_service.txt_records.emplace_back(MDNSTXTRecord{"version", ESPHOME_VERSION});
#endif
}

void MDNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "mDNS:\n"
                "  Hostname: %s",
                this->hostname_.c_str());
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  ESP_LOGV(TAG, "  Services:");
  for (const auto &service : this->services_) {
    ESP_LOGV(TAG, "  - %s, %s, %d", service.service_type.c_str(), service.proto.c_str(),
             const_cast<TemplatableValue<uint16_t> &>(service.port).value());
    for (const auto &record : service.txt_records) {
      ESP_LOGV(TAG, "    TXT: %s = %s", record.key.c_str(),
               const_cast<TemplatableValue<std::string> &>(record.value).value().c_str());
    }
  }
#endif
}

std::vector<MDNSService> MDNSComponent::get_services() { return this->services_; }

}  // namespace mdns
}  // namespace esphome
#endif
