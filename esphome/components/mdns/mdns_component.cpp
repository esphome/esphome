#include "esphome/core/defines.h"
#ifdef USE_MDNS
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "mdns_component.h"

#ifdef USE_ESP8266
#include <pgmspace.h>
// Macro to define strings in PROGMEM on ESP8266, regular memory on other platforms
#define MDNS_STATIC_CONST_CHAR(name, value) static const char name[] PROGMEM = value
// Helper to get string from PROGMEM - returns a temporary std::string
// Only define this function if we have services that will use it
#if defined(USE_API) || defined(USE_PROMETHEUS) || defined(USE_WEBSERVER) || defined(USE_MDNS_EXTRA_SERVICES)
static std::string mdns_string_p(const char *src) {
  char buf[64];
  strncpy_P(buf, src, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  return std::string(buf);
}
#define MDNS_STR(name) mdns_string_p(name)
#else
// If no services are configured, we still need the fallback service but it uses string literals
#define MDNS_STR(name) std::string(name)
#endif
#else
// On non-ESP8266 platforms, use regular const char*
#define MDNS_STATIC_CONST_CHAR(name, value) static constexpr const char *name = value
#define MDNS_STR(name) name
#endif

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

// Define all constant strings using the macro
MDNS_STATIC_CONST_CHAR(SERVICE_ESPHOMELIB, "_esphomelib");
MDNS_STATIC_CONST_CHAR(SERVICE_TCP, "_tcp");
MDNS_STATIC_CONST_CHAR(SERVICE_PROMETHEUS, "_prometheus-http");
MDNS_STATIC_CONST_CHAR(SERVICE_HTTP, "_http");

MDNS_STATIC_CONST_CHAR(TXT_FRIENDLY_NAME, "friendly_name");
MDNS_STATIC_CONST_CHAR(TXT_VERSION, "version");
MDNS_STATIC_CONST_CHAR(TXT_MAC, "mac");
MDNS_STATIC_CONST_CHAR(TXT_PLATFORM, "platform");
MDNS_STATIC_CONST_CHAR(TXT_BOARD, "board");
MDNS_STATIC_CONST_CHAR(TXT_NETWORK, "network");
MDNS_STATIC_CONST_CHAR(TXT_API_ENCRYPTION, "api_encryption");
MDNS_STATIC_CONST_CHAR(TXT_API_ENCRYPTION_SUPPORTED, "api_encryption_supported");
MDNS_STATIC_CONST_CHAR(TXT_PROJECT_NAME, "project_name");
MDNS_STATIC_CONST_CHAR(TXT_PROJECT_VERSION, "project_version");
MDNS_STATIC_CONST_CHAR(TXT_PACKAGE_IMPORT_URL, "package_import_url");

MDNS_STATIC_CONST_CHAR(PLATFORM_ESP8266, "ESP8266");
MDNS_STATIC_CONST_CHAR(PLATFORM_ESP32, "ESP32");
MDNS_STATIC_CONST_CHAR(PLATFORM_RP2040, "RP2040");

MDNS_STATIC_CONST_CHAR(NETWORK_WIFI, "wifi");
MDNS_STATIC_CONST_CHAR(NETWORK_ETHERNET, "ethernet");
MDNS_STATIC_CONST_CHAR(NETWORK_THREAD, "thread");

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
    service.service_type = MDNS_STR(SERVICE_ESPHOMELIB);
    service.proto = MDNS_STR(SERVICE_TCP);
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
      txt_records.push_back({MDNS_STR(TXT_FRIENDLY_NAME), friendly_name});
    }
    txt_records.push_back({MDNS_STR(TXT_VERSION), ESPHOME_VERSION});
    txt_records.push_back({MDNS_STR(TXT_MAC), get_mac_address()});

#ifdef USE_ESP8266
    txt_records.push_back({MDNS_STR(TXT_PLATFORM), MDNS_STR(PLATFORM_ESP8266)});
#elif defined(USE_ESP32)
    txt_records.push_back({MDNS_STR(TXT_PLATFORM), MDNS_STR(PLATFORM_ESP32)});
#elif defined(USE_RP2040)
    txt_records.push_back({MDNS_STR(TXT_PLATFORM), MDNS_STR(PLATFORM_RP2040)});
#elif defined(USE_LIBRETINY)
    txt_records.emplace_back(MDNSTXTRecord{"platform", lt_cpu_get_model_name()});
#endif

    txt_records.push_back({MDNS_STR(TXT_BOARD), ESPHOME_BOARD});

#if defined(USE_WIFI)
    txt_records.push_back({MDNS_STR(TXT_NETWORK), MDNS_STR(NETWORK_WIFI)});
#elif defined(USE_ETHERNET)
    txt_records.push_back({MDNS_STR(TXT_NETWORK), MDNS_STR(NETWORK_ETHERNET)});
#elif defined(USE_OPENTHREAD)
    txt_records.push_back({MDNS_STR(TXT_NETWORK), MDNS_STR(NETWORK_THREAD)});
#endif

#ifdef USE_API_NOISE
    MDNS_STATIC_CONST_CHAR(NOISE_ENCRYPTION, "Noise_NNpsk0_25519_ChaChaPoly_SHA256");
    if (api::global_api_server->get_noise_ctx()->has_psk()) {
      txt_records.push_back({MDNS_STR(TXT_API_ENCRYPTION), MDNS_STR(NOISE_ENCRYPTION)});
    } else {
      txt_records.push_back({MDNS_STR(TXT_API_ENCRYPTION_SUPPORTED), MDNS_STR(NOISE_ENCRYPTION)});
    }
#endif

#ifdef ESPHOME_PROJECT_NAME
    txt_records.push_back({MDNS_STR(TXT_PROJECT_NAME), ESPHOME_PROJECT_NAME});
    txt_records.push_back({MDNS_STR(TXT_PROJECT_VERSION), ESPHOME_PROJECT_VERSION});
#endif  // ESPHOME_PROJECT_NAME

#ifdef USE_DASHBOARD_IMPORT
    txt_records.push_back({MDNS_STR(TXT_PACKAGE_IMPORT_URL), dashboard_import::get_package_import_url()});
#endif
  }
#endif  // USE_API

#ifdef USE_PROMETHEUS
  this->services_.emplace_back();
  auto &prom_service = this->services_.back();
  prom_service.service_type = MDNS_STR(SERVICE_PROMETHEUS);
  prom_service.proto = MDNS_STR(SERVICE_TCP);
  prom_service.port = USE_WEBSERVER_PORT;
#endif

#ifdef USE_WEBSERVER
  this->services_.emplace_back();
  auto &web_service = this->services_.back();
  web_service.service_type = MDNS_STR(SERVICE_HTTP);
  web_service.proto = MDNS_STR(SERVICE_TCP);
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
