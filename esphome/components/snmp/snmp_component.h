#pragma once

#include <string>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace snmp {

/// The SNMP (Simple Network Management Protocol) component provides support for collecting and organizing
/// information about managed devices on a networks.

class SNMPComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void loop() override;

  void set_contact(const std::string& contact) {
    m_contact = contact;
  }

  void set_location(const std::string& location) {
    m_location = location;
  }

 protected:
  void setup_system_mib();
  void setup_storage_mib();
#if USE_ESP32
  void setup_esp32_heap_mib();
#endif
#if USE_ESP8266
  void setup_esp8266_heap_mib();
#endif
  void setup_chip_mib();
  void setup_wifi_mib();
#if USE_ESP32
  static int setup_psram_size(int* used);
#endif

  static uint32_t get_uptime() {
    return millis()/10;
  }

  static uint32_t get_net_uptime();

  static const std::string get_bssid();

#if USE_ESP32
  static int get_ram_size_kb();
#endif

  /// contact stringint
  std::string m_contact;

  /// location string
  std::string m_location;
};

}  // namespace snmp
}  // namespace esphome
