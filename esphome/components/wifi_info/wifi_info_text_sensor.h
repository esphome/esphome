#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/wifi/wifi_component.h"

namespace esphome {
namespace wifi_info {

class IPAddressWiFiInfo : public Component, public text_sensor::TextSensor {
 public:
  void loop() override {
    auto ip = wifi::global_wifi_component->wifi_sta_ip();
    if (ip != this->last_ip_) {
      this->last_ip_ = ip;
      this->publish_state(ip.str());
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  std::string unique_id() override { return get_mac_address() + "-wifiinfo-ip"; }
  void dump_config() override;

 protected:
  network::IPAddress last_ip_;
};

class ScanResultsWiFiInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    std::string scan_results;
    for (auto &scan : wifi::global_wifi_component->get_scan_result()) {
      if (scan.get_is_hidden())
        continue;

      scan_results += scan.get_ssid();
      scan_results += ": ";
      scan_results += esphome::to_string(scan.get_rssi());
      scan_results += "dB\n";
    }

    if (this->last_scan_results_ != scan_results) {
      this->last_scan_results_ = scan_results;
      // There's a limit of 255 characters per state.
      // Longer states just don't get sent so we truncate it.
      this->publish_state(scan_results.substr(0, 255));
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  std::string unique_id() override { return get_mac_address() + "-wifiinfo-scanresults"; }
  void dump_config() override;

 protected:
  std::string last_scan_results_;
};

class SSIDWiFiInfo : public Component, public text_sensor::TextSensor {
 public:
  void loop() override {
    std::string ssid = wifi::global_wifi_component->wifi_ssid();
    if (this->last_ssid_ != ssid) {
      this->last_ssid_ = ssid;
      this->publish_state(this->last_ssid_);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  std::string unique_id() override { return get_mac_address() + "-wifiinfo-ssid"; }
  void dump_config() override;

 protected:
  std::string last_ssid_;
};

class BSSIDWiFiInfo : public Component, public text_sensor::TextSensor {
 public:
  void loop() override {
    wifi::bssid_t bssid = wifi::global_wifi_component->wifi_bssid();
    if (memcmp(bssid.data(), last_bssid_.data(), 6) != 0) {
      std::copy(bssid.begin(), bssid.end(), last_bssid_.begin());
      char buf[30];
      sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
      this->publish_state(buf);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  std::string unique_id() override { return get_mac_address() + "-wifiinfo-bssid"; }
  void dump_config() override;

 protected:
  wifi::bssid_t last_bssid_;
};

class MacAddressWifiInfo : public Component, public text_sensor::TextSensor {
 public:
  void setup() override { this->publish_state(get_mac_address_pretty()); }
  std::string unique_id() override { return get_mac_address() + "-wifiinfo-macadr"; }
  void dump_config() override;
};

}  // namespace wifi_info
}  // namespace esphome
