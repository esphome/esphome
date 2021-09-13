#include "wifi_component.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_wifi.h>

#include <utility>
#include <algorithm>
#ifdef USE_WIFI_WPA2_EAP
#include <esp_wpa2.h>
#endif
#include "lwip/err.h"
#include "lwip/dns.h"
#include "lwip/apps/sntp.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/esphal.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"

namespace esphome {
namespace wifi {

static const char *const TAG = "wifi_esp32";

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
bool WiFiComponent::wifi_apply_output_power_(float output_power) {
  int8_t val = static_cast<int8_t>(output_power * 4);
  return esp_wifi_set_max_tx_power(val) == ESP_OK;
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
    // lwIP starts the SNTP client if it gets an SNTP server from DHCP. We don't need the time, and more importantly,
    // the built-in SNTP client has a memory leak in certain situations. Disable this feature.
    // https://github.com/esphome/issues/issues/2299
    sntp_servermode_dhcp(false);

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
  // setting is done in SYSTEM_EVENT_STA_START callback
  return true;
}
bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#_CPPv417wifi_sta_config_t
  wifi_config_t conf;
  memset(&conf, 0, sizeof(conf));
  strcpy(reinterpret_cast<char *>(conf.sta.ssid), ap.get_ssid().c_str());
  strcpy(reinterpret_cast<char *>(conf.sta.password), ap.get_password().c_str());

  // The weakest authmode to accept in the fast scan mode
  if (ap.get_password().empty()) {
    conf.sta.threshold.authmode = WIFI_AUTH_OPEN;
  } else {
    conf.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
  }

#ifdef USE_WIFI_WPA2_EAP
  if (ap.get_eap().has_value()) {
    conf.sta.threshold.authmode = WIFI_AUTH_WPA2_ENTERPRISE;
  }
#endif

  if (ap.get_bssid().has_value()) {
    conf.sta.bssid_set = 1;
    memcpy(conf.sta.bssid, ap.get_bssid()->data(), 6);
  } else {
    conf.sta.bssid_set = 0;
  }
  if (ap.get_channel().has_value()) {
    conf.sta.channel = *ap.get_channel();
    conf.sta.scan_method = WIFI_FAST_SCAN;
  } else {
    conf.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  }
  // Listen interval for ESP32 station to receive beacon when WIFI_PS_MAX_MODEM is set.
  // Units: AP beacon intervals. Defaults to 3 if set to 0.
  conf.sta.listen_interval = 0;

#if ESP_IDF_VERSION_MAJOR >= 4
  // Protected Management Frame
  // Device will prefer to connect in PMF mode if other device also advertizes PMF capability.
  conf.sta.pmf_cfg.capable = true;
  conf.sta.pmf_cfg.required = false;
#endif

  // note, we do our own filtering
  // The minimum rssi to accept in the fast scan mode
  conf.sta.threshold.rssi = -127;

  conf.sta.threshold.authmode = WIFI_AUTH_OPEN;

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

  // setup enterprise authentication if required
#ifdef USE_WIFI_WPA2_EAP
  if (ap.get_eap().has_value()) {
    // note: all certificates and keys have to be null terminated. Lengths are appended by +1 to include \0.
    EAPAuth eap = ap.get_eap().value();
    err = esp_wifi_sta_wpa2_ent_set_identity((uint8_t *) eap.identity.c_str(), eap.identity.length());
    if (err != ESP_OK) {
      ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_identity failed! %d", err);
    }
    int ca_cert_len = strlen(eap.ca_cert);
    int client_cert_len = strlen(eap.client_cert);
    int client_key_len = strlen(eap.client_key);
    if (ca_cert_len) {
      err = esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t *) eap.ca_cert, ca_cert_len + 1);
      if (err != ESP_OK) {
        ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_ca_cert failed! %d", err);
      }
    }
    // workout what type of EAP this is
    // validation is not required as the config tool has already validated it
    if (client_cert_len && client_key_len) {
      // if we have certs, this must be EAP-TLS
      err = esp_wifi_sta_wpa2_ent_set_cert_key((uint8_t *) eap.client_cert, client_cert_len + 1,
                                               (uint8_t *) eap.client_key, client_key_len + 1,
                                               (uint8_t *) eap.password.c_str(), strlen(eap.password.c_str()));
      if (err != ESP_OK) {
        ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_cert_key failed! %d", err);
      }
    } else {
      // in the absence of certs, assume this is username/password based
      err = esp_wifi_sta_wpa2_ent_set_username((uint8_t *) eap.username.c_str(), eap.username.length());
      if (err != ESP_OK) {
        ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_username failed! %d", err);
      }
      err = esp_wifi_sta_wpa2_ent_set_password((uint8_t *) eap.password.c_str(), eap.password.length());
      if (err != ESP_OK) {
        ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_password failed! %d", err);
      }
    }
    esp_wpa2_config_t wpa2_config = WPA2_CONFIG_INIT_DEFAULT();
    err = esp_wifi_sta_wpa2_ent_enable(&wpa2_config);
    if (err != ESP_OK) {
      ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_enable failed! %d", err);
    }
  }
#endif  // USE_WIFI_WPA2_EAP

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

#if ESP_IDF_VERSION_MAJOR >= 4
using esphome_ip4_addr_t = esp_ip4_addr_t;
#else
using esphome_ip4_addr_t = ip4_addr_t;
#endif

std::string format_ip4_addr(const esphome_ip4_addr_t &ip) {
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
    case WIFI_REASON_CONNECTION_FAIL:
      return "Connection Failed";
    case WIFI_REASON_UNSPECIFIED:
    default:
      return "Unspecified";
  }
}
#if ESP_IDF_VERSION_MAJOR >= 4
void WiFiComponent::wifi_event_callback_(arduino_event_id_t event, arduino_event_info_t info) {
#else
void WiFiComponent::wifi_event_callback_(system_event_id_t event, system_event_info_t info) {
#endif
  switch (event) {
    case SYSTEM_EVENT_WIFI_READY: {
      ESP_LOGV(TAG, "Event: WiFi ready");
      break;
    }
    case SYSTEM_EVENT_SCAN_DONE: {
#if ESP_IDF_VERSION_MAJOR >= 4
      auto it = info.wifi_scan_done;
#else
      auto it = info.scan_done;
#endif
      ESP_LOGV(TAG, "Event: WiFi Scan Done status=%u number=%u scan_id=%u", it.status, it.number, it.scan_id);
      break;
    }
    case SYSTEM_EVENT_STA_START: {
      ESP_LOGV(TAG, "Event: WiFi STA start");
      tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, App.get_name().c_str());
      break;
    }
    case SYSTEM_EVENT_STA_STOP: {
      ESP_LOGV(TAG, "Event: WiFi STA stop");
      break;
    }
    case SYSTEM_EVENT_STA_CONNECTED: {
#if ESP_IDF_VERSION_MAJOR >= 4
      auto it = info.wifi_sta_connected;
#else
      auto it = info.connected;
#endif
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      ESP_LOGV(TAG, "Event: Connected ssid='%s' bssid=" LOG_SECRET("%s") " channel=%u, authmode=%s", buf,
               format_mac_addr(it.bssid).c_str(), it.channel, get_auth_mode_str(it.authmode));
      break;
    }
    case SYSTEM_EVENT_STA_DISCONNECTED: {
#if ESP_IDF_VERSION_MAJOR >= 4
      auto it = info.wifi_sta_disconnected;
#else
      auto it = info.disconnected;
#endif
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      if (it.reason == WIFI_REASON_NO_AP_FOUND) {
        ESP_LOGW(TAG, "Event: Disconnected ssid='%s' reason='Probe Request Unsuccessful'", buf);
      } else {
        ESP_LOGW(TAG, "Event: Disconnected ssid='%s' bssid=" LOG_SECRET("%s") " reason='%s'", buf,
                 format_mac_addr(it.bssid).c_str(), get_disconnect_reason_str(it.reason));
      }
      break;
    }
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE: {
#if ESP_IDF_VERSION_MAJOR >= 4
      auto it = info.wifi_sta_authmode_change;
#else
      auto it = info.auth_change;
#endif
      ESP_LOGV(TAG, "Event: Authmode Change old=%s new=%s", get_auth_mode_str(it.old_mode),
               get_auth_mode_str(it.new_mode));
      // Mitigate CVE-2020-12638
      // https://lbsfilm.at/blog/wpa2-authenticationmode-downgrade-in-espressif-microprocessors
      if (it.old_mode != WIFI_AUTH_OPEN && it.new_mode == WIFI_AUTH_OPEN) {
        ESP_LOGW(TAG, "Potential Authmode downgrade detected, disconnecting...");
        // we can't call retry_connect() from this context, so disconnect immediately
        // and notify main thread with error_from_callback_
        err_t err = esp_wifi_disconnect();
        if (err != ESP_OK) {
          ESP_LOGW(TAG, "Disconnect failed: %s", esp_err_to_name(err));
        }
        this->error_from_callback_ = true;
      }
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
#if ESP_IDF_VERSION_MAJOR >= 4
      auto it = info.wifi_sta_connected;
      auto &mac = it.bssid;
#else
      auto it = info.sta_connected;
      auto &mac = it.mac;
#endif
      ESP_LOGV(TAG, "Event: AP client connected MAC=%s", format_mac_addr(mac).c_str());
      break;
    }
    case SYSTEM_EVENT_AP_STADISCONNECTED: {
#if ESP_IDF_VERSION_MAJOR >= 4
      auto it = info.wifi_sta_disconnected;
      auto &mac = it.bssid;
#else
      auto it = info.sta_disconnected;
      auto &mac = it.mac;
#endif
      ESP_LOGV(TAG, "Event: AP client disconnected MAC=%s", format_mac_addr(mac).c_str());
      break;
    }
    case SYSTEM_EVENT_AP_STAIPASSIGNED: {
      ESP_LOGV(TAG, "Event: AP client assigned IP");
      break;
    }
    case SYSTEM_EVENT_AP_PROBEREQRECVED: {
#if ESP_IDF_VERSION_MAJOR >= 4
      auto it = info.wifi_ap_probereqrecved;
#else
      auto it = info.ap_probereqrecved;
#endif
      ESP_LOGVV(TAG, "Event: AP receive Probe Request MAC=%s RSSI=%d", format_mac_addr(it.mac).c_str(), it.rssi);
      break;
    }
    default:
      break;
  }

#if ESP_IDF_VERSION_MAJOR >= 4
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    uint8_t reason = info.wifi_sta_disconnected.reason;
#else
  if (event == SYSTEM_EVENT_STA_DISCONNECTED) {
    uint8_t reason = info.disconnected.reason;
#endif
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
#if ESP_IDF_VERSION_MAJOR >= 4
  if (event == ARDUINO_EVENT_WIFI_SCAN_DONE) {
#else
  if (event == SYSTEM_EVENT_SCAN_DONE) {
#endif
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

#if ESP_IDF_VERSION_MAJOR >= 4
  // pairwise cipher of SoftAP, group cipher will be derived using this.
  conf.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
#endif

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
