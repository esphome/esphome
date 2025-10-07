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

// Define constant strings for values (PROGMEM on ESP8266, static pointers on others)
MDNS_STATIC_CONST_CHAR(PLATFORM_ESP8266, "ESP8266");
MDNS_STATIC_CONST_CHAR(PLATFORM_ESP32, "ESP32");
MDNS_STATIC_CONST_CHAR(PLATFORM_RP2040, "RP2040");

MDNS_STATIC_CONST_CHAR(NETWORK_WIFI, "wifi");
MDNS_STATIC_CONST_CHAR(NETWORK_ETHERNET, "ethernet");
MDNS_STATIC_CONST_CHAR(NETWORK_THREAD, "thread");

void MDNSComponent::compile_records_() {
  this->hostname_ = App.get_name();

  // IMPORTANT: The #ifdef blocks below must match COMPONENTS_WITH_MDNS_SERVICES
  // in mdns/__init__.py. If you add a new service here, update both locations.

#ifdef USE_API
  if (api::global_api_server != nullptr) {
    auto &service = this->services_.emplace_next();
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
      txt_records.push_back({"friendly_name", friendly_name});
    }
    txt_records.push_back({"version", ESPHOME_VERSION});
    txt_records.push_back({"mac", get_mac_address()});

#ifdef USE_ESP8266
    txt_records.push_back({"platform", MDNS_STR(PLATFORM_ESP8266)});
#elif defined(USE_ESP32)
    txt_records.push_back({"platform", MDNS_STR(PLATFORM_ESP32)});
#elif defined(USE_RP2040)
    txt_records.push_back({"platform", MDNS_STR(PLATFORM_RP2040)});
#elif defined(USE_LIBRETINY)
    txt_records.emplace_back(MDNSTXTRecord{"platform", lt_cpu_get_model_name()});
#endif

    txt_records.push_back({"board", ESPHOME_BOARD});

#if defined(USE_WIFI)
    txt_records.push_back({"network", MDNS_STR(NETWORK_WIFI)});
#elif defined(USE_ETHERNET)
    txt_records.push_back({"network", MDNS_STR(NETWORK_ETHERNET)});
#elif defined(USE_OPENTHREAD)
    txt_records.push_back({"network", MDNS_STR(NETWORK_THREAD)});
#endif

#ifdef USE_API_NOISE
    MDNS_STATIC_CONST_CHAR(NOISE_ENCRYPTION, "Noise_NNpsk0_25519_ChaChaPoly_SHA256");
    if (api::global_api_server->get_noise_ctx()->has_psk()) {
      txt_records.push_back({"api_encryption", MDNS_STR(NOISE_ENCRYPTION)});
    } else {
      txt_records.push_back({"api_encryption_supported", MDNS_STR(NOISE_ENCRYPTION)});
    }
#endif

#ifdef ESPHOME_PROJECT_NAME
    txt_records.push_back({"project_name", ESPHOME_PROJECT_NAME});
    txt_records.push_back({"project_version", ESPHOME_PROJECT_VERSION});
#endif  // ESPHOME_PROJECT_NAME

#ifdef USE_DASHBOARD_IMPORT
    txt_records.push_back({"package_import_url", dashboard_import::get_package_import_url()});
#endif
  }
#endif  // USE_API

#ifdef USE_PROMETHEUS
  auto &prom_service = this->services_.emplace_next();
  prom_service.service_type = "_prometheus-http";
  prom_service.proto = "_tcp";
  prom_service.port = USE_WEBSERVER_PORT;
#endif

#ifdef USE_WEBSERVER
  auto &web_service = this->services_.emplace_next();
  web_service.service_type = "_http";
  web_service.proto = "_tcp";
  web_service.port = USE_WEBSERVER_PORT;
#endif

#if !defined(USE_API) && !defined(USE_PROMETHEUS) && !defined(USE_WEBSERVER) && !defined(USE_MDNS_EXTRA_SERVICES)
  // Publish "http" service if not using native API or any other services
  // This is just to have *some* mDNS service so that .local resolution works
  auto &fallback_service = this->services_.emplace_next();
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
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "  Services:");
  for (const auto &service : this->services_) {
    ESP_LOGV(TAG, "  - %s, %s, %d", service.service_type, service.proto,
             const_cast<TemplatableValue<uint16_t> &>(service.port).value());
    for (const auto &record : service.txt_records) {
      ESP_LOGV(TAG, "    TXT: %s = %s", record.key,
               const_cast<TemplatableValue<std::string> &>(record.value).value().c_str());
    }
  }
#endif
}

}  // namespace mdns
}  // namespace esphome
#endif
