#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/wifi/wifi_component.h"
#ifdef USE_WIFI
#include <array>

namespace esphome::wifi_info {

#ifdef USE_WIFI_LISTENERS
class IPAddressWiFiInfo final : public Component, public text_sensor::TextSensor, public wifi::WiFiIPStateListener {
 public:
  void setup() override;
  void dump_config() override;
  void add_ip_sensors(uint8_t index, text_sensor::TextSensor *s) { this->ip_sensors_[index] = s; }

  // WiFiIPStateListener interface
  void on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                   const network::IPAddress &dns2) override;

 protected:
  std::array<text_sensor::TextSensor *, 5> ip_sensors_;
};

class DNSAddressWifiInfo final : public Component, public text_sensor::TextSensor, public wifi::WiFiIPStateListener {
 public:
  void setup() override;
  void dump_config() override;

  // WiFiIPStateListener interface
  void on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                   const network::IPAddress &dns2) override;
};

class ScanResultsWiFiInfo final : public Component,
                                  public text_sensor::TextSensor,
                                  public wifi::WiFiScanResultsListener {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

  // WiFiScanResultsListener interface
  void on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results) override;
};

class SSIDWiFiInfo final : public Component, public text_sensor::TextSensor, public wifi::WiFiConnectStateListener {
 public:
  void setup() override;
  void dump_config() override;

  // WiFiConnectStateListener interface
  void on_wifi_connect_state(const std::string &ssid, const wifi::bssid_t &bssid) override;
};

class BSSIDWiFiInfo final : public Component, public text_sensor::TextSensor, public wifi::WiFiConnectStateListener {
 public:
  void setup() override;
  void dump_config() override;

  // WiFiConnectStateListener interface
  void on_wifi_connect_state(const std::string &ssid, const wifi::bssid_t &bssid) override;
};

class PowerSaveModeWiFiInfo final : public Component,
                                    public text_sensor::TextSensor,
                                    public wifi::WiFiPowerSaveListener {
 public:
  void setup() override;
  void dump_config() override;

  // WiFiPowerSaveListener interface
  void on_wifi_power_save(wifi::WiFiPowerSaveMode mode) override;
};
#endif

class MacAddressWifiInfo final : public Component, public text_sensor::TextSensor {
 public:
  void setup() override {
    char mac_s[18];
    this->publish_state(get_mac_address_pretty_into_buffer(mac_s));
  }
  void dump_config() override;
};

}  // namespace esphome::wifi_info
#endif
