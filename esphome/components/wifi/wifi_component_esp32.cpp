#include "wifi_component.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_wifi.h>

#include <utility>
#include <algorithm>
#include "lwip/err.h"
#include "lwip/dns.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/esphal.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"

namespace esphome {
namespace wifi {

static const char *TAG = "wifi_esp32";

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  uint8_t current_mode = WiFi.getMode();
  bool current_sta = current_mode & 0b01;
  bool current_ap = current_mode & 0b10;
  bool sta_ = sta.value_or(current_sta);
  bool ap_ = ap.value_or(current_ap);
  if (current_sta == sta_ && current_ap == ap_)
    return true;

  if (sta_ && !current_sta) {
    ESP_LOGV(TAG, "Enabling STA.");
  } else if (!sta_ && current_sta) {
    ESP_LOGV(TAG, "Disabling STA.");
  }
  if (ap_ && !current_ap) {
    ESP_LOGV(TAG, "Enabling AP.");
  } else if (!ap_ && current_ap) {
    ESP_LOGV(TAG, "Disabling AP.");
  }

  uint8_t mode = 0;
  if (sta_)
    mode |= 0b01;
  if (ap_)
    mode |= 0b10;
  bool ret = WiFi.mode(static_cast<wifi_mode_t>(mode));

  if (!ret) {
    ESP_LOGW(TAG, "Setting WiFi mode failed!");
  }

  return ret;
}
bool WiFiComponent::wifi_sta_pre_setup_() {
  if (!this->wifi_mode_(true, {}))
    return false;

  WiFi.setAutoReconnect(false);
  delay(10);
  return true;
}
bool WiFiComponent::wifi_apply_power_save_() {
  wifi_ps_type_t power_save;
  switch (this->power_save_) {
    case WIFI_POWER_SAVE_LIGHT:
      power_save = WIFI_PS_MIN_MODEM;
      break;
    case WIFI_POWER_SAVE_HIGH:
      power_save = WIFI_PS_MAX_MODEM;
      break;
    case WIFI_POWER_SAVE_NONE:
    default:
      power_save = WIFI_PS_NONE;
      break;
  }
  return esp_wifi_set_ps(power_save) == ESP_OK;
}
bool WiFiComponent::wifi_sta_ip_config_(optional<ManualIP> manual_ip) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  tcpip_adapter_dhcp_status_t dhcp_status;
  tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &dhcp_status);
  if (!manual_ip.has_value()) {
    // Use DHCP client
    if (dhcp_status != TCPIP_ADAPTER_DHCP_STARTED) {
      esp_err_t err = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
      if (err != ESP_OK) {
        ESP_LOGV(TAG, "Starting DHCP client failed! %d", err);
      }
      return err == ESP_OK;
    }
    return true;
  }

  tcpip_adapter_ip_info_t info;
  memset(&info, 0, sizeof(info));
  info.ip.addr = static_cast<uint32_t>(manual_ip->static_ip);
  info.gw.addr = static_cast<uint32_t>(manual_ip->gateway);
  info.netmask.addr = static_cast<uint32_t>(manual_ip->subnet);

  esp_err_t dhcp_stop_ret = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
  if (dhcp_stop_ret != ESP_OK) {
    ESP_LOGV(TAG, "Stopping DHCP client failed! %d", dhcp_stop_ret);
  }

  esp_err_t wifi_set_info_ret = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &info);
  if (wifi_set_info_ret != ESP_OK) {
    ESP_LOGV(TAG, "Setting manual IP info failed! %s", esp_err_to_name(wifi_set_info_ret));
  }

  ip_addr_t dns;
  dns.type = IPADDR_TYPE_V4;
  if (uint32_t(manual_ip->dns1) != 0) {
    dns.u_addr.ip4.addr = static_cast<uint32_t>(manual_ip->dns1);
    dns_setserver(0, &dns);
  }
  if (uint32_t(manual_ip->dns2) != 0) {
    dns.u_addr.ip4.addr = static_cast<uint32_t>(manual_ip->dns2);
    dns_setserver(1, &dns);
  }

  return true;
}

IPAddress WiFiComponent::wifi_sta_ip_() {
  if (!this->has_sta())
    return IPAddress();
  tcpip_adapter_ip_info_t ip;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
  return IPAddress(ip.ip.addr);
}

bool WiFiComponent::wifi_apply_hostname_() {
  esp_err_t err = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, App.get_name().c_str());
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "Setting hostname failed: %d", err);
    return false;
  }
  return true;
}
bool WiFiComponent::wifi_sta_connect_(WiFiAP ap) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  wifi_config_t conf;
  memset(&conf, 0, sizeof(conf));
  strcpy(reinterpret_cast<char *>(conf.sta.ssid), ap.get_ssid().c_str());
  strcpy(reinterpret_cast<char *>(conf.sta.password), ap.get_password().c_str());

  if (ap.get_bssid().has_value()) {
    conf.sta.bssid_set = 1;
    memcpy(conf.sta.bssid, ap.get_bssid()->data(), 6);
  } else {
    conf.sta.bssid_set = 0;
  }
  if (ap.get_channel().has_value()) {
    conf.sta.channel = *ap.get_channel();
  }

  wifi_config_t current_conf;
  esp_err_t err;
  esp_wifi_get_config(WIFI_IF_STA, &current_conf);

  if (memcmp(&current_conf, &conf, sizeof(wifi_config_t)) != 0) {
    err = esp_wifi_disconnect();
    if (err != ESP_OK) {
      ESP_LOGV(TAG, "esp_wifi_disconnect failed! %d", err);
      return false;
    }
  }

  err = esp_wifi_set_config(WIFI_IF_STA, &conf);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_wifi_set_config failed! %d", err);
  }

  if (!this->wifi_sta_ip_config_(ap.get_manual_ip())) {
    return false;
  }

  this->wifi_apply_hostname_();

  err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_connect failed! %d", err);
    return false;
  }

  return true;
}
const char *get_auth_mode_str(uint8_t mode) {
  switch (mode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2 PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2 PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2 Enterprise";
    default:
      return "UNKNOWN";
  }
}
std::string format_ip4_addr(const ip4_addr_t &ip) {
  char buf[20];
  sprintf(buf, "%u.%u.%u.%u", uint8_t(ip.addr >> 0), uint8_t(ip.addr >> 8), uint8_t(ip.addr >> 16),
          uint8_t(ip.addr >> 24));
  return buf;
}
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
    case WIFI_REASON_AUTH_EXPIRE:
      return "Auth Expired";
    case WIFI_REASON_AUTH_LEAVE:
      return "Auth Leave";
    case WIFI_REASON_ASSOC_EXPIRE:
      return "Association Expired";
    case WIFI_REASON_ASSOC_TOOMANY:
      return "Too Many Associations";
    case WIFI_REASON_NOT_AUTHED:
      return "Not Authenticated";
    case WIFI_REASON_NOT_ASSOCED:
      return "Not Associated";
    case WIFI_REASON_ASSOC_LEAVE:
      return "Association Leave";
    case WIFI_REASON_ASSOC_NOT_AUTHED:
      return "Association not Authenticated";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
      return "Disassociate Power Cap Bad";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
      return "Disassociate Supported Channel Bad";
    case WIFI_REASON_IE_INVALID:
      return "IE Invalid";
    case WIFI_REASON_MIC_FAILURE:
      return "Mic Failure";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
      return "4-Way Handshake Timeout";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
      return "Group Key Update Timeout";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
      return "IE In 4-Way Handshake Differs";
    case WIFI_REASON_GROUP_CIPHER_INVALID:
      return "Group Cipher Invalid";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
      return "Pairwise Cipher Invalid";
    case WIFI_REASON_AKMP_INVALID:
      return "AKMP Invalid";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
      return "Unsupported RSN IE version";
    case WIFI_REASON_INVALID_RSN_IE_CAP:
      return "Invalid RSN IE Cap";
    case WIFI_REASON_802_1X_AUTH_FAILED:
      return "802.1x Authentication Failed";
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
      return "Cipher Suite Rejected";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "Beacon Timeout";
    case WIFI_REASON_NO_AP_FOUND:
      return "AP Not Found";
    case WIFI_REASON_AUTH_FAIL:
      return "Authentication Failed";
    case WIFI_REASON_ASSOC_FAIL:
      return "Association Failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "Handshake Failed";
    case WIFI_REASON_UNSPECIFIED:
    default:
      return "Unspecified";
  }
}
void WiFiComponent::wifi_event_callback_(system_event_id_t event, system_event_info_t info) {
  switch (event) {
    case SYSTEM_EVENT_WIFI_READY: {
      ESP_LOGV(TAG, "Event: WiFi ready");
      break;
    }
    case SYSTEM_EVENT_SCAN_DONE: {
      auto it = info.scan_done;
      ESP_LOGV(TAG, "Event: WiFi Scan Done status=%u number=%u scan_id=%u", it.status, it.number, it.scan_id);
      break;
    }
    case SYSTEM_EVENT_STA_START: {
      ESP_LOGV(TAG, "Event: WiFi STA start");
      break;
    }
    case SYSTEM_EVENT_STA_STOP: {
      ESP_LOGV(TAG, "Event: WiFi STA stop");
      break;
    }
    case SYSTEM_EVENT_STA_CONNECTED: {
      auto it = info.connected;
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      ESP_LOGV(TAG, "Event: Connected ssid='%s' bssid=" LOG_SECRET("%s") " channel=%u, authmode=%s", buf,
               format_mac_addr(it.bssid).c_str(), it.channel, get_auth_mode_str(it.authmode));
      break;
    }
    case SYSTEM_EVENT_STA_DISCONNECTED: {
      auto it = info.disconnected;
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      ESP_LOGW(TAG, "Event: Disconnected ssid='%s' bssid=" LOG_SECRET("%s") " reason=%s", buf,
               format_mac_addr(it.bssid).c_str(), get_disconnect_reason_str(it.reason));
      break;
    }
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE: {
      auto it = info.auth_change;
      ESP_LOGV(TAG, "Event: Authmode Change old=%s new=%s", get_auth_mode_str(it.old_mode),
               get_auth_mode_str(it.new_mode));
      break;
    }
    case SYSTEM_EVENT_STA_GOT_IP: {
      auto it = info.got_ip.ip_info;
      ESP_LOGV(TAG, "Event: Got IP static_ip=%s gateway=%s", format_ip4_addr(it.ip).c_str(),
               format_ip4_addr(it.gw).c_str());
      break;
    }
    case SYSTEM_EVENT_STA_LOST_IP: {
      ESP_LOGV(TAG, "Event: Lost IP");
      break;
    }
    case SYSTEM_EVENT_AP_START: {
      ESP_LOGV(TAG, "Event: WiFi AP start");
      break;
    }
    case SYSTEM_EVENT_AP_STOP: {
      ESP_LOGV(TAG, "Event: WiFi AP stop");
      break;
    }
    case SYSTEM_EVENT_AP_STACONNECTED: {
      auto it = info.sta_connected;
      ESP_LOGV(TAG, "Event: AP client connected MAC=%s aid=%u", format_mac_addr(it.mac).c_str(), it.aid);
      break;
    }
    case SYSTEM_EVENT_AP_STADISCONNECTED: {
      auto it = info.sta_disconnected;
      ESP_LOGV(TAG, "Event: AP client disconnected MAC=%s aid=%u", format_mac_addr(it.mac).c_str(), it.aid);
      break;
    }
    case SYSTEM_EVENT_AP_STAIPASSIGNED: {
      ESP_LOGV(TAG, "Event: AP client assigned IP");
      break;
    }
    case SYSTEM_EVENT_AP_PROBEREQRECVED: {
      auto it = info.ap_probereqrecved;
      ESP_LOGVV(TAG, "Event: AP receive Probe Request MAC=%s RSSI=%d", format_mac_addr(it.mac).c_str(), it.rssi);
      break;
    }
    default:
      break;
  }

  if (event == SYSTEM_EVENT_STA_DISCONNECTED) {
    uint8_t reason = info.disconnected.reason;
    if (reason == WIFI_REASON_AUTH_EXPIRE || reason == WIFI_REASON_BEACON_TIMEOUT ||
        reason == WIFI_REASON_NO_AP_FOUND || reason == WIFI_REASON_ASSOC_FAIL ||
        reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
      err_t err = esp_wifi_disconnect();
      if (err != ESP_OK) {
        ESP_LOGV(TAG, "Disconnect failed: %s", esp_err_to_name(err));
      }
      this->error_from_callback_ = true;
    }
  }
  if (event == SYSTEM_EVENT_SCAN_DONE) {
    this->wifi_scan_done_callback_();
  }
}
void WiFiComponent::wifi_pre_setup_() {
  auto f = std::bind(&WiFiComponent::wifi_event_callback_, this, std::placeholders::_1, std::placeholders::_2);
  WiFi.onEvent(f);
  WiFi.persistent(false);
  // Make sure WiFi is in clean state before anything starts
  this->wifi_mode_(false, false);
}
wl_status_t WiFiComponent::wifi_sta_status_() { return WiFi.status(); }
bool WiFiComponent::wifi_scan_start_() {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  // need to use WiFi because of WiFiScanClass allocations :(
  int16_t err = WiFi.scanNetworks(true, true, false, 200);
  if (err != WIFI_SCAN_RUNNING) {
    ESP_LOGV(TAG, "WiFi.scanNetworks failed! %d", err);
    return false;
  }

  return true;
}
void WiFiComponent::wifi_scan_done_callback_() {
  this->scan_result_.clear();

  int16_t num = WiFi.scanComplete();
  if (num < 0)
    return;

  this->scan_result_.reserve(static_cast<unsigned int>(num));
  for (int i = 0; i < num; i++) {
    String ssid = WiFi.SSID(i);
    wifi_auth_mode_t authmode = WiFi.encryptionType(i);
    int32_t rssi = WiFi.RSSI(i);
    uint8_t *bssid = WiFi.BSSID(i);
    int32_t channel = WiFi.channel(i);

    WiFiScanResult scan({bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]}, std::string(ssid.c_str()),
                        channel, rssi, authmode != WIFI_AUTH_OPEN, ssid.length() == 0);
    this->scan_result_.push_back(scan);
  }
  WiFi.scanDelete();
  this->scan_done_ = true;
}
bool WiFiComponent::wifi_ap_ip_config_(optional<ManualIP> manual_ip) {
  esp_err_t err;

  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  tcpip_adapter_ip_info_t info;
  memset(&info, 0, sizeof(info));
  if (manual_ip.has_value()) {
    info.ip.addr = static_cast<uint32_t>(manual_ip->static_ip);
    info.gw.addr = static_cast<uint32_t>(manual_ip->gateway);
    info.netmask.addr = static_cast<uint32_t>(manual_ip->subnet);
  } else {
    info.ip.addr = static_cast<uint32_t>(IPAddress(192, 168, 4, 1));
    info.gw.addr = static_cast<uint32_t>(IPAddress(192, 168, 4, 1));
    info.netmask.addr = static_cast<uint32_t>(IPAddress(255, 255, 255, 0));
  }
  tcpip_adapter_dhcp_status_t dhcp_status;
  tcpip_adapter_dhcps_get_status(TCPIP_ADAPTER_IF_AP, &dhcp_status);
  err = tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "tcpip_adapter_dhcps_stop failed! %d", err);
    return false;
  }

  err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "tcpip_adapter_set_ip_info failed! %d", err);
    return false;
  }

  dhcps_lease_t lease;
  lease.enable = true;
  IPAddress start_address = info.ip.addr;
  start_address[3] += 99;
  lease.start_ip.addr = static_cast<uint32_t>(start_address);
  ESP_LOGV(TAG, "DHCP server IP lease start: %s", start_address.toString().c_str());
  start_address[3] += 100;
  lease.end_ip.addr = static_cast<uint32_t>(start_address);
  ESP_LOGV(TAG, "DHCP server IP lease end: %s", start_address.toString().c_str());
  err = tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_SET, TCPIP_ADAPTER_REQUESTED_IP_ADDRESS, &lease, sizeof(lease));

  if (err != ESP_OK) {
    ESP_LOGV(TAG, "tcpip_adapter_dhcps_option failed! %d", err);
    return false;
  }

  err = tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);

  if (err != ESP_OK) {
    ESP_LOGV(TAG, "tcpip_adapter_dhcps_start failed! %d", err);
    return false;
  }

  return true;
}
bool WiFiComponent::wifi_start_ap_(const WiFiAP &ap) {
  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  wifi_config_t conf;
  memset(&conf, 0, sizeof(conf));
  strcpy(reinterpret_cast<char *>(conf.ap.ssid), ap.get_ssid().c_str());
  conf.ap.channel = ap.get_channel().value_or(1);
  conf.ap.ssid_hidden = ap.get_ssid().size();
  conf.ap.max_connection = 5;
  conf.ap.beacon_interval = 100;

  if (ap.get_password().empty()) {
    conf.ap.authmode = WIFI_AUTH_OPEN;
    *conf.ap.password = 0;
  } else {
    conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    strcpy(reinterpret_cast<char *>(conf.ap.password), ap.get_password().c_str());
  }

  esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &conf);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_wifi_set_config failed! %d", err);
    return false;
  }

  yield();

  if (!this->wifi_ap_ip_config_(ap.get_manual_ip())) {
    ESP_LOGV(TAG, "wifi_ap_ip_config_ failed!");
    return false;
  }

  return true;
}
IPAddress WiFiComponent::wifi_soft_ap_ip() {
  tcpip_adapter_ip_info_t ip;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip);
  return IPAddress(ip.ip.addr);
}
bool WiFiComponent::wifi_disconnect_() { return esp_wifi_disconnect(); }

}  // namespace wifi
}  // namespace esphome

#endif
