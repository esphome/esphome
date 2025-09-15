#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/wifi/wifi_component.h"
#ifdef USE_WIFI
#include <array>

namespace esphome {
namespace wifi_info {

class IPAddressWiFiInfo : public Component, public text_sensor::TextSensor {
 public:
  void setup() override;
  void dump_config() override;
  void add_ip_sensors(uint8_t index, text_sensor::TextSensor *s) { this->ip_sensors_[index] = s; }

 protected:
  void state_callback_(network::IPAddresses ips);
  std::array<text_sensor::TextSensor *, 5> ip_sensors_;
};

class DNSAddressWifiInfo : public Component, public text_sensor::TextSensor {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void state_callback_(network::IPAddress dns1_ip, network::IPAddress dns2_ip);
};

class ScanResultsWiFiInfo : public Component, public text_sensor::TextSensor {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void state_callback_(const std::vector<wifi::WiFiScanResult> &results);
};

class SSIDWiFiInfo : public Component, public text_sensor::TextSensor {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void state_callback_(std::string &ssid);
};

class BSSIDWiFiInfo : public Component, public text_sensor::TextSensor {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void state_callback_(wifi::bssid_t bssid);
};

class MacAddressWifiInfo : public Component, public text_sensor::TextSensor {
 public:
  void setup() override { this->publish_state(get_mac_address_pretty()); }
  void dump_config() override;
};

}  // namespace wifi_info
}  // namespace esphome
#endif
