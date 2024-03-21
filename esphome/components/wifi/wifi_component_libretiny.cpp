#include "wifi_component.h"

#ifdef USE_LIBRETINY

#include <utility>
#include <algorithm>
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/dns.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"

namespace esphome {
namespace wifi {

static const char *const TAG = "wifi_lt";

static bool s_sta_connecting = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  uint8_t current_mode = WiFi.getMode();
  bool current_sta = current_mode & 0b01;
  bool current_ap = current_mode & 0b10;
  bool enable_sta = sta.value_or(current_sta);
  bool enable_ap = ap.value_or(current_ap);
  if (current_sta == enable_sta && current_ap == enable_ap)
    return true;

  if (enable_sta && !current_sta) {
    ESP_LOGV(TAG, "Enabling STA.");
  } else if (!enable_sta && current_sta) {
    ESP_LOGV(TAG, "Disabling STA.");
  }
  if (enable_ap && !current_ap) {
    ESP_LOGV(TAG, "Enabling AP.");
  } else if (!enable_ap && current_ap) {
    ESP_LOGV(TAG, "Disabling AP.");
  }

  uint8_t mode = 0;
  if (enable_sta)
    mode |= 0b01;
  if (enable_ap)
    mode |= 0b10;
  bool ret = WiFi.mode(static_cast<wifi_mode_t>(mode));

  if (!ret) {
    ESP_LOGW(TAG, "Setting WiFi mode failed!");
  }

  return ret;
}
bool WiFiComponent::wifi_apply_output_power_(float output_power) {
  int8_t val = static_cast<int8_t>(output_power * 4);
  return WiFi.setTxPower(val);
}
bool WiFiComponent::wifi_sta_pre_setup_() {
  if (!this->wifi_mode_(true, {}))
    return false;

  WiFi.setAutoReconnect(false);
  delay(10);
  return true;
}
bool WiFiComponent::wifi_apply_power_save_() { return WiFi.setSleep(this->power_save_ != WIFI_POWER_SAVE_NONE); }
bool WiFiComponent::wifi_sta_ip_config_(optional<ManualIP> manual_ip) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  if (!manual_ip.has_value()) {
    return true;
  }

  WiFi.config(manual_ip->static_ip, manual_ip->gateway, manual_ip->subnet, manual_ip->dns1, manual_ip->dns2);

  return true;
}

network::IPAddresses WiFiComponent::wifi_sta_ip_addresses() {
  if (!this->has_sta())
    return {};
  return {WiFi.localIP()};
}

bool WiFiComponent::wifi_apply_hostname_() {
  // setting is done in SYSTEM_EVENT_STA_START callback too
  WiFi.setHostname(App.get_name().c_str());
  return true;
}
bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  String ssid = WiFi.SSID();
  if (ssid && strcmp(ssid.c_str(), ap.get_ssid().c_str()) != 0) {
    WiFi.disconnect();
  }

  if (!this->wifi_sta_ip_config_(ap.get_manual_ip())) {
    return false;
  }

  this->wifi_apply_hostname_();

  s_sta_connecting = true;

  WiFiStatus status = WiFi.begin(ap.get_ssid().c_str(), ap.get_password().empty() ? NULL : ap.get_password().c_str(),
                                 ap.get_channel().has_value() ? *ap.get_channel() : 0,
                                 ap.get_bssid().has_value() ? ap.get_bssid()->data() : NULL);
  if (status != WL_CONNECTED) {
    ESP_LOGW(TAG, "esp_wifi_connect failed! %d", status);
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
    default:
      return "UNKNOWN";
  }
}

using esphome_ip4_addr_t = IPAddress;

std::string format_ip4_addr(const esphome_ip4_addr_t &ip) {
  char buf[20];
  uint32_t addr = ip;
  sprintf(buf, "%u.%u.%u.%u", uint8_t(addr >> 0), uint8_t(addr >> 8), uint8_t(addr >> 16), uint8_t(addr >> 24));
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
    case WIFI_REASON_CONNECTION_FAIL:
      return "Connection Failed";
    case WIFI_REASON_UNSPECIFIED:
    default:
      return "Unspecified";
  }
}

#define ESPHOME_EVENT_ID_WIFI_READY ARDUINO_EVENT_WIFI_READY
#define ESPHOME_EVENT_ID_WIFI_SCAN_DONE ARDUINO_EVENT_WIFI_SCAN_DONE
#define ESPHOME_EVENT_ID_WIFI_STA_START ARDUINO_EVENT_WIFI_STA_START
#define ESPHOME_EVENT_ID_WIFI_STA_STOP ARDUINO_EVENT_WIFI_STA_STOP
#define ESPHOME_EVENT_ID_WIFI_STA_CONNECTED ARDUINO_EVENT_WIFI_STA_CONNECTED
#define ESPHOME_EVENT_ID_WIFI_STA_DISCONNECTED ARDUINO_EVENT_WIFI_STA_DISCONNECTED
#define ESPHOME_EVENT_ID_WIFI_STA_AUTHMODE_CHANGE ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE
#define ESPHOME_EVENT_ID_WIFI_STA_GOT_IP ARDUINO_EVENT_WIFI_STA_GOT_IP
#define ESPHOME_EVENT_ID_WIFI_STA_GOT_IP6 ARDUINO_EVENT_WIFI_STA_GOT_IP6
#define ESPHOME_EVENT_ID_WIFI_STA_LOST_IP ARDUINO_EVENT_WIFI_STA_LOST_IP
#define ESPHOME_EVENT_ID_WIFI_AP_START ARDUINO_EVENT_WIFI_AP_START
#define ESPHOME_EVENT_ID_WIFI_AP_STOP ARDUINO_EVENT_WIFI_AP_STOP
#define ESPHOME_EVENT_ID_WIFI_AP_STACONNECTED ARDUINO_EVENT_WIFI_AP_STACONNECTED
#define ESPHOME_EVENT_ID_WIFI_AP_STADISCONNECTED ARDUINO_EVENT_WIFI_AP_STADISCONNECTED
#define ESPHOME_EVENT_ID_WIFI_AP_STAIPASSIGNED ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED
#define ESPHOME_EVENT_ID_WIFI_AP_PROBEREQRECVED ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED
#define ESPHOME_EVENT_ID_WIFI_AP_GOT_IP6 ARDUINO_EVENT_WIFI_AP_GOT_IP6
using esphome_wifi_event_id_t = arduino_event_id_t;
using esphome_wifi_event_info_t = arduino_event_info_t;

void WiFiComponent::wifi_event_callback_(esphome_wifi_event_id_t event, esphome_wifi_event_info_t info) {
  switch (event) {
    case ESPHOME_EVENT_ID_WIFI_READY: {
      ESP_LOGV(TAG, "Event: WiFi ready");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_SCAN_DONE: {
      auto it = info.wifi_scan_done;
      ESP_LOGV(TAG, "Event: WiFi Scan Done status=%u number=%u scan_id=%u", it.status, it.number, it.scan_id);

      this->wifi_scan_done_callback_();
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_START: {
      ESP_LOGV(TAG, "Event: WiFi STA start");
      WiFi.setHostname(App.get_name().c_str());
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_STOP: {
      ESP_LOGV(TAG, "Event: WiFi STA stop");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_CONNECTED: {
      auto it = info.wifi_sta_connected;
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      ESP_LOGV(TAG, "Event: Connected ssid='%s' bssid=" LOG_SECRET("%s") " channel=%u, authmode=%s", buf,
               format_mac_addr(it.bssid).c_str(), it.channel, get_auth_mode_str(it.authmode));

      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_DISCONNECTED: {
      auto it = info.wifi_sta_disconnected;
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      if (it.reason == WIFI_REASON_NO_AP_FOUND) {
        ESP_LOGW(TAG, "Event: Disconnected ssid='%s' reason='Probe Request Unsuccessful'", buf);
      } else {
        ESP_LOGW(TAG, "Event: Disconnected ssid='%s' bssid=" LOG_SECRET("%s") " reason='%s'", buf,
                 format_mac_addr(it.bssid).c_str(), get_disconnect_reason_str(it.reason));
      }

      uint8_t reason = it.reason;
      if (reason == WIFI_REASON_AUTH_EXPIRE || reason == WIFI_REASON_BEACON_TIMEOUT ||
          reason == WIFI_REASON_NO_AP_FOUND || reason == WIFI_REASON_ASSOC_FAIL ||
          reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
        WiFi.disconnect();
        this->error_from_callback_ = true;
      }

      s_sta_connecting = false;
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_AUTHMODE_CHANGE: {
      auto it = info.wifi_sta_authmode_change;
      ESP_LOGV(TAG, "Event: Authmode Change old=%s new=%s", get_auth_mode_str(it.old_mode),
               get_auth_mode_str(it.new_mode));
      // Mitigate CVE-2020-12638
      // https://lbsfilm.at/blog/wpa2-authenticationmode-downgrade-in-espressif-microprocessors
      if (it.old_mode != WIFI_AUTH_OPEN && it.new_mode == WIFI_AUTH_OPEN) {
        ESP_LOGW(TAG, "Potential Authmode downgrade detected, disconnecting...");
        // we can't call retry_connect() from this context, so disconnect immediately
        // and notify main thread with error_from_callback_
        WiFi.disconnect();
        this->error_from_callback_ = true;
      }
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_GOT_IP: {
      // auto it = info.got_ip.ip_info;
      ESP_LOGV(TAG, "Event: Got IP static_ip=%s gateway=%s", format_ip4_addr(WiFi.localIP()).c_str(),
               format_ip4_addr(WiFi.gatewayIP()).c_str());
      s_sta_connecting = false;
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_LOST_IP: {
      ESP_LOGV(TAG, "Event: Lost IP");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_START: {
      ESP_LOGV(TAG, "Event: WiFi AP start");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STOP: {
      ESP_LOGV(TAG, "Event: WiFi AP stop");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STACONNECTED: {
      auto it = info.wifi_sta_connected;
      auto &mac = it.bssid;
      ESP_LOGV(TAG, "Event: AP client connected MAC=%s", format_mac_addr(mac).c_str());
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STADISCONNECTED: {
      auto it = info.wifi_sta_disconnected;
      auto &mac = it.bssid;
      ESP_LOGV(TAG, "Event: AP client disconnected MAC=%s", format_mac_addr(mac).c_str());
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STAIPASSIGNED: {
      ESP_LOGV(TAG, "Event: AP client assigned IP");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_PROBEREQRECVED: {
      auto it = info.wifi_ap_probereqrecved;
      ESP_LOGVV(TAG, "Event: AP receive Probe Request MAC=%s RSSI=%d", format_mac_addr(it.mac).c_str(), it.rssi);
      break;
    }
    default:
      break;
  }
}
void WiFiComponent::wifi_pre_setup_() {
  auto f = std::bind(&WiFiComponent::wifi_event_callback_, this, std::placeholders::_1, std::placeholders::_2);
  WiFi.onEvent(f);
  // Make sure WiFi is in clean state before anything starts
  this->wifi_mode_(false, false);
}
WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() {
  auto status = WiFi.status();
  if (status == WL_CONNECTED) {
    return WiFiSTAConnectStatus::CONNECTED;
  } else if (status == WL_CONNECT_FAILED || status == WL_CONNECTION_LOST) {
    return WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
  } else if (status == WL_NO_SSID_AVAIL) {
    return WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND;
  } else if (s_sta_connecting) {
    return WiFiSTAConnectStatus::CONNECTING;
  }
  return WiFiSTAConnectStatus::IDLE;
}
bool WiFiComponent::wifi_scan_start_(bool passive) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  // need to use WiFi because of WiFiScanClass allocations :(
  int16_t err = WiFi.scanNetworks(true, true, passive, 200);
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

#ifdef USE_WIFI_AP
bool WiFiComponent::wifi_ap_ip_config_(optional<ManualIP> manual_ip) {
  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  if (manual_ip.has_value()) {
    return WiFi.softAPConfig(manual_ip->static_ip, manual_ip->gateway, manual_ip->subnet);
  } else {
    return WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  }
}

bool WiFiComponent::wifi_start_ap_(const WiFiAP &ap) {
  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  if (!this->wifi_ap_ip_config_(ap.get_manual_ip())) {
    ESP_LOGV(TAG, "wifi_ap_ip_config_ failed!");
    return false;
  }

  yield();

  return WiFi.softAP(ap.get_ssid().c_str(), ap.get_password().empty() ? NULL : ap.get_password().c_str(),
                     ap.get_channel().value_or(1), ap.get_hidden());
}

network::IPAddress WiFiComponent::wifi_soft_ap_ip() { return {WiFi.softAPIP()}; }
#endif  // USE_WIFI_AP

bool WiFiComponent::wifi_disconnect_() { return WiFi.disconnect(); }

bssid_t WiFiComponent::wifi_bssid() {
  bssid_t bssid{};
  uint8_t *raw_bssid = WiFi.BSSID();
  if (raw_bssid != nullptr) {
    for (size_t i = 0; i < bssid.size(); i++)
      bssid[i] = raw_bssid[i];
  }
  return bssid;
}
std::string WiFiComponent::wifi_ssid() { return WiFi.SSID().c_str(); }
int8_t WiFiComponent::wifi_rssi() { return WiFi.RSSI(); }
int32_t WiFiComponent::wifi_channel_() { return WiFi.channel(); }
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return {WiFi.subnetMask()}; }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return {WiFi.gatewayIP()}; }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) { return {WiFi.dnsIP(num)}; }
void WiFiComponent::wifi_loop_() {}

}  // namespace wifi
}  // namespace esphome

#endif  // USE_LIBRETINY
