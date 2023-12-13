#include "wifi_component.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP8266

#include <user_interface.h>

#include <utility>
#include <algorithm>
#ifdef USE_WIFI_WPA2_EAP
#include <wpa2_enterprise.h>
#endif

extern "C" {
#include "lwip/err.h"
#include "lwip/dns.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"  // LWIP_VERSION_
#include "lwip/apps/sntp.h"
#if LWIP_IPV6
#include "lwip/netif.h"  // struct netif
#include <AddrList.h>
#endif
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(3, 0, 0)
#include "LwipDhcpServer.h"
#define wifi_softap_set_dhcps_lease(lease) dhcpSoftAP.set_dhcps_lease(lease)
#define wifi_softap_set_dhcps_lease_time(time) dhcpSoftAP.set_dhcps_lease_time(time)
#define wifi_softap_set_dhcps_offer_option(offer, mode) dhcpSoftAP.set_dhcps_offer_option(offer, mode)
#endif
}

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/util.h"
#include "esphome/core/application.h"

namespace esphome {
namespace wifi {

static const char *const TAG = "wifi_esp8266";

static bool s_sta_connected = false;          // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_got_ip = false;             // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_connect_not_found = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_connect_error = false;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_connecting = false;         // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

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
  wifi_fpm_auto_sleep_set_in_null_mode(1);
  return wifi_set_sleep_type(power_save);
}

#if LWIP_VERSION_MAJOR != 1
/*
  lwip v2 needs to be notified of IP changes, see also
  https://github.com/d-a-v/Arduino/blob/0e7d21e17144cfc5f53c016191daca8723e89ee8/libraries/ESP8266WiFi/src/ESP8266WiFiSTA.cpp#L251
 */
#undef netif_set_addr  // need to call lwIP-v1.4 netif_set_addr()
extern "C" {
struct netif *eagle_lwip_getif(int netif_index);
void netif_set_addr(struct netif *netif, const ip4_addr_t *ip, const ip4_addr_t *netmask, const ip4_addr_t *gw);
};
#endif

bool WiFiComponent::wifi_sta_ip_config_(optional<ManualIP> manual_ip) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  enum dhcp_status dhcp_status = wifi_station_dhcpc_status();
  if (!manual_ip.has_value()) {
    // lwIP starts the SNTP client if it gets an SNTP server from DHCP. We don't need the time, and more importantly,
    // the built-in SNTP client has a memory leak in certain situations. Disable this feature.
    // https://github.com/esphome/issues/issues/2299
    sntp_servermode_dhcp(false);

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

#if LWIP_VERSION_MAJOR != 1
  // get current->previous IP address
  // (check below)
  ip_info previp{};
  wifi_get_ip_info(STATION_IF, &previp);
#endif

  struct ip_info info {};
  info.ip = manual_ip->static_ip;
  info.gw = manual_ip->gateway;
  info.netmask = manual_ip->subnet;

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
  if (manual_ip->dns1.is_set()) {
    dns = manual_ip->dns1;
    dns_setserver(0, &dns);
  }
  if (manual_ip->dns2.is_set()) {
    dns = manual_ip->dns2;
    dns_setserver(1, &dns);
  }

#if LWIP_VERSION_MAJOR != 1
  // trigger address change by calling lwIP-v1.4 api
  // only when ip is already set by other mean (generally dhcp)
  if (previp.ip.addr != 0 && previp.ip.addr != info.ip.addr) {
    netif_set_addr(eagle_lwip_getif(STATION_IF), reinterpret_cast<const ip4_addr_t *>(&info.ip),
                   reinterpret_cast<const ip4_addr_t *>(&info.netmask), reinterpret_cast<const ip4_addr_t *>(&info.gw));
  }
#endif
  return ret;
}

network::IPAddress WiFiComponent::wifi_sta_ip() {
  if (!this->has_sta())
    return {};
  struct ip_info ip {};
  wifi_get_ip_info(STATION_IF, &ip);
  return network::IPAddress(&ip.ip);
}
bool WiFiComponent::wifi_apply_hostname_() {
  const std::string &hostname = App.get_name();
  bool ret = wifi_station_set_hostname(const_cast<char *>(hostname.c_str()));
  if (!ret) {
    ESP_LOGV(TAG, "Setting WiFi Hostname failed!");
  }

  // inform dhcp server of hostname change using dhcp_renew()
  for (netif *intf = netif_list; intf; intf = intf->next) {
    // unconditionally update all known interfaces
#if LWIP_VERSION_MAJOR == 1
    intf->hostname = (char *) wifi_station_get_hostname();
#else
    intf->hostname = wifi_station_get_hostname();
#endif
    if (netif_dhcp_data(intf) != nullptr) {
      // renew already started DHCP leases
      err_t lwipret = dhcp_renew(intf);
      if (lwipret != ERR_OK) {
        ESP_LOGW(TAG, "wifi_apply_hostname_(%s): lwIP error %d on interface %c%c (index %d)", intf->hostname,
                 (int) lwipret, intf->name[0], intf->name[1], intf->num);
        ret = false;
      }
    }
  }

  return ret;
}

bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  this->wifi_disconnect_();

  struct station_config conf {};
  memset(&conf, 0, sizeof(conf));
  strncpy(reinterpret_cast<char *>(conf.ssid), ap.get_ssid().c_str(), sizeof(conf.ssid));
  strncpy(reinterpret_cast<char *>(conf.password), ap.get_password().c_str(), sizeof(conf.password));

  if (ap.get_bssid().has_value()) {
    conf.bssid_set = 1;
    memcpy(conf.bssid, ap.get_bssid()->data(), 6);
  } else {
    conf.bssid_set = 0;
  }

#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 4, 0)
  if (ap.get_password().empty()) {
    conf.threshold.authmode = AUTH_OPEN;
  } else {
    // Only allow auth modes with at least WPA
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

  // setup enterprise authentication if required
#ifdef USE_WIFI_WPA2_EAP
  if (ap.get_eap().has_value()) {
    // note: all certificates and keys have to be null terminated. Lengths are appended by +1 to include \0.
    EAPAuth eap = ap.get_eap().value();
    ret = wifi_station_set_enterprise_identity((uint8_t *) eap.identity.c_str(), eap.identity.length());
    if (ret) {
      ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_identity failed! %d", ret);
    }
    int ca_cert_len = strlen(eap.ca_cert);
    int client_cert_len = strlen(eap.client_cert);
    int client_key_len = strlen(eap.client_key);
    if (ca_cert_len) {
      ret = wifi_station_set_enterprise_ca_cert((uint8_t *) eap.ca_cert, ca_cert_len + 1);
      if (ret) {
        ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_ca_cert failed! %d", ret);
      }
    }
    // workout what type of EAP this is
    // validation is not required as the config tool has already validated it
    if (client_cert_len && client_key_len) {
      // if we have certs, this must be EAP-TLS
      ret = wifi_station_set_enterprise_cert_key((uint8_t *) eap.client_cert, client_cert_len + 1,
                                                 (uint8_t *) eap.client_key, client_key_len + 1,
                                                 (uint8_t *) eap.password.c_str(), strlen(eap.password.c_str()));
      if (ret) {
        ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_cert_key failed! %d", ret);
      }
    } else {
      // in the absence of certs, assume this is username/password based
      ret = wifi_station_set_enterprise_username((uint8_t *) eap.username.c_str(), eap.username.length());
      if (ret) {
        ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_username failed! %d", ret);
      }
      ret = wifi_station_set_enterprise_password((uint8_t *) eap.password.c_str(), eap.password.length());
      if (ret) {
        ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_set_password failed! %d", ret);
      }
    }
    ret = wifi_station_set_wpa2_enterprise_auth(true);
    if (ret) {
      ESP_LOGV(TAG, "esp_wifi_sta_wpa2_ent_enable failed! %d", ret);
    }
  }
#endif  // USE_WIFI_WPA2_EAP

  this->wifi_apply_hostname_();

  // Reset flags, do this _before_ wifi_station_connect as the callback method
  // may be called from wifi_station_connect
  s_sta_connecting = true;
  s_sta_connected = false;
  s_sta_got_ip = false;
  s_sta_connect_error = false;
  s_sta_connect_not_found = false;

  ETS_UART_INTR_DISABLE();
  ret = wifi_station_connect();
  ETS_UART_INTR_ENABLE();
  if (!ret) {
    ESP_LOGV(TAG, "wifi_station_connect failed!");
    return false;
  }

#if ENABLE_IPV6
  for (bool configured = false; !configured;) {
    for (auto addr : addrList) {
      ESP_LOGV(TAG, "Address %s", addr.toString().c_str());
      if ((configured = !addr.isLocal() && addr.isV6())) {
        break;
      }
    }
    delay(500);  // NOLINT
  }
#endif

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

const LogString *get_auth_mode_str(uint8_t mode) {
  switch (mode) {
    case AUTH_OPEN:
      return LOG_STR("OPEN");
    case AUTH_WEP:
      return LOG_STR("WEP");
    case AUTH_WPA_PSK:
      return LOG_STR("WPA PSK");
    case AUTH_WPA2_PSK:
      return LOG_STR("WPA2 PSK");
    case AUTH_WPA_WPA2_PSK:
      return LOG_STR("WPA/WPA2 PSK");
    default:
      return LOG_STR("UNKNOWN");
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
const LogString *get_op_mode_str(uint8_t mode) {
  switch (mode) {
    case WIFI_OFF:
      return LOG_STR("OFF");
    case WIFI_STA:
      return LOG_STR("STA");
    case WIFI_AP:
      return LOG_STR("AP");
    case WIFI_AP_STA:
      return LOG_STR("AP+STA");
    default:
      return LOG_STR("UNKNOWN");
  }
}

const LogString *get_disconnect_reason_str(uint8_t reason) {
  /* If this were one big switch statement, GCC would generate a lookup table for it. However, the values of the
   * REASON_* constants aren't continuous, and GCC will fill in the gap with the default value -- wasting 4 bytes of RAM
   * per entry. As there's ~175 default entries, this wastes 700 bytes of RAM.
   */
  if (reason <= REASON_CIPHER_SUITE_REJECTED) {  // This must be the last constant with a value <200
    switch (reason) {
      case REASON_AUTH_EXPIRE:
        return LOG_STR("Auth Expired");
      case REASON_AUTH_LEAVE:
        return LOG_STR("Auth Leave");
      case REASON_ASSOC_EXPIRE:
        return LOG_STR("Association Expired");
      case REASON_ASSOC_TOOMANY:
        return LOG_STR("Too Many Associations");
      case REASON_NOT_AUTHED:
        return LOG_STR("Not Authenticated");
      case REASON_NOT_ASSOCED:
        return LOG_STR("Not Associated");
      case REASON_ASSOC_LEAVE:
        return LOG_STR("Association Leave");
      case REASON_ASSOC_NOT_AUTHED:
        return LOG_STR("Association not Authenticated");
      case REASON_DISASSOC_PWRCAP_BAD:
        return LOG_STR("Disassociate Power Cap Bad");
      case REASON_DISASSOC_SUPCHAN_BAD:
        return LOG_STR("Disassociate Supported Channel Bad");
      case REASON_IE_INVALID:
        return LOG_STR("IE Invalid");
      case REASON_MIC_FAILURE:
        return LOG_STR("Mic Failure");
      case REASON_4WAY_HANDSHAKE_TIMEOUT:
        return LOG_STR("4-Way Handshake Timeout");
      case REASON_GROUP_KEY_UPDATE_TIMEOUT:
        return LOG_STR("Group Key Update Timeout");
      case REASON_IE_IN_4WAY_DIFFERS:
        return LOG_STR("IE In 4-Way Handshake Differs");
      case REASON_GROUP_CIPHER_INVALID:
        return LOG_STR("Group Cipher Invalid");
      case REASON_PAIRWISE_CIPHER_INVALID:
        return LOG_STR("Pairwise Cipher Invalid");
      case REASON_AKMP_INVALID:
        return LOG_STR("AKMP Invalid");
      case REASON_UNSUPP_RSN_IE_VERSION:
        return LOG_STR("Unsupported RSN IE version");
      case REASON_INVALID_RSN_IE_CAP:
        return LOG_STR("Invalid RSN IE Cap");
      case REASON_802_1X_AUTH_FAILED:
        return LOG_STR("802.1x Authentication Failed");
      case REASON_CIPHER_SUITE_REJECTED:
        return LOG_STR("Cipher Suite Rejected");
    }
  }

  switch (reason) {
    case REASON_BEACON_TIMEOUT:
      return LOG_STR("Beacon Timeout");
    case REASON_NO_AP_FOUND:
      return LOG_STR("AP Not Found");
    case REASON_AUTH_FAIL:
      return LOG_STR("Authentication Failed");
    case REASON_ASSOC_FAIL:
      return LOG_STR("Association Failed");
    case REASON_HANDSHAKE_TIMEOUT:
      return LOG_STR("Handshake Failed");
    case REASON_UNSPECIFIED:
    default:
      return LOG_STR("Unspecified");
  }
}

void WiFiComponent::wifi_event_callback(System_Event_t *event) {
  switch (event->event) {
    case EVENT_STAMODE_CONNECTED: {
      auto it = event->event_info.connected;
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      ESP_LOGV(TAG, "Event: Connected ssid='%s' bssid=%s channel=%u", buf, format_mac_addr(it.bssid).c_str(),
               it.channel);
      s_sta_connected = true;
      break;
    }
    case EVENT_STAMODE_DISCONNECTED: {
      auto it = event->event_info.disconnected;
      char buf[33];
      memcpy(buf, it.ssid, it.ssid_len);
      buf[it.ssid_len] = '\0';
      if (it.reason == REASON_NO_AP_FOUND) {
        ESP_LOGW(TAG, "Event: Disconnected ssid='%s' reason='Probe Request Unsuccessful'", buf);
        s_sta_connect_not_found = true;
      } else {
        ESP_LOGW(TAG, "Event: Disconnected ssid='%s' bssid=" LOG_SECRET("%s") " reason='%s'", buf,
                 format_mac_addr(it.bssid).c_str(), LOG_STR_ARG(get_disconnect_reason_str(it.reason)));
        s_sta_connect_error = true;
      }
      s_sta_connected = false;
      s_sta_connecting = false;
      break;
    }
    case EVENT_STAMODE_AUTHMODE_CHANGE: {
      auto it = event->event_info.auth_change;
      ESP_LOGV(TAG, "Event: Changed AuthMode old=%s new=%s", LOG_STR_ARG(get_auth_mode_str(it.old_mode)),
               LOG_STR_ARG(get_auth_mode_str(it.new_mode)));
      // Mitigate CVE-2020-12638
      // https://lbsfilm.at/blog/wpa2-authenticationmode-downgrade-in-espressif-microprocessors
      if (it.old_mode != AUTH_OPEN && it.new_mode == AUTH_OPEN) {
        ESP_LOGW(TAG, "Potential Authmode downgrade detected, disconnecting...");
        // we can't call retry_connect() from this context, so disconnect immediately
        // and notify main thread with error_from_callback_
        wifi_station_disconnect();
        global_wifi_component->error_from_callback_ = true;
      }
      break;
    }
    case EVENT_STAMODE_GOT_IP: {
      auto it = event->event_info.got_ip;
      ESP_LOGV(TAG, "Event: Got IP static_ip=%s gateway=%s netmask=%s", format_ip_addr(it.ip).c_str(),
               format_ip_addr(it.gw).c_str(), format_ip_addr(it.mask).c_str());
      s_sta_got_ip = true;
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
      ESP_LOGVV(TAG, "Event: AP receive Probe Request MAC=%s RSSI=%d", format_mac_addr(it.mac).c_str(), it.rssi);
      break;
    }
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 4, 0)
    case EVENT_OPMODE_CHANGED: {
      auto it = event->event_info.opmode_changed;
      ESP_LOGV(TAG, "Event: Changed Mode old=%s new=%s", LOG_STR_ARG(get_op_mode_str(it.old_opmode)),
               LOG_STR_ARG(get_op_mode_str(it.new_opmode)));
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

  if (event->event == EVENT_STAMODE_DISCONNECTED) {
    global_wifi_component->error_from_callback_ = true;
  }

  WiFiMockClass::_event_callback(event);
}

bool WiFiComponent::wifi_apply_output_power_(float output_power) {
  uint8_t val = static_cast<uint8_t>(output_power * 4);
  system_phy_set_max_tpw(val);
  return true;
}
bool WiFiComponent::wifi_sta_pre_setup_() {
  if (!this->wifi_mode_(true, {}))
    return false;

  bool ret1, ret2;
  ETS_UART_INTR_DISABLE();
  ret1 = wifi_station_set_auto_connect(0);
  ret2 = wifi_station_set_reconnect_policy(false);
  ETS_UART_INTR_ENABLE();

  if (!ret1 || !ret2) {
    ESP_LOGV(TAG, "Disabling Auto-Connect failed!");
  }

  delay(10);
  return true;
}

void WiFiComponent::wifi_pre_setup_() {
  wifi_set_event_handler_cb(&WiFiComponent::wifi_event_callback);

  // Make sure WiFi is in clean state before anything starts
  this->wifi_mode_(false, false);
}

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() {
  station_status_t status = wifi_station_get_connect_status();
  switch (status) {
    case STATION_GOT_IP:
      return WiFiSTAConnectStatus::CONNECTED;
    case STATION_NO_AP_FOUND:
      return WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND;
      ;
    case STATION_CONNECT_FAIL:
    case STATION_WRONG_PASSWORD:
      return WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
    case STATION_CONNECTING:
      return WiFiSTAConnectStatus::CONNECTING;
    case STATION_IDLE:
    default:
      return WiFiSTAConnectStatus::IDLE;
  }
}
bool WiFiComponent::wifi_scan_start_(bool passive) {
  static bool first_scan = false;

  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  struct scan_config config {};
  memset(&config, 0, sizeof(config));
  config.ssid = nullptr;
  config.bssid = nullptr;
  config.channel = 0;
  config.show_hidden = 1;
#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(2, 4, 0)
  config.scan_type = passive ? WIFI_SCAN_TYPE_PASSIVE : WIFI_SCAN_TYPE_ACTIVE;
  if (first_scan) {
    if (passive) {
      config.scan_time.passive = 200;
    } else {
      config.scan_time.active.min = 100;
      config.scan_time.active.max = 200;
    }
  } else {
    if (passive) {
      config.scan_time.passive = 500;
    } else {
      config.scan_time.active.min = 400;
      config.scan_time.active.max = 500;
    }
  }
#endif
  first_scan = false;
  bool ret = wifi_station_scan(&config, &WiFiComponent::s_wifi_scan_done_callback);
  if (!ret) {
    ESP_LOGV(TAG, "wifi_station_scan failed!");
    return false;
  }

  return ret;
}
bool WiFiComponent::wifi_disconnect_() {
  bool ret = true;
  // Only call disconnect if interface is up
  if (wifi_get_opmode() & WIFI_STA)
    ret = wifi_station_disconnect();
  station_config conf{};
  memset(&conf, 0, sizeof(conf));
  ETS_UART_INTR_DISABLE();
  wifi_station_set_config_current(&conf);
  ETS_UART_INTR_ENABLE();
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

#ifdef USE_WIFI_AP
bool WiFiComponent::wifi_ap_ip_config_(optional<ManualIP> manual_ip) {
  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  struct ip_info info {};
  if (manual_ip.has_value()) {
    info.ip = manual_ip->static_ip;
    info.gw = manual_ip->gateway;
    info.netmask = manual_ip->subnet;
  } else {
    info.ip = network::IPAddress(192, 168, 4, 1);
    info.gw = network::IPAddress(192, 168, 4, 1);
    info.netmask = network::IPAddress(255, 255, 255, 0);
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

#if USE_ARDUINO_VERSION_CODE >= VERSION_CODE(3, 0, 0)
  dhcpSoftAP.begin(&info);
#endif

  struct dhcps_lease lease {};
  lease.enable = true;
  network::IPAddress start_address = network::IPAddress(&info.ip);
  start_address += 99;
  lease.start_ip = start_address;
  ESP_LOGV(TAG, "DHCP server IP lease start: %s", start_address.str().c_str());
  start_address += 100;
  lease.end_ip = start_address;
  ESP_LOGV(TAG, "DHCP server IP lease end: %s", start_address.str().c_str());
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
  strncpy(reinterpret_cast<char *>(conf.ssid), ap.get_ssid().c_str(), sizeof(conf.ssid));
  conf.ssid_len = static_cast<uint8>(ap.get_ssid().size());
  conf.channel = ap.get_channel().value_or(1);
  conf.ssid_hidden = ap.get_hidden();
  conf.max_connection = 5;
  conf.beacon_interval = 100;

  if (ap.get_password().empty()) {
    conf.authmode = AUTH_OPEN;
    *conf.password = 0;
  } else {
    conf.authmode = AUTH_WPA2_PSK;
    strncpy(reinterpret_cast<char *>(conf.password), ap.get_password().c_str(), sizeof(conf.password));
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

network::IPAddress WiFiComponent::wifi_soft_ap_ip() {
  struct ip_info ip {};
  wifi_get_ip_info(SOFTAP_IF, &ip);
  return network::IPAddress(&ip.ip);
}
#endif  // USE_WIFI_AP

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
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return {(const ip_addr_t *) WiFi.subnetMask()}; }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return {(const ip_addr_t *) WiFi.gatewayIP()}; }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) { return {(const ip_addr_t *) WiFi.dnsIP(num)}; }
void WiFiComponent::wifi_loop_() {}

}  // namespace wifi
}  // namespace esphome

#endif
