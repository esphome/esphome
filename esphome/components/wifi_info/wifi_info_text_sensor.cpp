#include "wifi_info_text_sensor.h"
#ifdef USE_WIFI
#include "esphome/core/log.h"

#ifdef USE_ESP8266
#include <pgmspace.h>
#endif

namespace esphome::wifi_info {

static const char *const TAG = "wifi_info";

#ifdef USE_WIFI_IP_STATE_LISTENERS

/********************
 * IPAddressWiFiInfo
 *******************/

void IPAddressWiFiInfo::setup() { wifi::global_wifi_component->add_ip_state_listener(this); }

void IPAddressWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "IP Address", this); }

void IPAddressWiFiInfo::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                                    const network::IPAddress &dns2) {
  char buf[network::IP_ADDRESS_BUFFER_SIZE];
  ips[0].str_to(buf);
  this->publish_state(buf);
  uint8_t sensor = 0;
  for (const auto &ip : ips) {
    if (ip.is_set()) {
      if (this->ip_sensors_[sensor] != nullptr) {
        ip.str_to(buf);
        this->ip_sensors_[sensor]->publish_state(buf);
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
  // IP_ADDRESS_BUFFER_SIZE (40) = max IP (39) + null; space reuses first null's slot
  char buf[network::IP_ADDRESS_BUFFER_SIZE * 2];
  dns1.str_to(buf);
  size_t len1 = strlen(buf);
  buf[len1] = ' ';
  dns2.str_to(buf + len1 + 1);
  this->publish_state(buf);
}

#endif  // USE_WIFI_IP_STATE_LISTENERS

#ifdef USE_WIFI_SCAN_RESULTS_LISTENERS

/**********************
 * ScanResultsWiFiInfo
 *********************/

void ScanResultsWiFiInfo::setup() { wifi::global_wifi_component->add_scan_results_listener(this); }

void ScanResultsWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "Scan Results", this); }

// Format: "SSID: -XXdB\n" - caller must ensure ssid_len + 9 bytes available in buffer
static char *format_scan_entry(char *buf, const char *ssid, size_t ssid_len, int8_t rssi) {
  memcpy(buf, ssid, ssid_len);
  buf += ssid_len;
  *buf++ = ':';
  *buf++ = ' ';
  buf = int8_to_str(buf, rssi);
  *buf++ = 'd';
  *buf++ = 'B';
  *buf++ = '\n';
  return buf;
}

void ScanResultsWiFiInfo::on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results) {
  char buf[MAX_STATE_LEN + 1];
  char *ptr = buf;
  const char *end = buf + MAX_STATE_LEN;

  for (const auto &scan : results) {
    if (scan.get_is_hidden())
      continue;
    const std::string &ssid = scan.get_ssid();
    // Max space: ssid + ": " (2) + "-128" (4) + "dB\n" (3) = ssid + 9
    if (ptr + ssid.size() + 9 > end)
      break;
    ptr = format_scan_entry(ptr, ssid.c_str(), ssid.size(), scan.get_rssi());
  }

  *ptr = '\0';
  this->publish_state(buf);
}

#endif  // USE_WIFI_SCAN_RESULTS_LISTENERS

#ifdef USE_WIFI_CONNECT_STATE_LISTENERS

/***************
 * SSIDWiFiInfo
 **************/

void SSIDWiFiInfo::setup() { wifi::global_wifi_component->add_connect_state_listener(this); }

void SSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "SSID", this); }

void SSIDWiFiInfo::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) {
  this->publish_state(ssid.c_str(), ssid.size());
}

/****************
 * BSSIDWiFiInfo
 ***************/

void BSSIDWiFiInfo::setup() { wifi::global_wifi_component->add_connect_state_listener(this); }

void BSSIDWiFiInfo::dump_config() { LOG_TEXT_SENSOR("", "BSSID", this); }

void BSSIDWiFiInfo::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) {
  char buf[18] = "unknown";
  if (mac_address_is_valid(bssid.data())) {
    format_mac_addr_upper(bssid.data(), buf);
  }
  this->publish_state(buf);
}

#endif  // USE_WIFI_CONNECT_STATE_LISTENERS

#ifdef USE_WIFI_POWER_SAVE_LISTENERS

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

#endif  // USE_WIFI_POWER_SAVE_LISTENERS

/*********************
 * MacAddressWifiInfo
 ********************/

void MacAddressWifiInfo::dump_config() { LOG_TEXT_SENSOR("", "MAC Address", this); }

}  // namespace esphome::wifi_info
#endif
