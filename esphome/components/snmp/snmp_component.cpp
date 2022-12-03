#include "snmp_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/components/wifi/wifi_component.h"

#include <SNMP_Agent.h>
#if USE_ESP32
#include <WiFi.h>
#include <esp_himem.h>
#endif
#if USE_ESP8266
#include <ESP8266WiFi.h>
#endif
#include <WiFiUdp.h>

// Integration test available: https://github.com/aquaticus/esphome_snmp_tests

namespace esphome {
namespace snmp {

#define CUSTOM_OID ".1.3.9999."

static WiFiUDP udp;
static SNMPAgent snmp_agent("public", "private");

static const char *const TAG = "snmp";

uint32_t SNMPComponent::get_net_uptime() {
  return (millis() - wifi::global_wifi_component->wifi_connected_timestamp()) / 10;
}

void SNMPComponent::setup_system_mib() {
  // sysDesc
  const char *desc_fmt = "ESPHome version " ESPHOME_VERSION " compiled %s, Board " ESPHOME_BOARD;
  char description[128];
  snprintf(description, sizeof(description), desc_fmt, App.get_compilation_time().c_str());
  snmp_agent.addReadOnlyStaticStringHandler(RFC1213_OID_sysDescr, description);

  // sysName
  snmp_agent.addDynamicReadOnlyStringHandler(RFC1213_OID_sysName, []() -> const std::string { return App.get_name(); });

  // sysServices
  snmp_agent.addReadOnlyIntegerHandler(RFC1213_OID_sysServices, 64 /*=2^(7-1) applications*/);

  // sysObjectID
  snmp_agent.addOIDHandler(RFC1213_OID_sysObjectID,
#if USE_ESP32
  CUSTOM_OID "32"
#else
  CUSTOM_OID "8266"
#endif
  );

  // sysContact
  snmp_agent.addReadOnlyStaticStringHandler(RFC1213_OID_sysContact, m_contact);

  // sysLocation
  snmp_agent.addReadOnlyStaticStringHandler(RFC1213_OID_sysLocation, m_location);
}

#if USE_ESP32
int SNMPComponent::setup_psram_size(int *used) {
  int size = 0;
  *used = 0;
  bool available = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
  if (available) {
    size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (size > 0) {
      *used = esp_himem_get_phys_size() - esp_himem_get_free_size();
    }
#endif

    return size;
  }
#endif

  void SNMPComponent::setup_storage_mib() {
    //  hrStorageIndex
    snmp_agent.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.1.1", 1);

    // hrStorageDesc
    snmp_agent.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.25.2.3.1.3.1", "FLASH");

    // hrAllocationUnit
    snmp_agent.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.4.1", 1);

    // hrStorageSize
    // static int flash_size = ESP.getFlashChipSize();
    snmp_agent.addDynamicIntegerHandler(".1.3.6.1.2.1.25.2.3.1.5.1", []() -> int { return ESP.getFlashChipSize(); });

    // hrStorageUsed
    snmp_agent.addDynamicIntegerHandler(".1.3.6.1.2.1.25.2.3.1.6.1", []() -> int { return ESP.getSketchSize(); });

    // SPI RAM

    //  hrStorageIndex
    snmp_agent.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.1.2", 2);

    // hrStorageDesc
    snmp_agent.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.25.2.3.1.3.2", "SPI RAM");

    // hrAllocationUnit
    snmp_agent.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.4.2", 1);

#if USE_ESP32
    // hrStorageSize
    snmp_agent.addDynamicIntegerHandler(".1.3.6.1.2.1.25.2.3.1.5.2", []() -> int {
      int u;
      return setup_psram_size(&u);
    });

    // hrStorageUsed
    snmp_agent.addDynamicIntegerHandler(".1.3.6.1.2.1.25.2.3.1.6.2", []() -> int {
      int u;
      setup_psram_size(&u);
      return u;
    });
#endif

#if USE_ESP8266
    // hrStorageSize
    snmp_agent.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.5.2", 0);

    // hrStorageUsed
    snmp_agent.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.6.2", 0);
#endif

    // hrMemorySize [kb]
#if USE_ESP32
    snmp_agent.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.2", get_ram_size_kb() );
#endif

#if USE_ESP8266
    snmp_agent.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.2", 160);
#endif
  }

  const std::string SNMPComponent::get_bssid() {
    char buf[30];
    wifi::bssid_t bssid = wifi::global_wifi_component->wifi_bssid();
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    return buf;
  }

#if USE_ESP32
  void SNMPComponent::setup_esp32_heap_mib() {
    // heap size
    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "32.1.0", []() -> int { return ESP.getHeapSize(); });

    // free heap
    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "32.2.0", []() -> int { return ESP.getFreeHeap(); });

    // min free heap
    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "32.3.0", []() -> int { return ESP.getMinFreeHeap(); });

    // max alloc heap
    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "32.4.0", []() -> int { return ESP.getMaxAllocHeap(); });
  }
#endif

#if USE_ESP8266
  void SNMPComponent::setup_esp8266_heap_mib() {
    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "8266.1.0", []() -> int { return ESP.getFreeHeap(); });

    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "8266.2.0", []() -> int { return ESP.getHeapFragmentation(); });

    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "8266.3.0", []() -> int { return ESP.getMaxFreeBlockSize(); });
  }
#endif

  void SNMPComponent::setup_chip_mib() {
    // esp32/ esp8266
#if ESP32
    snmp_agent.addReadOnlyIntegerHandler(CUSTOM_OID "2.1.0", 32);
#endif
#if ESP8266
    snmp_agent.addReadOnlyIntegerHandler(CUSTOM_OID "2.1.0", 8266);
#endif

    // CPU clock
    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "2.2.0", []() -> int { return ESP.getCpuFreqMHz(); });

    // chip model
#if ESP32
    snmp_agent.addDynamicReadOnlyStringHandler(CUSTOM_OID "2.3.0",
                                               []() -> const std::string { return ESP.getChipModel(); });
#endif
#if USE_ESP8266
    static std::string core = std::string(ESP.getCoreVersion().c_str());
    snmp_agent.addDynamicReadOnlyStringHandler(CUSTOM_OID "2.3.0", []() -> const std::string { return core; });
#endif

    // number of cores
#if USE_ESP32
    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "2.4.0", []() -> int { return ESP.getChipCores(); });
#endif
#if USE_ESP8266
    snmp_agent.addReadOnlyIntegerHandler(CUSTOM_OID "2.4.0", 1);
#endif

    // chip id
#if ESP32
    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "2.5.0", []() -> int { return ESP.getChipRevision(); });
#endif
#if ESP8266
    snmp_agent.addReadOnlyIntegerHandler(CUSTOM_OID "2.5.0", 0 /*no data for ESP8266*/);
#endif
  }

  void SNMPComponent::setup_wifi_mib() {
    // RSSI
    snmp_agent.addDynamicIntegerHandler(CUSTOM_OID "4.1.0",
                                        []() -> int { return wifi::global_wifi_component->wifi_rssi(); });

    // BSSID
    snmp_agent.addDynamicReadOnlyStringHandler(CUSTOM_OID "4.2.0", get_bssid);

    // SSID
    snmp_agent.addDynamicReadOnlyStringHandler(
        CUSTOM_OID "4.3.0", []() -> const std::string { return wifi::global_wifi_component->wifi_ssid(); });

    // IP
    snmp_agent.addDynamicReadOnlyStringHandler(
        CUSTOM_OID "4.4.0", []() -> const std::string { return wifi::global_wifi_component->wifi_sta_ip().str(); });
  }

  void SNMPComponent::setup() {
    ESP_LOGCONFIG(TAG, "Setting up SNMP...");

    // sysUpTime
    // this is uptime of network management part of the system
    snmp_agent.addDynamicReadOnlyTimestampHandler(RFC1213_OID_sysUpTime, get_net_uptime);

    // hrSystemUptime
    snmp_agent.addDynamicReadOnlyTimestampHandler(".1.3.6.1.2.1.25.1.1.0", get_uptime);

    setup_system_mib();
    setup_storage_mib();
#if USE_ESP32
    setup_esp32_heap_mib();
#endif
#if USE_ESP8266
    setup_esp8266_heap_mib();
#endif
    setup_chip_mib();
    setup_wifi_mib();

    snmp_agent.sortHandlers();  // for walk to work properly

    snmp_agent.setUDP(&udp);
    snmp_agent.begin();
  }

  void SNMPComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "SNMP Config:");
    ESP_LOGCONFIG(TAG, "  Contact: \"%s\"", m_contact.c_str());
    ESP_LOGCONFIG(TAG, "  Location: \"%s\"", m_location.c_str());
  }

  void SNMPComponent::loop() { snmp_agent.loop(); }

#if USE_ESP32
  int SNMPComponent::get_ram_size_kb() {
    // use hardcoded values (number of values in esp_chip_model_t depends on IDF version)
    // from esp_system.h
    const int CHIP_ESP32  = 1;
    const int CHIP_ESP32S2 = 2;
    const int CHIP_ESP32S3 = 9;
    const int CHIP_ESP32C3 = 5;
    const int CHIP_ESP32H2 = 6;
    const int CHIP_ESP32C2 = 12;
    const int CHIP_ESP32C6 = 13;

    esp_chip_model_t model;
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    switch((int)chip_info.model)
    {
      case CHIP_ESP32:
        return 520;

      case CHIP_ESP32S2:
        return 320;

      case CHIP_ESP32S3:
        return 512;

      case CHIP_ESP32C3:
        return 400;

      case CHIP_ESP32H2:
        return 256;

      case CHIP_ESP32C2:
        return 400;

      case CHIP_ESP32C6:
        return 400;
    }

    return 0;
  }
#endif

}  // namespace snmp
}  // namespace esphome
