#include "wifi_info_text_sensor.h"
#ifdef USE_WIFI
#include "esphome/core/log.h"

namespace esphome {
namespace wifi_info {

static const char *const TAG = "wifi_info";

/********************
 * IPAddressWiFiInfo
 *******************/

void IPAddressWiFiInfo::setup() {
  wifi::global_wifi_component->add_on_ip_state_callback(
      [this](network::IPAddresses ips, network::IPAddress dns1_ip, network::IPAddress dns2_ip) {
        this->state_callback_(ips);
      });
}

void IPAddressWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "IP Address", this); }

void IPAddressWiFiInfo::state_callback_(network::IPAddresses ips) {
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

/*********************
 * DNSAddressWifiInfo
 ********************/

void DNSAddressWifiInfo::setup() {
  wifi::global_wifi_component->add_on_ip_state_callback(
      [this](network::IPAddresses ips, network::IPAddress dns1_ip, network::IPAddress dns2_ip) {
        this->state_callback_(dns1_ip, dns2_ip);
      });
}

void DNSAddressWifiInfo::dump_config() { LOG_TEXT_SENSOR("", "DNS Address", this); }

void DNSAddressWifiInfo::state_callback_(network::IPAddress dns1_ip, network::IPAddress dns2_ip) {
  std::string dns_results = dns1_ip.str() + " " + dns2_ip.str();
  this->publish_state(dns_results);
}

/**********************
 * ScanResultsWiFiInfo
 *********************/

void ScanResultsWiFiInfo::setup() {
  wifi::global_wifi_component->add_on_wifi_scan_state_callback(
      [this](const std::vector<wifi::WiFiScanResult> &results) { this->state_callback_(results); });
}

void ScanResultsWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "Scan Results", this); }

void ScanResultsWiFiInfo::state_callback_(const std::vector<wifi::WiFiScanResult> &results) {
  std::string scan_results;
  for (auto scan : results) {
    if (scan.get_is_hidden())
      continue;

    scan_results += scan.get_ssid();
    scan_results += ": ";
    scan_results += esphome::to_string(scan.get_rssi());
    scan_results += "dB\n";
  }
  // There's a limit of 255 characters per state; longer states just don't get sent so we truncate it
  this->publish_state(scan_results.substr(0, 255));
}

/***************
 * SSIDWiFiInfo
 **************/

void SSIDWiFiInfo::setup() {
  wifi::global_wifi_component->add_on_wifi_connect_state_callback(
      [this](std::string ssid, wifi::bssid_t bssid) { this->state_callback_(ssid); });
}

void SSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "SSID", this); }

void SSIDWiFiInfo::state_callback_(std::string &ssid) { this->publish_state(ssid); }

/****************
 * BSSIDWiFiInfo
 ***************/

void BSSIDWiFiInfo::setup() {
  wifi::global_wifi_component->add_on_wifi_connect_state_callback(
      [this](std::string ssid, wifi::bssid_t bssid) { this->state_callback_(bssid); });
}

void BSSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "BSSID", this); }

void BSSIDWiFiInfo::state_callback_(wifi::bssid_t bssid) {
  char buf[18] = "unknown";
  if (mac_address_is_valid(bssid.data())) {
    format_mac_addr_upper(bssid.data(), buf);
  }
  this->publish_state(buf);
}
/*********************
 * MacAddressWifiInfo
 ********************/

void MacAddressWifiInfo::dump_config() { LOG_TEXT_SENSOR("", "MAC Address", this); }

}  // namespace wifi_info
}  // namespace esphome
#endif
