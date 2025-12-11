#include "wifi_info_text_sensor.h"
#ifdef USE_WIFI
#include "esphome/core/log.h"

#ifdef USE_ESP8266
#include <pgmspace.h>
#endif

namespace esphome::wifi_info {

static const char *const TAG = "wifi_info";

#ifdef USE_WIFI_LISTENERS

static constexpr size_t MAX_STATE_LENGTH = 255;

/********************
 * IPAddressWiFiInfo
 *******************/

void IPAddressWiFiInfo::setup() { wifi::global_wifi_component->add_ip_state_listener(this); }

void IPAddressWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "IP Address", this); }

void IPAddressWiFiInfo::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                                    const network::IPAddress &dns2) {
  this->publish_state(ips[0].str());
  uint8_t sensor = 0;
  for (const auto &ip : ips) {
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

void DNSAddressWifiInfo::setup() { wifi::global_wifi_component->add_ip_state_listener(this); }

void DNSAddressWifiInfo::dump_config() { LOG_TEXT_SENSOR("", "DNS Address", this); }

void DNSAddressWifiInfo::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                                     const network::IPAddress &dns2) {
  std::string dns_results = dns1.str() + " " + dns2.str();
  this->publish_state(dns_results);
}

/**********************
 * ScanResultsWiFiInfo
 *********************/

void ScanResultsWiFiInfo::setup() { wifi::global_wifi_component->add_scan_results_listener(this); }

void ScanResultsWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "Scan Results", this); }

void ScanResultsWiFiInfo::on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results) {
  std::string scan_results;
  for (const auto &scan : results) {
    if (scan.get_is_hidden())
      continue;

    scan_results += scan.get_ssid();
    scan_results += ": ";
    scan_results += esphome::to_string(scan.get_rssi());
    scan_results += "dB\n";
  }
  // There's a limit of 255 characters per state; longer states just don't get sent so we truncate it
  if (scan_results.length() > MAX_STATE_LENGTH) {
    scan_results.resize(MAX_STATE_LENGTH);
  }
  this->publish_state(scan_results);
}

/***************
 * SSIDWiFiInfo
 **************/

void SSIDWiFiInfo::setup() { wifi::global_wifi_component->add_connect_state_listener(this); }

void SSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "SSID", this); }

void SSIDWiFiInfo::on_wifi_connect_state(const std::string &ssid, const wifi::bssid_t &bssid) {
  this->publish_state(ssid);
}

/****************
 * BSSIDWiFiInfo
 ***************/

void BSSIDWiFiInfo::setup() { wifi::global_wifi_component->add_connect_state_listener(this); }

void BSSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "BSSID", this); }

void BSSIDWiFiInfo::on_wifi_connect_state(const std::string &ssid, const wifi::bssid_t &bssid) {
  char buf[18] = "unknown";
  if (mac_address_is_valid(bssid.data())) {
    format_mac_addr_upper(bssid.data(), buf);
  }
  this->publish_state(buf);
}

/************************
 * PowerSaveModeWiFiInfo
 ***********************/

void PowerSaveModeWiFiInfo::setup() { wifi::global_wifi_component->add_power_save_listener(this); }

void PowerSaveModeWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "WiFi Power Save Mode", this); }

void PowerSaveModeWiFiInfo::on_wifi_power_save(wifi::WiFiPowerSaveMode mode) {
#ifdef USE_ESP8266
#define MODE_STR(s) static const char MODE_##s[] PROGMEM = #s
  MODE_STR(NONE);
  MODE_STR(LIGHT);
  MODE_STR(HIGH);
  MODE_STR(UNKNOWN);

  const char *mode_str_p;
  switch (mode) {
    case wifi::WIFI_POWER_SAVE_NONE:
      mode_str_p = MODE_NONE;
      break;
    case wifi::WIFI_POWER_SAVE_LIGHT:
      mode_str_p = MODE_LIGHT;
      break;
    case wifi::WIFI_POWER_SAVE_HIGH:
      mode_str_p = MODE_HIGH;
      break;
    default:
      mode_str_p = MODE_UNKNOWN;
      break;
  }

  char mode_str[8];
  strncpy_P(mode_str, mode_str_p, sizeof(mode_str));
  mode_str[sizeof(mode_str) - 1] = '\0';
#undef MODE_STR
#else
  const char *mode_str;
  switch (mode) {
    case wifi::WIFI_POWER_SAVE_NONE:
      mode_str = "NONE";
      break;
    case wifi::WIFI_POWER_SAVE_LIGHT:
      mode_str = "LIGHT";
      break;
    case wifi::WIFI_POWER_SAVE_HIGH:
      mode_str = "HIGH";
      break;
    default:
      mode_str = "UNKNOWN";
      break;
  }
#endif
  this->publish_state(mode_str);
}

#endif

/*********************
 * MacAddressWifiInfo
 ********************/

void MacAddressWifiInfo::dump_config() { LOG_TEXT_SENSOR("", "MAC Address", this); }

}  // namespace esphome::wifi_info
#endif
