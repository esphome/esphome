#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/wifi/wifi_component.h"

namespace esphome {
namespace wifi_info {

class IPAddressWiFiInfo : public Component, public text_sensor::TextSensor {
public:
  void loop() override {
    IPAddress ip = WiFi.localIP();
    if (ip != this->last_ip_) {
      this->last_ip_ = ip;
      this->publish_state(ip.toString().c_str());
    }
  }
  float get_setup_priority() const override {
    return setup_priority::AFTER_WIFI;
  }
  std::string unique_id() override {
    return get_mac_address() + "-wifiinfo-ip";
  }
  void dump_config() override;

protected:
  IPAddress last_ip_;
};

class SSIDWiFiInfo : public Component, public text_sensor::TextSensor {
public:
  void loop() override {
    String ssid = WiFi.SSID();
    if (this->last_ssid_ != ssid.c_str()) {
      this->last_ssid_ = std::string(ssid.c_str());
      this->publish_state(this->last_ssid_);
    }
  }
  float get_setup_priority() const override {
    return setup_priority::AFTER_WIFI;
  }
  std::string unique_id() override {
    return get_mac_address() + "-wifiinfo-ssid";
  }
  void dump_config() override;

protected:
  std::string last_ssid_;
};

class BSSIDWiFiInfo : public Component, public text_sensor::TextSensor {
public:
  void loop() override {
    uint8_t *bssid = WiFi.BSSID();
    if (memcmp(bssid, this->last_bssid_.data(), 6) != 0) {
      std::copy(bssid, bssid + 6, this->last_bssid_.data());
      char buf[30];
      sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1],
              bssid[2], bssid[3], bssid[4], bssid[5]);
      this->publish_state(buf);
    }
  }
  float get_setup_priority() const override {
    return setup_priority::AFTER_WIFI;
  }
  std::string unique_id() override {
    return get_mac_address() + "-wifiinfo-bssid";
  }
  void dump_config() override;

protected:
  wifi::bssid_t last_bssid_;
};

class MacAddressWifiInfo : public Component, public text_sensor::TextSensor {
public:
  void setup() override { this->publish_state(get_mac_address_pretty()); }
  std::string unique_id() override {
    return get_mac_address() + "-wifiinfo-macadr";
  }
  void dump_config() override;
};

class NetworksWifiInfo : public Component, public text_sensor::TextSensor {
public:
  void loop() override {
    // Parse network data only once when a connection is established
    if (is_connected_ == wifi::global_wifi_component->is_connected())
      return;
    is_connected_ = wifi::global_wifi_component->is_connected();

    auto scan_result = wifi::global_wifi_component->get_scan_result();
    // No results found.
    if (scan_result.empty())
      return;

    // Compact network data & limit total string length to 255 chars - HA text length limit
    char buf[256]{'\0'};

    for (auto &scan : scan_result) {
      char buf1[32]{'\0'};
      bool bssid_match = memcmp(scan.get_bssid().data(), WiFi.BSSID(), 6) == 0;
      sprintf(buf1, " %d %02X%02X%02X%02X%02X%02X %d %X %.1f", scan.get_rssi(),
              scan.get_bssid()[0], scan.get_bssid()[1], scan.get_bssid()[2],
              scan.get_bssid()[3], scan.get_bssid()[4], scan.get_bssid()[5],
              scan.get_channel(),
              scan.get_with_auth() + 2 * scan.get_is_hidden() +
                  4 * scan.get_matches() + 8 * bssid_match,
              scan.get_priority());

      int len = strlen(buf);
      if (len + strlen(scan.get_ssid().data()) + strlen(buf1) > 255) {
        // No room in published state for more data
        break;
      }

      if (len > 0) {
        buf[len++] = '|';
      }
      strcpy(buf + len, scan.get_ssid().data());
      len += strlen(scan.get_ssid().data());
      strcpy(buf + len, buf1);
    }

    char cs{0};
    for (auto x : buf)
      cs += x;

    if (this->cs_ != cs) {
      this->cs_ = cs;
      this->publish_state(buf);
    }
  }

  float get_setup_priority() const override {
    return setup_priority::AFTER_WIFI;
  }
  std::string unique_id() override {
    return get_mac_address() + "-wifiinfo-networks";
  }
  void dump_config() override;

protected:
  char cs_{0};
  bool is_connected_{false};
};

} // namespace wifi_info
} // namespace esphome
