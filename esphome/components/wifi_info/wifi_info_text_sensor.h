#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/wifi/wifi_component.h"
#ifdef USE_WIFI
#include <array>

namespace esphome {
namespace wifi_info {

static constexpr size_t MAX_STATE_LENGTH = 255;

class IPAddressWiFiInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    auto ips = wifi::global_wifi_component->wifi_sta_ip_addresses();
    if (ips != this->last_ips_) {
      this->last_ips_ = ips;
      this->publish_state(ips[0].str());
      uint8_t sensor = 0;
      for (auto &ip : ips) {
        if (ip.is_set()) {
          if (this->ip_sensors_[sensor] != nullptr) {
            this->ip_sensors_[sensor]->publish_state(ip.str());
          }
          sensor++;
        }
      }
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;
  void add_ip_sensors(uint8_t index, text_sensor::TextSensor *s) { this->ip_sensors_[index] = s; }

 protected:
  network::IPAddresses last_ips_;
  std::array<text_sensor::TextSensor *, 5> ip_sensors_;
};

class DNSAddressWifiInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    auto dns_one = wifi::global_wifi_component->get_dns_address(0);
    auto dns_two = wifi::global_wifi_component->get_dns_address(1);

    std::string dns_results = dns_one.str() + " " + dns_two.str();

    if (dns_results != this->last_results_) {
      this->last_results_ = dns_results;
      this->publish_state(dns_results);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  std::string last_results_;
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

    // There's a limit of 255 characters per state.
    // Longer states just don't get sent so we truncate it.
    if (scan_results.length() > MAX_STATE_LENGTH) {
      scan_results.resize(MAX_STATE_LENGTH);
    }
    if (this->last_scan_results_ != scan_results) {
      this->last_scan_results_ = scan_results;
      this->publish_state(scan_results);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  std::string last_scan_results_;
};

class SSIDWiFiInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    std::string ssid = wifi::global_wifi_component->wifi_ssid();
    if (this->last_ssid_ != ssid) {
      this->last_ssid_ = ssid;
      this->publish_state(this->last_ssid_);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  std::string last_ssid_;
};

class BSSIDWiFiInfo : public PollingComponent, public text_sensor::TextSensor {
 public:
  void update() override {
    wifi::bssid_t bssid = wifi::global_wifi_component->wifi_bssid();
    if (memcmp(bssid.data(), last_bssid_.data(), 6) != 0) {
      std::copy(bssid.begin(), bssid.end(), last_bssid_.begin());
      char buf[18];
      format_mac_addr_upper(bssid.data(), buf);
      this->publish_state(buf);
    }
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void dump_config() override;

 protected:
  wifi::bssid_t last_bssid_;
};

class MacAddressWifiInfo : public Component, public text_sensor::TextSensor {
 public:
  void setup() override { this->publish_state(get_mac_address_pretty()); }
  void dump_config() override;
};

}  // namespace wifi_info
}  // namespace esphome
#endif
