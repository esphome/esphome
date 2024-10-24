#include "wifi_component.h"

#ifdef USE_WIFI
#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <esp_netif.h>
#include <esp_wifi.h>

#include <algorithm>
#include <utility>
#ifdef USE_WIFI_WPA2_EAP
#include <esp_wpa2.h>
#endif
#include "lwip/apps/sntp.h"
#include "lwip/dns.h"
#include "lwip/err.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

namespace esphome {
namespace wifi {

static const char *const TAG = "wifi_esp32";

static esp_netif_t *s_sta_netif = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#ifdef USE_WIFI_AP
static esp_netif_t *s_ap_netif = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif                                     // USE_WIFI_AP

static bool s_sta_connecting = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void WiFiComponent::wifi_pre_setup_() {
  uint8_t mac[6];
  if (has_custom_mac_address()) {
    get_mac_address_raw(mac);
    set_mac_address(mac);
  }
  auto f = std::bind(&WiFiComponent::wifi_event_callback_, this, std::placeholders::_1, std::placeholders::_2);
  WiFi.onEvent(f);
  WiFi.persistent(false);
  // Make sure WiFi is in clean state before anything starts
  this->wifi_mode_(false, false);
}

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  wifi_mode_t current_mode = WiFiClass::getMode();
  bool current_sta = current_mode == WIFI_MODE_STA || current_mode == WIFI_MODE_APSTA;
  bool current_ap = current_mode == WIFI_MODE_AP || current_mode == WIFI_MODE_APSTA;

  bool set_sta = sta.value_or(current_sta);
  bool set_ap = ap.value_or(current_ap);

  wifi_mode_t set_mode;
  if (set_sta && set_ap) {
    set_mode = WIFI_MODE_APSTA;
  } else if (set_sta && !set_ap) {
    set_mode = WIFI_MODE_STA;
  } else if (!set_sta && set_ap) {
    set_mode = WIFI_MODE_AP;
  } else {
    set_mode = WIFI_MODE_NULL;
  }

  if (current_mode == set_mode)
    return true;

  if (set_sta && !current_sta) {
    ESP_LOGV(TAG, "Enabling STA.");
  } else if (!set_sta && current_sta) {
    ESP_LOGV(TAG, "Disabling STA.");
  }
  if (set_ap && !current_ap) {
    ESP_LOGV(TAG, "Enabling AP.");
  } else if (!set_ap && current_ap) {
    ESP_LOGV(TAG, "Disabling AP.");
  }

  bool ret = WiFiClass::mode(set_mode);

  if (!ret) {
    ESP_LOGW(TAG, "Setting WiFi mode failed!");
    return false;
  }

  // WiFiClass::mode above calls esp_netif_create_default_wifi_sta() and
  // esp_netif_create_default_wifi_ap(), which creates the interfaces.
  // s_sta_netif handle is set during ESPHOME_EVENT_ID_WIFI_STA_START event

#ifdef USE_WIFI_AP
  if (set_ap)
    s_ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
#endif

  return ret;
}

bool WiFiComponent::wifi_sta_pre_setup_() {
  if (!this->wifi_mode_(true, {}))
    return false;

  WiFi.setAutoReconnect(false);
  delay(10);
  return true;
}

bool WiFiComponent::wifi_apply_output_power_(float output_power) {
  int8_t val = static_cast<int8_t>(output_power * 4);
  return esp_wifi_set_max_tx_power(val) == ESP_OK;
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

bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#_CPPv417wifi_sta_config_t
  wifi_config_t conf;
  memset(&conf, 0, sizeof(conf));
  snprintf(reinterpret_cast<char *>(conf.sta.ssid), sizeof(conf.sta.ssid), "%s", ap.get_ssid().c_str());
  snprintf(reinterpret_cast<char *>(conf.sta.password), sizeof(conf.sta.password), "%s", ap.get_password().c_str());

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
    conf.sta.bssid_set = true;
    memcpy(conf.sta.bssid, ap.get_bssid()->data(), 6);
  } else {
    conf.sta.bssid_set = false;
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

  // Protected Management Frame
  // Device will prefer to connect in PMF mode if other device also advertises PMF capability.
  conf.sta.pmf_cfg.capable = true;
  conf.sta.pmf_cfg.required = false;

  // note, we do our own filtering
  // The minimum rssi to accept in the fast scan mode
  conf.sta.threshold.rssi = -127;

  conf.sta.threshold.authmode = WIFI_AUTH_OPEN;

  wifi_config_t current_conf;
  esp_err_t err;
  err = esp_wifi_get_config(WIFI_IF_STA, &current_conf);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "esp_wifi_get_config failed: %s", esp_err_to_name(err));
    // can continue
  }

  if (memcmp(&current_conf, &conf, sizeof(wifi_config_t)) != 0) {  // NOLINT
    err = esp_wifi_disconnect();
    if (err != ESP_OK) {
      ESP_LOGV(TAG, "esp_wifi_disconnect failed: %s", esp_err_to_name(err));
      return false;
    }
  }

  err = esp_wifi_set_config(WIFI_IF_STA, &conf);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
    return false;
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
    err = esp_wifi_sta_wpa2_ent_enable();
    if (err != ESP_OK) {
      ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_enable failed! %d", err);
    }
  }
#endif  // USE_WIFI_WPA2_EAP

  this->wifi_apply_hostname_();

  s_sta_connecting = true;

  err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
    return false;
  }

  return true;
}

bool WiFiComponent::wifi_sta_ip_config_(optional<ManualIP> manual_ip) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  esp_netif_dhcp_status_t dhcp_status;
  esp_err_t err = esp_netif_dhcpc_get_status(s_sta_netif, &dhcp_status);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_netif_dhcpc_get_status failed: %s", esp_err_to_name(err));
    return false;
  }

  if (!manual_ip.has_value()) {
    // lwIP starts the SNTP client if it gets an SNTP server from DHCP. We don't need the time, and more importantly,
    // the built-in SNTP client has a memory leak in certain situations. Disable this feature.
    // https://github.com/esphome/issues/issues/2299
    sntp_servermode_dhcp(false);

    // No manual IP is set; use DHCP client
    if (dhcp_status != ESP_NETIF_DHCP_STARTED) {
      err = esp_netif_dhcpc_start(s_sta_netif);
      if (err != ESP_OK) {
        ESP_LOGV(TAG, "Starting DHCP client failed! %d", err);
      }
      return err == ESP_OK;
    }
    return true;
  }

  esp_netif_ip_info_t info;  // struct of ip4_addr_t with ip, netmask, gw
  info.ip = manual_ip->static_ip;
  info.gw = manual_ip->gateway;
  info.netmask = manual_ip->subnet;
  err = esp_netif_dhcpc_stop(s_sta_netif);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    ESP_LOGV(TAG, "Stopping DHCP client failed! %s", esp_err_to_name(err));
  }

  err = esp_netif_set_ip_info(s_sta_netif, &info);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "Setting manual IP info failed! %s", esp_err_to_name(err));
  }

  esp_netif_dns_info_t dns;
  if (manual_ip->dns1.is_set()) {
    dns.ip = manual_ip->dns1;
    esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns);
  }
  if (manual_ip->dns2.is_set()) {
    dns.ip = manual_ip->dns2;
    esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_BACKUP, &dns);
  }

  return true;
}

network::IPAddresses WiFiComponent::wifi_sta_ip_addresses() {
  if (!this->has_sta())
    return {};
  network::IPAddresses addresses;
  esp_netif_ip_info_t ip;
  esp_err_t err = esp_netif_get_ip_info(s_sta_netif, &ip);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_netif_get_ip_info failed: %s", esp_err_to_name(err));
    // TODO: do something smarter
    // return false;
  } else {
    addresses[0] = network::IPAddress(&ip.ip);
  }
#if USE_NETWORK_IPV6
  struct esp_ip6_addr if_ip6s[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  uint8_t count = 0;
  count = esp_netif_get_all_ip6(s_sta_netif, if_ip6s);
  assert(count <= CONFIG_LWIP_IPV6_NUM_ADDRESSES);
  for (int i = 0; i < count; i++) {
    addresses[i + 1] = network::IPAddress(&if_ip6s[i]);
  }
#endif /* USE_NETWORK_IPV6 */
  return addresses;
}

bool WiFiComponent::wifi_apply_hostname_() {
  // setting is done in SYSTEM_EVENT_STA_START callback
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
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3 PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3 PSK";
    case WIFI_AUTH_WAPI_PSK:
      return "WAPI PSK";
    default:
      return "UNKNOWN";
  }
}

using esphome_ip4_addr_t = esp_ip4_addr_t;

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
    case WIFI_REASON_ROAMING:
      return "Station Roaming";
    case WIFI_REASON_UNSPECIFIED:
    default:
      return "Unspecified";
  }
}

void WiFiComponent::wifi_loop_() {}

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
      // apply hostname
      s_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
      esp_err_t err = esp_netif_set_hostname(s_sta_netif, App.get_name().c_str());
      if (err != ERR_OK) {
        ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(err));
      }
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
#if USE_NETWORK_IPV6
      this->set_timeout(100, [] { WiFi.enableIpV6(); });
#endif /* USE_NETWORK_IPV6 */

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
        err_t err = esp_wifi_disconnect();
        if (err != ESP_OK) {
          ESP_LOGV(TAG, "Disconnect failed: %s", esp_err_to_name(err));
        }
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
        err_t err = esp_wifi_disconnect();
        if (err != ESP_OK) {
          ESP_LOGW(TAG, "Disconnect failed: %s", esp_err_to_name(err));
        }
        this->error_from_callback_ = true;
      }
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_GOT_IP: {
      auto it = info.got_ip.ip_info;
      ESP_LOGV(TAG, "Event: Got IP static_ip=%s gateway=%s", format_ip4_addr(it.ip).c_str(),
               format_ip4_addr(it.gw).c_str());
      this->got_ipv4_address_ = true;
#if USE_NETWORK_IPV6
      s_sta_connecting = this->num_ipv6_addresses_ < USE_NETWORK_MIN_IPV6_ADDR_COUNT;
#else
      s_sta_connecting = false;
#endif /* USE_NETWORK_IPV6 */
      break;
    }
#if USE_NETWORK_IPV6
    case ESPHOME_EVENT_ID_WIFI_STA_GOT_IP6: {
      auto it = info.got_ip6.ip6_info;
      ESP_LOGV(TAG, "Got IPv6 address=" IPV6STR, IPV62STR(it.ip));
      this->num_ipv6_addresses_++;
      s_sta_connecting = !(this->got_ipv4_address_ & (this->num_ipv6_addresses_ >= USE_NETWORK_MIN_IPV6_ADDR_COUNT));
      break;
    }
#endif /* USE_NETWORK_IPV6 */
    case ESPHOME_EVENT_ID_WIFI_STA_LOST_IP: {
      ESP_LOGV(TAG, "Event: Lost IP");
      this->got_ipv4_address_ = false;
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

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() {
  auto status = WiFiClass::status();
  if (status == WL_CONNECT_FAILED || status == WL_CONNECTION_LOST) {
    return WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
  }
  if (status == WL_NO_SSID_AVAIL) {
    return WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND;
  }
  if (s_sta_connecting) {
    return WiFiSTAConnectStatus::CONNECTING;
  }
  if (status == WL_CONNECTED) {
    return WiFiSTAConnectStatus::CONNECTED;
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
  esp_err_t err;

  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  esp_netif_ip_info_t info;
  if (manual_ip.has_value()) {
    info.ip = manual_ip->static_ip;
    info.gw = manual_ip->gateway;
    info.netmask = manual_ip->subnet;
  } else {
    info.ip = network::IPAddress(192, 168, 4, 1);
    info.gw = network::IPAddress(192, 168, 4, 1);
    info.netmask = network::IPAddress(255, 255, 255, 0);
  }

  err = esp_netif_dhcps_stop(s_ap_netif);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    ESP_LOGE(TAG, "esp_netif_dhcps_stop failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_netif_set_ip_info(s_ap_netif, &info);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_set_ip_info failed! %d", err);
    return false;
  }

  dhcps_lease_t lease;
  lease.enable = true;
  network::IPAddress start_address = network::IPAddress(&info.ip);
  start_address += 99;
  lease.start_ip = start_address;
  ESP_LOGV(TAG, "DHCP server IP lease start: %s", start_address.str().c_str());
  start_address += 10;
  lease.end_ip = start_address;
  ESP_LOGV(TAG, "DHCP server IP lease end: %s", start_address.str().c_str());
  err = esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS, &lease, sizeof(lease));

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_dhcps_option failed! %d", err);
    return false;
  }

  err = esp_netif_dhcps_start(s_ap_netif);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_dhcps_start failed! %d", err);
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
  snprintf(reinterpret_cast<char *>(conf.ap.ssid), sizeof(conf.ap.ssid), "%s", ap.get_ssid().c_str());
  conf.ap.channel = ap.get_channel().value_or(1);
  conf.ap.ssid_hidden = ap.get_ssid().size();
  conf.ap.max_connection = 5;
  conf.ap.beacon_interval = 100;

  if (ap.get_password().empty()) {
    conf.ap.authmode = WIFI_AUTH_OPEN;
    *conf.ap.password = 0;
  } else {
    conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    snprintf(reinterpret_cast<char *>(conf.ap.password), sizeof(conf.ap.password), "%s", ap.get_password().c_str());
  }

  // pairwise cipher of SoftAP, group cipher will be derived using this.
  conf.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;

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

network::IPAddress WiFiComponent::wifi_soft_ap_ip() {
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(s_ap_netif, &ip);
  return network::IPAddress(&ip.ip);
}
#endif  // USE_WIFI_AP

bool WiFiComponent::wifi_disconnect_() { return esp_wifi_disconnect(); }

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
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return network::IPAddress(WiFi.subnetMask()); }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return network::IPAddress(WiFi.gatewayIP()); }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) { return network::IPAddress(WiFi.dnsIP(num)); }

}  // namespace wifi
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
#endif
