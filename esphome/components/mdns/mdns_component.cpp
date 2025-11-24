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
#else
// On non-ESP8266 platforms, use regular const char*
#define MDNS_STATIC_CONST_CHAR(name, value) static constexpr const char name[] = value
#endif

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_DASHBOARD_IMPORT
#include "esphome/components/dashboard_import/dashboard_import.h"
#endif

namespace esphome::mdns {

static const char *const TAG = "mdns";

#ifndef USE_WEBSERVER_PORT
#define USE_WEBSERVER_PORT 80  // NOLINT
#endif

// Define all constant strings using the macro
MDNS_STATIC_CONST_CHAR(SERVICE_TCP, "_tcp");

// Wrap build-time defines into flash storage
MDNS_STATIC_CONST_CHAR(VALUE_VERSION, ESPHOME_VERSION);

void MDNSComponent::compile_records_(StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services, char *mac_address_buf) {
  // IMPORTANT: The #ifdef blocks below must match COMPONENTS_WITH_MDNS_SERVICES
  // in mdns/__init__.py. If you add a new service here, update both locations.

#ifdef USE_API
  MDNS_STATIC_CONST_CHAR(SERVICE_ESPHOMELIB, "_esphomelib");
  MDNS_STATIC_CONST_CHAR(TXT_FRIENDLY_NAME, "friendly_name");
  MDNS_STATIC_CONST_CHAR(TXT_VERSION, "version");
  MDNS_STATIC_CONST_CHAR(TXT_MAC, "mac");
  MDNS_STATIC_CONST_CHAR(TXT_PLATFORM, "platform");
  MDNS_STATIC_CONST_CHAR(TXT_BOARD, "board");
  MDNS_STATIC_CONST_CHAR(TXT_NETWORK, "network");
  MDNS_STATIC_CONST_CHAR(VALUE_BOARD, ESPHOME_BOARD);

  if (api::global_api_server != nullptr) {
    auto &service = services.emplace_next();
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
    txt_records.init(txt_count);

    if (!friendly_name_empty) {
      txt_records.push_back({MDNS_STR(TXT_FRIENDLY_NAME), MDNS_STR(friendly_name.c_str())});
    }
    txt_records.push_back({MDNS_STR(TXT_VERSION), MDNS_STR(VALUE_VERSION)});

    // MAC address: passed from caller (either member buffer or stack buffer depending on USE_MDNS_STORE_SERVICES)
    txt_records.push_back({MDNS_STR(TXT_MAC), MDNS_STR(mac_address_buf)});

#ifdef USE_ESP8266
    MDNS_STATIC_CONST_CHAR(PLATFORM_ESP8266, "ESP8266");
    txt_records.push_back({MDNS_STR(TXT_PLATFORM), MDNS_STR(PLATFORM_ESP8266)});
#elif defined(USE_ESP32)
    MDNS_STATIC_CONST_CHAR(PLATFORM_ESP32, "ESP32");
    txt_records.push_back({MDNS_STR(TXT_PLATFORM), MDNS_STR(PLATFORM_ESP32)});
#elif defined(USE_RP2040)
    MDNS_STATIC_CONST_CHAR(PLATFORM_RP2040, "RP2040");
    txt_records.push_back({MDNS_STR(TXT_PLATFORM), MDNS_STR(PLATFORM_RP2040)});
#elif defined(USE_LIBRETINY)
    txt_records.push_back({MDNS_STR(TXT_PLATFORM), MDNS_STR(lt_cpu_get_model_name())});
#endif

    txt_records.push_back({MDNS_STR(TXT_BOARD), MDNS_STR(VALUE_BOARD)});

#if defined(USE_WIFI)
    MDNS_STATIC_CONST_CHAR(NETWORK_WIFI, "wifi");
    txt_records.push_back({MDNS_STR(TXT_NETWORK), MDNS_STR(NETWORK_WIFI)});
#elif defined(USE_ETHERNET)
    MDNS_STATIC_CONST_CHAR(NETWORK_ETHERNET, "ethernet");
    txt_records.push_back({MDNS_STR(TXT_NETWORK), MDNS_STR(NETWORK_ETHERNET)});
#elif defined(USE_OPENTHREAD)
    MDNS_STATIC_CONST_CHAR(NETWORK_THREAD, "thread");
    txt_records.push_back({MDNS_STR(TXT_NETWORK), MDNS_STR(NETWORK_THREAD)});
#endif

#ifdef USE_API_NOISE
    MDNS_STATIC_CONST_CHAR(TXT_API_ENCRYPTION, "api_encryption");
    MDNS_STATIC_CONST_CHAR(TXT_API_ENCRYPTION_SUPPORTED, "api_encryption_supported");
    MDNS_STATIC_CONST_CHAR(NOISE_ENCRYPTION, "Noise_NNpsk0_25519_ChaChaPoly_SHA256");
    bool has_psk = api::global_api_server->get_noise_ctx().has_psk();
    const char *encryption_key = has_psk ? TXT_API_ENCRYPTION : TXT_API_ENCRYPTION_SUPPORTED;
    txt_records.push_back({MDNS_STR(encryption_key), MDNS_STR(NOISE_ENCRYPTION)});
#endif

#ifdef ESPHOME_PROJECT_NAME
    MDNS_STATIC_CONST_CHAR(TXT_PROJECT_NAME, "project_name");
    MDNS_STATIC_CONST_CHAR(TXT_PROJECT_VERSION, "project_version");
    MDNS_STATIC_CONST_CHAR(VALUE_PROJECT_NAME, ESPHOME_PROJECT_NAME);
    MDNS_STATIC_CONST_CHAR(VALUE_PROJECT_VERSION, ESPHOME_PROJECT_VERSION);
    txt_records.push_back({MDNS_STR(TXT_PROJECT_NAME), MDNS_STR(VALUE_PROJECT_NAME)});
    txt_records.push_back({MDNS_STR(TXT_PROJECT_VERSION), MDNS_STR(VALUE_PROJECT_VERSION)});
#endif  // ESPHOME_PROJECT_NAME

#ifdef USE_DASHBOARD_IMPORT
    MDNS_STATIC_CONST_CHAR(TXT_PACKAGE_IMPORT_URL, "package_import_url");
    txt_records.push_back({MDNS_STR(TXT_PACKAGE_IMPORT_URL), MDNS_STR(dashboard_import::get_package_import_url())});
#endif
  }
#endif  // USE_API

#ifdef USE_PROMETHEUS
  MDNS_STATIC_CONST_CHAR(SERVICE_PROMETHEUS, "_prometheus-http");

  auto &prom_service = services.emplace_next();
  prom_service.service_type = MDNS_STR(SERVICE_PROMETHEUS);
  prom_service.proto = MDNS_STR(SERVICE_TCP);
  prom_service.port = USE_WEBSERVER_PORT;
#endif

#ifdef USE_WEBSERVER
  MDNS_STATIC_CONST_CHAR(SERVICE_HTTP, "_http");

  auto &web_service = services.emplace_next();
  web_service.service_type = MDNS_STR(SERVICE_HTTP);
  web_service.proto = MDNS_STR(SERVICE_TCP);
  web_service.port = USE_WEBSERVER_PORT;
#endif

#if !defined(USE_API) && !defined(USE_PROMETHEUS) && !defined(USE_WEBSERVER) && !defined(USE_MDNS_EXTRA_SERVICES)
  MDNS_STATIC_CONST_CHAR(SERVICE_HTTP, "_http");
  MDNS_STATIC_CONST_CHAR(TXT_VERSION, "version");

  // Publish "http" service if not using native API or any other services
  // This is just to have *some* mDNS service so that .local resolution works
  auto &fallback_service = services.emplace_next();
  fallback_service.service_type = MDNS_STR(SERVICE_HTTP);
  fallback_service.proto = MDNS_STR(SERVICE_TCP);
  fallback_service.port = USE_WEBSERVER_PORT;
  fallback_service.txt_records = {{MDNS_STR(TXT_VERSION), MDNS_STR(VALUE_VERSION)}};
#endif
}

void MDNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "mDNS:\n"
                "  Hostname: %s",
                App.get_name().c_str());
#ifdef USE_MDNS_STORE_SERVICES
  ESP_LOGV(TAG, "  Services:");
  for (const auto &service : this->services_) {
    ESP_LOGV(TAG, "  - %s, %s, %d", MDNS_STR_ARG(service.service_type), MDNS_STR_ARG(service.proto),
             const_cast<TemplatableValue<uint16_t> &>(service.port).value());
    for (const auto &record : service.txt_records) {
      ESP_LOGV(TAG, "    TXT: %s = %s", MDNS_STR_ARG(record.key), MDNS_STR_ARG(record.value));
    }
  }
#endif
}

}  // namespace esphome::mdns
#endif
