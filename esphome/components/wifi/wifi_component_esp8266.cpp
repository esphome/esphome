#include "wifi_component.h"

#ifdef ARDUINO_ARCH_ESP8266

#include <user_interface.h>

#include <utility>
#include <algorithm>
#include "lwip/err.h"
#include "lwip/dns.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/esphal.h"
#include "esphome/core/util.h"
#include "esphome/core/application.h"

namespace esphome {
namespace wifi {

static const char *TAG = "wifi_esp8266";

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  uint8_t current_mode = wifi_get_opmode();
  bool current_sta = current_mode & 0b01;
  bool current_ap = current_mode & 0b10;
  bool target_sta = sta.value_or(current_sta);
  bool target_ap = ap.value_or(current_ap);
  if (current_sta == target_sta && current_ap == target_ap)
    return true;

  if (target_sta && !current_sta) {
    ESP_LOGV(TAG, "Enabling STA.");
  } else if (!target_sta && current_sta) {
    ESP_LOGV(TAG, "Disabling STA.");
    // Stop DHCP client when disabling STA
    // See https://github.com/esp8266/Arduino/pull/5703
    wifi_station_dhcpc_stop();
  }
  if (target_ap && !current_ap) {
    ESP_LOGV(TAG, "Enabling AP.");
  } else if (!target_ap && current_ap) {
    ESP_LOGV(TAG, "Disabling AP.");
  }

  ETS_UART_INTR_DISABLE();
  uint8_t mode = 0;
  if (target_sta)
    mode |= 0b01;
  if (target_ap)
    mode |= 0b10;
  bool ret = wifi_set_opmode_current(mode);
  ETS_UART_INTR_ENABLE();

  if (!ret) {
    ESP_LOGW(TAG, "Setting WiFi mode failed!");
  }

  return ret;
}
bool WiFiComponent::wifi_disable_auto_connect_() {
  bool ret1, ret2;
  ETS_UART_INTR_DISABLE();
  ret1 = wifi_station_set_auto_connect(0);
  ret2 = wifi_station_set_reconnect_policy(false);
  ETS_UART_INTR_ENABLE();

  if (!ret1 || !ret2) {
    ESP_LOGV(TAG, "Disabling Auto-Connect failed!");
  }

  return ret1 && ret2;
}
bool WiFiComponent::wifi_apply_power_save_() {
  sleep_type_t power_save;
  switch (this->power_save_) {
    case WIFI_POWER_SAVE_LIGHT:
      power_save = LIGHT_SLEEP_T;
      break;
    case WIFI_POWER_SAVE_HIGH:
      power_save = MODEM_SLEEP_T;
      break;
    case WIFI_POWER_SAVE_NONE:
    default:
      power_save = NONE_SLEEP_T;
      break;
  }
  return wifi_set_sleep_type(power_save);
}
bool WiFiComponent::wifi_sta_ip_config_(optional<ManualIP> manual_ip) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  enum dhcp_status dhcp_status = wifi_station_dhcpc_status();
  if (!manual_ip.has_value()) {
    // Use DHCP client
    if (dhcp_status != DHCP_STARTED) {
      bool ret = wifi_station_dhcpc_start();
      if (!ret) {
        ESP_LOGV(TAG, "Starting DHCP client failed!");
      }
      return ret;
    }
    return true;
  }

  bool ret = true;

  struct ip_info info {};
  info.ip.addr = static_cast<uint32_t>(manual_ip->static_ip);
  info.gw.addr = static_cast<uint32_t>(manual_ip->gateway);
  info.netmask.addr = static_cast<uint32_t>(manual_ip->subnet);

  if (dhcp_status == DHCP_STARTED) {
    bool dhcp_stop_ret = wifi_station_dhcpc_stop();
    if (!dhcp_stop_ret) {
      ESP_LOGV(TAG, "Stopping DHCP client failed!");
      ret = false;
    }
  }
  bool wifi_set_info_ret = wifi_set_ip_info(STATION_IF, &info);
  if (!wifi_set_info_ret) {
    ESP_LOGV(TAG, "Setting manual IP info failed!");
    ret = false;
  }

  ip_addr_t dns;
  if (uint32_t(manual_ip->dns1) != 0) {
    dns.addr = static_cast<uint32_t>(manual_ip->dns1);
    dns_setserver(0, &dns);
  }
  if (uint32_t(manual_ip->dns2) != 0) {
    dns.addr = static_cast<uint32_t>(manual_ip->dns2);
    dns_setserver(1, &dns);
  }

  return ret;
}

IPAddress WiFiComponent::wifi_sta_ip_() {
  if (!this->has_sta())
    return {};
  struct ip_info ip {};
  wifi_get_ip_info(STATION_IF, &ip);
  return {ip.ip.addr};
}
bool WiFiComponent::wifi_apply_hostname_() {
  bool ret = wifi_station_set_hostname(const_cast<char *>(App.get_name().c_str()));
  if (!ret) {
    ESP_LOGV(TAG, "Setting WiFi Hostname failed!");
  }
  return ret;
}

bool WiFiComponent::wifi_sta_connect_(WiFiAP ap) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  ETS_UART_INTR_DISABLE();
  wifi_station_disconnect();
  ETS_UART_INTR_ENABLE();

  struct station_config conf {};
  memset(&conf, 0, sizeof(conf));
  strcpy(reinterpret_cast<char *>(conf.ssid), ap.get_ssid().c_str());
  strcpy(reinterpret_cast<char *>(conf.password), ap.get_password().c_str());

  if (ap.get_bssid().has_value()) {
    conf.bssid_set = 1;
    memcpy(conf.bssid, ap.get_bssid()->data(), 6);
  } else {
    conf.bssid_set = 0;
  }

#ifndef ARDUINO_ESP8266_RELEASE_2_3_0
  if (ap.get_password().empty()) {
    conf.threshold.authmode = AUTH_OPEN;
  } else {
    conf.threshold.authmode = AUTH_WPA_PSK;
  }
  conf.threshold.rssi = -127;
#endif

  ETS_UART_INTR_DISABLE();
  bool ret = wifi_station_set_config_current(&conf);
  ETS_UART_INTR_ENABLE();

  if (!ret) {
    ESP_LOGV(TAG, "Setting WiFi Station config failed!");
    return false;
  }

  if (!this->wifi_sta_ip_config_(ap.get_manual_ip())) {
    return false;
  }

  this->wifi_apply_hostname_();

  ETS_UART_INTR_DISABLE();
  ret = wifi_station_connect();
  ETS_UART_INTR_ENABLE();
  if (!ret) {
    ESP_LOGV(TAG, "wifi_station_connect failed!");
    return false;
  }

  if (ap.get_channel().has_value()) {
    ret = wifi_set_channel(*ap.get_channel());
    if (!ret) {
      ESP_LOGV(TAG, "wifi_set_channel failed!");
      return false;
    }
  }

  return true;
}

class WiFiMockClass : public ESP8266WiFiGenericClass {
 public:
  static void _event_callback(void *event) { ESP8266WiFiGenericClass::_eventCallback(event); }  // NOLINT
};

const char *get_auth_mode_str(uint8_t mode) {
  switch (mode) {
    case AUTH_OPEN:
      return "OPEN";
    case AUTH_WEP:
      return "WEP";
    case AUTH_WPA_PSK:
      return "WPA PSK";
    case AUTH_WPA2_PSK:
      return "WPA2 PSK";
    case AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2 PSK";
    default:
      return "UNKNOWN";
  }
}
#ifdef ipv4_addr
std::string format_ip_addr(struct ipv4_addr ip) {
  char buf[20];
  sprintf(buf, "%u.%u.%u.%u", uint8_t(ip.addr >> 0), uint8_t(ip.addr >> 8), uint8_t(ip.addr >> 16),
          uint8_t(ip.addr >> 24));
  return buf;
}
#else
std::string format_ip_addr(struct ip_addr ip) {
  char buf[20];
  sprintf(buf, "%u.%u.%u.%u", uint8_t(ip.addr >> 0), uint8_t(ip.addr >> 8), uint8_t(ip.addr >> 16),
          uint8_t(ip.addr >> 24));
  return buf;
}
#endif
const char *get_op_mode_str(uint8_t mode) {
  switch (mode) {
    case WIFI_OFF:
      return "OFF";
    case WIFI_STA:
      return "STA";
    case WIFI_AP:
      return "AP";
    case WIFI_AP_STA:
      return "AP+STA";
    default:
      return "UNKNOWN";
  }
}
const char *get_disconnect_reason_str(uint8_t reason) {
  switch (reason) {
    case REASON_AUTH_EXPIRE:
      return "Auth Expired";
    case REASON_AUTH_LEAVE:
      return "Auth Leave";
    case REASON_ASSOC_EXPIRE:
      return "Association Expired";
    case REASON_ASSOC_TOOMANY:
      return "Too Many Associations";
    case REASON_NOT_AUTHED:
      return "Not Authenticated";
    case REASON_NOT_ASSOCED:
      return "Not Associated";
    case REASON_ASSOC_LEAVE:
      return "Association Leave";
    case REASON_ASSOC_NOT_AUTHED:
      return "Association not Authenticated";
    case REASON_DISASSOC_PWRCAP_BAD:
      return "Disassociate Power Cap Bad";
    case REASON_DISASSOC_SUPCHAN_BAD:
      return "Disassociate Supported Channel Bad";
    case REASON_IE_INVALID:
      return "IE Invalid";
    case REASON_MIC_FAILURE:
      return "Mic Failure";
    case REASON_4WAY_HANDSHAKE_TIMEOUT:
      return "4-Way Handshake Timeout";
    case REASON_GROUP_KEY_UPDATE_TIMEOUT:
      return "Group Key Update Timeout";
    case REASON_IE_IN_4WAY_DIFFERS:
      return "IE In 4-Way Handshake Differs";
    case REASON_GROUP_CIPHER_INVALID:
      return "Group Cipher Invalid";
    case REASON_PAIRWISE_CIPHER_INVALID:
      return "Pairwise Cipher Invalid";
    case REASON_AKMP_INVALID:
      return "AKMP Invalid";
    case REASON_UNSUPP_RSN_IE_VERSION:
      return "Unsupported RSN IE version";
    case REASON_INVALID_RSN_IE_CAP:
      return "Invalid RSN IE Cap";
    case REASON_802_1X_AUTH_FAILED:
      return "802.1x Authentication Failed";
    case REASON_CIPHER_SUITE_REJECTED:
      return "Cipher Suite Rejected";
    case REASON_BEACON_TIMEOUT:
      return "Beacon Timeout";
    case REASON_NO_AP_FOUND:
      return "AP Not Found";
    case REASON_AUTH_FAIL:
      return "Authentication Failed";
    case REASON_ASSOC_FAIL:
      return "Association Failed";
    case REASON_HANDSHAKE_TIMEOUT:
      return "Handshake Failed";
    case REASON_UNSPECIFIED:
    default:
      return "Unspecified";
  }
}

void WiFiComponent::wifi_event_callback(System_Event_t *event) {
#ifdef ESPHOME_LOG_HAS_VERBOSE
  // TODO: this callback is called while in cont context, so delay will fail
  // We need to defer the log messages until we're out of this context
  // only affects verbose log level
  // reproducible by enabling verbose log level and letting the ESP disconnect and
  // then reconnect to WiFi.
  switch (event->event) {
    case EVENT_STAMODE_CONNECTED: {
      auto it = event->event_info.connected;
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      ESP_LOGV(TAG, "Event: Connected ssid='%s' bssid=%s channel=%u", buf, format_mac_addr(it.bssid).c_str(),
               it.channel);
      break;
    }
    case EVENT_STAMODE_DISCONNECTED: {
      auto it = event->event_info.disconnected;
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      ESP_LOGV(TAG, "Event: Disconnected ssid='%s' bssid=%s reason='%s'", buf, format_mac_addr(it.bssid).c_str(),
               get_disconnect_reason_str(it.reason));
      break;
    }
    case EVENT_STAMODE_AUTHMODE_CHANGE: {
      auto it = event->event_info.auth_change;
      ESP_LOGV(TAG, "Event: Changed AuthMode old=%s new=%s", get_auth_mode_str(it.old_mode),
               get_auth_mode_str(it.new_mode));
      break;
    }
    case EVENT_STAMODE_GOT_IP: {
      auto it = event->event_info.got_ip;
      ESP_LOGV(TAG, "Event: Got IP static_ip=%s gateway=%s netmask=%s", format_ip_addr(it.ip).c_str(),
               format_ip_addr(it.gw).c_str(), format_ip_addr(it.mask).c_str());
      break;
    }
    case EVENT_STAMODE_DHCP_TIMEOUT: {
      ESP_LOGW(TAG, "Event: Getting IP address timeout");
      break;
    }
    case EVENT_SOFTAPMODE_STACONNECTED: {
      auto it = event->event_info.sta_connected;
      ESP_LOGV(TAG, "Event: AP client connected MAC=%s aid=%u", format_mac_addr(it.mac).c_str(), it.aid);
      break;
    }
    case EVENT_SOFTAPMODE_STADISCONNECTED: {
      auto it = event->event_info.sta_disconnected;
      ESP_LOGV(TAG, "Event: AP client disconnected MAC=%s aid=%u", format_mac_addr(it.mac).c_str(), it.aid);
      break;
    }
    case EVENT_SOFTAPMODE_PROBEREQRECVED: {
      auto it = event->event_info.ap_probereqrecved;
      ESP_LOGV(TAG, "Event: AP receive Probe Request MAC=%s RSSI=%d", format_mac_addr(it.mac).c_str(), it.rssi);
      break;
    }
#ifndef ARDUINO_ESP8266_RELEASE_2_3_0
    case EVENT_OPMODE_CHANGED: {
      auto it = event->event_info.opmode_changed;
      ESP_LOGV(TAG, "Event: Changed Mode old=%s new=%s", get_op_mode_str(it.old_opmode),
               get_op_mode_str(it.new_opmode));
      break;
    }
    case EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP: {
      auto it = event->event_info.distribute_sta_ip;
      ESP_LOGV(TAG, "Event: AP Distribute Station IP MAC=%s IP=%s aid=%u", format_mac_addr(it.mac).c_str(),
               format_ip_addr(it.ip).c_str(), it.aid);
      break;
    }
#endif
    default:
      break;
  }
#endif

  if (event->event == EVENT_STAMODE_DISCONNECTED) {
    global_wifi_component->error_from_callback_ = true;
  }

  WiFiMockClass::_event_callback(event);
}

void WiFiComponent::wifi_register_callbacks_() { wifi_set_event_handler_cb(&WiFiComponent::wifi_event_callback); }
wl_status_t WiFiComponent::wifi_sta_status_() {
  station_status_t status = wifi_station_get_connect_status();
  switch (status) {
    case STATION_GOT_IP:
      return WL_CONNECTED;
    case STATION_NO_AP_FOUND:
      return WL_NO_SSID_AVAIL;
    case STATION_CONNECT_FAIL:
    case STATION_WRONG_PASSWORD:
      return WL_CONNECT_FAILED;
    case STATION_IDLE:
      return WL_IDLE_STATUS;
    case STATION_CONNECTING:
    default:
      return WL_DISCONNECTED;
  }
}
bool WiFiComponent::wifi_scan_start_() {
  static bool FIRST_SCAN = false;

  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  station_status_t sta_status = wifi_station_get_connect_status();
  if (sta_status != STATION_GOT_IP && sta_status != STATION_IDLE) {
    wifi_station_disconnect();
  }

  struct scan_config config {};
  memset(&config, 0, sizeof(config));
  config.ssid = nullptr;
  config.bssid = nullptr;
  config.channel = 0;
  config.show_hidden = 1;
#ifndef ARDUINO_ESP8266_RELEASE_2_3_0
  config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  if (FIRST_SCAN) {
    config.scan_time.active.min = 100;
    config.scan_time.active.max = 200;
  } else {
    config.scan_time.active.min = 400;
    config.scan_time.active.max = 500;
  }
#endif
  FIRST_SCAN = false;
  bool ret = wifi_station_scan(&config, &WiFiComponent::s_wifi_scan_done_callback);
  if (!ret) {
    ESP_LOGV(TAG, "wifi_station_scan failed!");
    return false;
  }

  return ret;
}
void WiFiComponent::s_wifi_scan_done_callback(void *arg, STATUS status) {
  global_wifi_component->wifi_scan_done_callback_(arg, status);
}

void WiFiComponent::wifi_scan_done_callback_(void *arg, STATUS status) {
  this->scan_result_.clear();

  if (status != OK) {
    ESP_LOGV(TAG, "Scan failed! %d", status);
    this->retry_connect();
    return;
  }
  auto *head = reinterpret_cast<bss_info *>(arg);
  for (bss_info *it = head; it != nullptr; it = STAILQ_NEXT(it, next)) {
    WiFiScanResult res({it->bssid[0], it->bssid[1], it->bssid[2], it->bssid[3], it->bssid[4], it->bssid[5]},
                       std::string(reinterpret_cast<char *>(it->ssid), it->ssid_len), it->channel, it->rssi,
                       it->authmode != AUTH_OPEN, it->is_hidden != 0);
    this->scan_result_.push_back(res);
  }
  this->scan_done_ = true;
}
bool WiFiComponent::wifi_ap_ip_config_(optional<ManualIP> manual_ip) {
  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  struct ip_info info {};
  if (manual_ip.has_value()) {
    info.ip.addr = static_cast<uint32_t>(manual_ip->static_ip);
    info.gw.addr = static_cast<uint32_t>(manual_ip->gateway);
    info.netmask.addr = static_cast<uint32_t>(manual_ip->subnet);
  } else {
    info.ip.addr = static_cast<uint32_t>(IPAddress(192, 168, 4, 1));
    info.gw.addr = static_cast<uint32_t>(IPAddress(192, 168, 4, 1));
    info.netmask.addr = static_cast<uint32_t>(IPAddress(255, 255, 255, 0));
  }

  if (wifi_softap_dhcps_status() == DHCP_STARTED) {
    if (!wifi_softap_dhcps_stop()) {
      ESP_LOGV(TAG, "Stopping DHCP server failed!");
    }
  }

  if (!wifi_set_ip_info(SOFTAP_IF, &info)) {
    ESP_LOGV(TAG, "Setting SoftAP info failed!");
    return false;
  }

  struct dhcps_lease lease {};
  IPAddress start_address = info.ip.addr;
  start_address[3] += 99;
  lease.start_ip.addr = static_cast<uint32_t>(start_address);
  ESP_LOGV(TAG, "DHCP server IP lease start: %s", start_address.toString().c_str());
  start_address[3] += 100;
  lease.end_ip.addr = static_cast<uint32_t>(start_address);
  ESP_LOGV(TAG, "DHCP server IP lease end: %s", start_address.toString().c_str());
  if (!wifi_softap_set_dhcps_lease(&lease)) {
    ESP_LOGV(TAG, "Setting SoftAP DHCP lease failed!");
    return false;
  }

  // lease time 1440 minutes (=24 hours)
  if (!wifi_softap_set_dhcps_lease_time(1440)) {
    ESP_LOGV(TAG, "Setting SoftAP DHCP lease time failed!");
    return false;
  }

  uint8_t mode = 1;
  // bit0, 1 enables router information from ESP8266 SoftAP DHCP server.
  if (!wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &mode)) {
    ESP_LOGV(TAG, "wifi_softap_set_dhcps_offer_option failed!");
    return false;
  }

  if (!wifi_softap_dhcps_start()) {
    ESP_LOGV(TAG, "Starting SoftAP DHCPS failed!");
    return false;
  }

  return true;
}
bool WiFiComponent::wifi_start_ap_(const WiFiAP &ap) {
  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  struct softap_config conf {};
  strcpy(reinterpret_cast<char *>(conf.ssid), ap.get_ssid().c_str());
  conf.ssid_len = static_cast<uint8>(ap.get_ssid().size());
  conf.channel = ap.get_channel().value_or(1);
  conf.ssid_hidden = 0;
  conf.max_connection = 5;
  conf.beacon_interval = 100;

  if (ap.get_password().empty()) {
    conf.authmode = AUTH_OPEN;
    *conf.password = 0;
  } else {
    conf.authmode = AUTH_WPA2_PSK;
    strcpy(reinterpret_cast<char *>(conf.password), ap.get_password().c_str());
  }

  ETS_UART_INTR_DISABLE();
  bool ret = wifi_softap_set_config_current(&conf);
  ETS_UART_INTR_ENABLE();

  if (!ret) {
    ESP_LOGV(TAG, "wifi_softap_set_config_current failed!");
    return false;
  }

  if (!this->wifi_ap_ip_config_(ap.get_manual_ip())) {
    ESP_LOGV(TAG, "wifi_ap_ip_config_ failed!");
    return false;
  }

  return true;
}
IPAddress WiFiComponent::wifi_soft_ap_ip_() {
  struct ip_info ip {};
  wifi_get_ip_info(SOFTAP_IF, &ip);
  return {ip.ip.addr};
}

}  // namespace wifi
}  // namespace esphome

#endif
