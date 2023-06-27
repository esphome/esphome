#include "wifi_component.h"

#ifdef USE_ESP_IDF

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_event.h>
#include <esp_netif.h>

#include <cinttypes>
#include <utility>
#include <algorithm>
#ifdef USE_WIFI_WPA2_EAP
#include <esp_wpa2.h>
#endif
#include "dhcpserver/dhcpserver.h"
#include "lwip/err.h"
#include "lwip/dns.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"

namespace esphome {
namespace wifi {

static const char *const TAG = "wifi_esp32";

static EventGroupHandle_t s_wifi_event_group;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static QueueHandle_t s_event_queue;            // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static esp_netif_t *s_sta_netif = nullptr;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static esp_netif_t *s_ap_netif = nullptr;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_started = false;             // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_connected = false;           // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_got_ip = false;              // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_ap_started = false;              // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_connect_not_found = false;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_connect_error = false;       // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_connecting = false;          // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_wifi_started = false;            // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

struct IDFWiFiEvent {
  esp_event_base_t event_base;
  int32_t event_id;
  union {
    wifi_event_sta_scan_done_t sta_scan_done;
    wifi_event_sta_connected_t sta_connected;
    wifi_event_sta_disconnected_t sta_disconnected;
    wifi_event_sta_authmode_change_t sta_authmode_change;
    wifi_event_ap_staconnected_t ap_staconnected;
    wifi_event_ap_stadisconnected_t ap_stadisconnected;
    wifi_event_ap_probe_req_rx_t ap_probe_req_rx;
    wifi_event_bss_rssi_low_t bss_rssi_low;
    ip_event_got_ip_t ip_got_ip;
#if LWIP_IPV6
    ip_event_got_ip6_t ip_got_ip6;
#endif
    ip_event_ap_staipassigned_t ip_ap_staipassigned;
  } data;
};

// general design: event handler translates events and pushes them to a queue,
// events get processed in the main loop
void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  IDFWiFiEvent event;
  memset(&event, 0, sizeof(IDFWiFiEvent));
  event.event_base = event_base;
  event.event_id = event_id;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {  // NOLINT(bugprone-branch-clone)
    // no data
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {  // NOLINT(bugprone-branch-clone)
    // no data
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_AUTHMODE_CHANGE) {
    memcpy(&event.data.sta_authmode_change, event_data, sizeof(wifi_event_sta_authmode_change_t));
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    memcpy(&event.data.sta_connected, event_data, sizeof(wifi_event_sta_connected_t));
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    memcpy(&event.data.sta_disconnected, event_data, sizeof(wifi_event_sta_disconnected_t));
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    memcpy(&event.data.ip_got_ip, event_data, sizeof(ip_event_got_ip_t));
#if LWIP_IPV6
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_GOT_IP6) {
    memcpy(&event.data.ip_got_ip6, event_data, sizeof(ip_event_got_ip6_t));
#endif
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {  // NOLINT(bugprone-branch-clone)
    // no data
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
    memcpy(&event.data.sta_scan_done, event_data, sizeof(wifi_event_sta_scan_done_t));
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {  // NOLINT(bugprone-branch-clone)
    // no data
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {  // NOLINT(bugprone-branch-clone)
    // no data
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_PROBEREQRECVED) {
    memcpy(&event.data.ap_probe_req_rx, event_data, sizeof(wifi_event_ap_probe_req_rx_t));
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
    memcpy(&event.data.ap_staconnected, event_data, sizeof(wifi_event_ap_staconnected_t));
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    memcpy(&event.data.ap_stadisconnected, event_data, sizeof(wifi_event_ap_stadisconnected_t));
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
    memcpy(&event.data.ip_ap_staipassigned, event_data, sizeof(ip_event_ap_staipassigned_t));
  } else {
    // did not match any event, don't send anything
    return;
  }

  // copy to heap to keep queue object small
  auto *to_send = new IDFWiFiEvent;  // NOLINT(cppcoreguidelines-owning-memory)
  memcpy(to_send, &event, sizeof(IDFWiFiEvent));
  // don't block, we may miss events but the core can handle that
  if (xQueueSend(s_event_queue, &to_send, 0L) != pdPASS) {
    delete to_send;  // NOLINT(cppcoreguidelines-owning-memory)
  }
}

void WiFiComponent::wifi_pre_setup_() {
#ifdef USE_ESP32_IGNORE_EFUSE_MAC_CRC
  uint8_t mac[6];
  get_mac_address_raw(mac);
  set_mac_address(mac);
  ESP_LOGV(TAG, "Use EFuse MAC without checking CRC: %s", get_mac_address_pretty().c_str());
#endif
  esp_err_t err = esp_netif_init();
  if (err != ERR_OK) {
    ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
    return;
  }
  s_wifi_event_group = xEventGroupCreate();
  if (s_wifi_event_group == nullptr) {
    ESP_LOGE(TAG, "xEventGroupCreate failed");
    return;
  }
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  s_event_queue = xQueueCreate(64, sizeof(IDFWiFiEvent *));
  if (s_event_queue == nullptr) {
    ESP_LOGE(TAG, "xQueueCreate failed");
    return;
  }
  err = esp_event_loop_create_default();
  if (err != ERR_OK) {
    ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
    return;
  }
  esp_event_handler_instance_t instance_wifi_id, instance_ip_id;
  err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr, &instance_wifi_id);
  if (err != ERR_OK) {
    ESP_LOGE(TAG, "esp_event_handler_instance_register failed: %s", esp_err_to_name(err));
    return;
  }
  err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr, &instance_ip_id);
  if (err != ERR_OK) {
    ESP_LOGE(TAG, "esp_event_handler_instance_register failed: %s", esp_err_to_name(err));
    return;
  }

  s_sta_netif = esp_netif_create_default_wifi_sta();
  s_ap_netif = esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  // cfg.nvs_enable = false;
  err = esp_wifi_init(&cfg);
  if (err != ERR_OK) {
    ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
    return;
  }
  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err != ERR_OK) {
    ESP_LOGE(TAG, "esp_wifi_set_storage failed: %s", esp_err_to_name(err));
    return;
  }
}

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  esp_err_t err;
  wifi_mode_t current_mode = WIFI_MODE_NULL;
  if (s_wifi_started) {
    err = esp_wifi_get_mode(&current_mode);
    if (err != ERR_OK) {
      ESP_LOGW(TAG, "esp_wifi_get_mode failed: %s", esp_err_to_name(err));
      return false;
    }
  }
  bool current_sta = current_mode == WIFI_MODE_STA || current_mode == WIFI_MODE_APSTA;
  bool current_ap = current_mode == WIFI_MODE_AP || current_mode == WIFI_MODE_APSTA;

  bool set_sta = sta.has_value() ? *sta : current_sta;
  bool set_ap = ap.has_value() ? *ap : current_ap;

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

  if (set_mode == WIFI_MODE_NULL && s_wifi_started) {
    err = esp_wifi_stop();
    if (err != ESP_OK) {
      ESP_LOGV(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(err));
      return false;
    }
    s_wifi_started = false;
    return true;
  }

  err = esp_wifi_set_mode(set_mode);
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
    return false;
  }

  if (set_mode != WIFI_MODE_NULL && !s_wifi_started) {
    err = esp_wifi_start();
    if (err != ESP_OK) {
      ESP_LOGV(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
      return false;
    }
    s_wifi_started = true;
  }

  return true;
}

bool WiFiComponent::wifi_sta_pre_setup_() { return this->wifi_mode_(true, {}); }

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
  strncpy(reinterpret_cast<char *>(conf.sta.ssid), ap.get_ssid().c_str(), sizeof(conf.sta.ssid));
  strncpy(reinterpret_cast<char *>(conf.sta.password), ap.get_password().c_str(), sizeof(conf.sta.password));

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

#ifdef USE_WIFI_11KV_SUPPORT
  conf.sta.btm_enabled = this->btm_;
  conf.sta.rm_enabled = this->rrm_;
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

#if ESP_IDF_VERSION_MAJOR >= 4
  // Protected Management Frame
  // Device will prefer to connect in PMF mode if other device also advertises PMF capability.
  conf.sta.pmf_cfg.capable = true;
  conf.sta.pmf_cfg.required = false;
#endif

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

  if (memcmp(&current_conf, &conf, sizeof(wifi_config_t)) != 0) {
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

  // Reset flags, do this _before_ wifi_station_connect as the callback method
  // may be called from wifi_station_connect
  s_sta_connecting = true;
  s_sta_connected = false;
  s_sta_got_ip = false;
  s_sta_connect_error = false;
  s_sta_connect_not_found = false;

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
  info.ip.addr = static_cast<uint32_t>(manual_ip->static_ip);
  info.gw.addr = static_cast<uint32_t>(manual_ip->gateway);
  info.netmask.addr = static_cast<uint32_t>(manual_ip->subnet);
  err = esp_netif_dhcpc_stop(s_sta_netif);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    ESP_LOGV(TAG, "esp_netif_dhcpc_stop failed: %s", esp_err_to_name(err));
    return false;
  }
  err = esp_netif_set_ip_info(s_sta_netif, &info);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_netif_set_ip_info failed: %s", esp_err_to_name(err));
    return false;
  }

  esp_netif_dns_info_t dns;
  if (uint32_t(manual_ip->dns1) != 0) {
    dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(manual_ip->dns1);
    esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns);
  }
  if (uint32_t(manual_ip->dns2) != 0) {
    dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(manual_ip->dns2);
    esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_BACKUP, &dns);
  }

  return true;
}

network::IPAddress WiFiComponent::wifi_sta_ip() {
  if (!this->has_sta())
    return {};
  esp_netif_ip_info_t ip;
  esp_err_t err = esp_netif_get_ip_info(s_sta_netif, &ip);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_netif_get_ip_info failed: %s", esp_err_to_name(err));
    return false;
  }
  return {ip.ip.addr};
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

std::string format_ip4_addr(const esp_ip4_addr_t &ip) { return str_snprintf(IPSTR, 15, IP2STR(&ip)); }
#if LWIP_IPV6
std::string format_ip6_addr(const esp_ip6_addr_t &ip) { return str_snprintf(IPV6STR, 39, IPV62STR(ip)); }
#endif
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

void WiFiComponent::wifi_loop_() {
  while (true) {
    IDFWiFiEvent *data;
    if (xQueueReceive(s_event_queue, &data, 0L) != pdTRUE) {
      // no event ready
      break;
    }

    // process event
    wifi_process_event_(data);

    delete data;  // NOLINT(cppcoreguidelines-owning-memory)
  }
}
void WiFiComponent::wifi_process_event_(IDFWiFiEvent *data) {
  esp_err_t err;
  if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_STA_START) {
    ESP_LOGV(TAG, "Event: WiFi STA start");
    // apply hostname
    err = esp_netif_set_hostname(s_sta_netif, App.get_name().c_str());
    if (err != ERR_OK) {
      ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(err));
    }

    s_sta_started = true;
    // re-apply power save mode
    wifi_apply_power_save_();

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_STA_STOP) {
    ESP_LOGV(TAG, "Event: WiFi STA stop");
    s_sta_started = false;

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_STA_AUTHMODE_CHANGE) {
    const auto &it = data->data.sta_authmode_change;
    ESP_LOGV(TAG, "Event: Authmode Change old=%s new=%s", get_auth_mode_str(it.old_mode),
             get_auth_mode_str(it.new_mode));

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_STA_CONNECTED) {
    const auto &it = data->data.sta_connected;
    char buf[33];
    assert(it.ssid_len <= 32);
    memcpy(buf, it.ssid, it.ssid_len);
    buf[it.ssid_len] = '\0';
    ESP_LOGV(TAG, "Event: Connected ssid='%s' bssid=" LOG_SECRET("%s") " channel=%u, authmode=%s", buf,
             format_mac_addr(it.bssid).c_str(), it.channel, get_auth_mode_str(it.authmode));
    s_sta_connected = true;

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_STA_DISCONNECTED) {
    const auto &it = data->data.sta_disconnected;
    char buf[33];
    assert(it.ssid_len <= 32);
    memcpy(buf, it.ssid, it.ssid_len);
    buf[it.ssid_len] = '\0';
    if (it.reason == WIFI_REASON_NO_AP_FOUND) {
      ESP_LOGW(TAG, "Event: Disconnected ssid='%s' reason='Probe Request Unsuccessful'", buf);
      s_sta_connect_not_found = true;

    } else {
      ESP_LOGW(TAG, "Event: Disconnected ssid='%s' bssid=" LOG_SECRET("%s") " reason='%s'", buf,
               format_mac_addr(it.bssid).c_str(), get_disconnect_reason_str(it.reason));
      s_sta_connect_error = true;
    }
    s_sta_connected = false;
    s_sta_connecting = false;
    error_from_callback_ = true;

  } else if (data->event_base == IP_EVENT && data->event_id == IP_EVENT_STA_GOT_IP) {
    const auto &it = data->data.ip_got_ip;
#if LWIP_IPV6_AUTOCONFIG
    esp_netif_create_ip6_linklocal(s_sta_netif);
#endif
    ESP_LOGV(TAG, "Event: Got IP static_ip=%s gateway=%s", format_ip4_addr(it.ip_info.ip).c_str(),
             format_ip4_addr(it.ip_info.gw).c_str());
    s_sta_got_ip = true;

#if LWIP_IPV6
  } else if (data->event_base == IP_EVENT && data->event_id == IP_EVENT_GOT_IP6) {
    const auto &it = data->data.ip_got_ip6;
    ESP_LOGV(TAG, "Event: Got IPv6 address=%s", format_ip6_addr(it.ip6_info.ip).c_str());
#endif

  } else if (data->event_base == IP_EVENT && data->event_id == IP_EVENT_STA_LOST_IP) {
    ESP_LOGV(TAG, "Event: Lost IP");
    s_sta_got_ip = false;

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_SCAN_DONE) {
    const auto &it = data->data.sta_scan_done;
    ESP_LOGV(TAG, "Event: WiFi Scan Done status=%" PRIu32 " number=%u scan_id=%u", it.status, it.number, it.scan_id);

    scan_result_.clear();
    this->scan_done_ = true;
    if (it.status != 0) {
      // scan error
      return;
    }

    uint16_t number = it.number;
    std::vector<wifi_ap_record_t> records(number);
    err = esp_wifi_scan_get_ap_records(&number, records.data());
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_wifi_scan_get_ap_records failed: %s", esp_err_to_name(err));
      return;
    }
    records.resize(number);

    scan_result_.reserve(number);
    for (int i = 0; i < number; i++) {
      auto &record = records[i];
      bssid_t bssid;
      std::copy(record.bssid, record.bssid + 6, bssid.begin());
      std::string ssid(reinterpret_cast<const char *>(record.ssid));
      WiFiScanResult result(bssid, ssid, record.primary, record.rssi, record.authmode != WIFI_AUTH_OPEN, ssid.empty());
      scan_result_.push_back(result);
    }

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_AP_START) {
    ESP_LOGV(TAG, "Event: WiFi AP start");
    s_ap_started = true;

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_AP_STOP) {
    ESP_LOGV(TAG, "Event: WiFi AP stop");
    s_ap_started = false;

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_AP_PROBEREQRECVED) {
    const auto &it = data->data.ap_probe_req_rx;
    ESP_LOGVV(TAG, "Event: AP receive Probe Request MAC=%s RSSI=%d", format_mac_addr(it.mac).c_str(), it.rssi);

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_AP_STACONNECTED) {
    const auto &it = data->data.ap_staconnected;
    ESP_LOGV(TAG, "Event: AP client connected MAC=%s", format_mac_addr(it.mac).c_str());

  } else if (data->event_base == WIFI_EVENT && data->event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    const auto &it = data->data.ap_stadisconnected;
    ESP_LOGV(TAG, "Event: AP client disconnected MAC=%s", format_mac_addr(it.mac).c_str());

  } else if (data->event_base == IP_EVENT && data->event_id == IP_EVENT_AP_STAIPASSIGNED) {
    const auto &it = data->data.ip_ap_staipassigned;
    ESP_LOGV(TAG, "Event: AP client assigned IP %s", format_ip4_addr(it.ip).c_str());
  }
}

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() {
  if (s_sta_connected && s_sta_got_ip) {
    return WiFiSTAConnectStatus::CONNECTED;
  }
  if (s_sta_connect_error) {
    return WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
  }
  if (s_sta_connect_not_found) {
    return WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND;
  }
  if (s_sta_connecting) {
    return WiFiSTAConnectStatus::CONNECTING;
  }
  return WiFiSTAConnectStatus::IDLE;
}
bool WiFiComponent::wifi_scan_start_(bool passive) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  wifi_scan_config_t config{};
  config.ssid = nullptr;
  config.bssid = nullptr;
  config.channel = 0;
  config.show_hidden = true;
  config.scan_type = passive ? WIFI_SCAN_TYPE_PASSIVE : WIFI_SCAN_TYPE_ACTIVE;
  if (passive) {
    config.scan_time.passive = 300;
  } else {
    config.scan_time.active.min = 100;
    config.scan_time.active.max = 300;
  }

  esp_err_t err = esp_wifi_scan_start(&config, false);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
    return false;
  }

  scan_done_ = false;
  return true;
}
bool WiFiComponent::wifi_ap_ip_config_(optional<ManualIP> manual_ip) {
  esp_err_t err;

  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  esp_netif_ip_info_t info;
  if (manual_ip.has_value()) {
    info.ip.addr = static_cast<uint32_t>(manual_ip->static_ip);
    info.gw.addr = static_cast<uint32_t>(manual_ip->gateway);
    info.netmask.addr = static_cast<uint32_t>(manual_ip->subnet);
  } else {
    info.ip.addr = static_cast<uint32_t>(network::IPAddress(192, 168, 4, 1));
    info.gw.addr = static_cast<uint32_t>(network::IPAddress(192, 168, 4, 1));
    info.netmask.addr = static_cast<uint32_t>(network::IPAddress(255, 255, 255, 0));
  }

  err = esp_netif_dhcpc_stop(s_sta_netif);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    ESP_LOGV(TAG, "esp_netif_dhcpc_stop failed: %s", esp_err_to_name(err));
    return false;
  }

  err = esp_netif_set_ip_info(s_sta_netif, &info);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_netif_set_ip_info failed! %d", err);
    return false;
  }

  dhcps_lease_t lease;
  lease.enable = true;
  network::IPAddress start_address = info.ip.addr;
  start_address[3] += 99;
  lease.start_ip.addr = static_cast<uint32_t>(start_address);
  ESP_LOGV(TAG, "DHCP server IP lease start: %s", start_address.str().c_str());
  start_address[3] += 100;
  lease.end_ip.addr = static_cast<uint32_t>(start_address);
  ESP_LOGV(TAG, "DHCP server IP lease end: %s", start_address.str().c_str());
  err = esp_netif_dhcps_option(s_sta_netif, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS, &lease, sizeof(lease));

  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_netif_dhcps_option failed! %d", err);
    return false;
  }

  err = esp_netif_dhcps_start(s_sta_netif);

  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_netif_dhcps_start failed! %d", err);
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
  strncpy(reinterpret_cast<char *>(conf.ap.ssid), ap.get_ssid().c_str(), sizeof(conf.ap.ssid));
  conf.ap.channel = ap.get_channel().value_or(1);
  conf.ap.ssid_hidden = ap.get_ssid().size();
  conf.ap.max_connection = 5;
  conf.ap.beacon_interval = 100;

  if (ap.get_password().empty()) {
    conf.ap.authmode = WIFI_AUTH_OPEN;
    *conf.ap.password = 0;
  } else {
    conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    strncpy(reinterpret_cast<char *>(conf.ap.password), ap.get_password().c_str(), sizeof(conf.ap.password));
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

  if (!this->wifi_ap_ip_config_(ap.get_manual_ip())) {
    ESP_LOGV(TAG, "wifi_ap_ip_config_ failed!");
    return false;
  }

  return true;
}
network::IPAddress WiFiComponent::wifi_soft_ap_ip() {
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(s_sta_netif, &ip);
  return {ip.ip.addr};
}
bool WiFiComponent::wifi_disconnect_() { return esp_wifi_disconnect(); }

bssid_t WiFiComponent::wifi_bssid() {
  wifi_ap_record_t info;
  esp_err_t err = esp_wifi_sta_get_ap_info(&info);
  bssid_t res{};
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_sta_get_ap_info failed: %s", esp_err_to_name(err));
    return res;
  }
  std::copy(info.bssid, info.bssid + 6, res.begin());
  return res;
}
std::string WiFiComponent::wifi_ssid() {
  wifi_ap_record_t info{};
  esp_err_t err = esp_wifi_sta_get_ap_info(&info);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_sta_get_ap_info failed: %s", esp_err_to_name(err));
    return "";
  }
  auto *ssid_s = reinterpret_cast<const char *>(info.ssid);
  size_t len = strnlen(ssid_s, sizeof(info.ssid));
  return {ssid_s, len};
}
int8_t WiFiComponent::wifi_rssi() {
  wifi_ap_record_t info;
  esp_err_t err = esp_wifi_sta_get_ap_info(&info);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_sta_get_ap_info failed: %s", esp_err_to_name(err));
    return 0;
  }
  return info.rssi;
}
int32_t WiFiComponent::wifi_channel_() {
  uint8_t primary;
  wifi_second_chan_t second;
  esp_err_t err = esp_wifi_get_channel(&primary, &second);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_get_channel failed: %s", esp_err_to_name(err));
    return 0;
  }
  return primary;
}
network::IPAddress WiFiComponent::wifi_subnet_mask_() {
  esp_netif_ip_info_t ip;
  esp_err_t err = esp_netif_get_ip_info(s_sta_netif, &ip);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_netif_get_ip_info failed: %s", esp_err_to_name(err));
    return {};
  }
  return {ip.netmask.addr};
}
network::IPAddress WiFiComponent::wifi_gateway_ip_() {
  esp_netif_ip_info_t ip;
  esp_err_t err = esp_netif_get_ip_info(s_sta_netif, &ip);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_netif_get_ip_info failed: %s", esp_err_to_name(err));
    return {};
  }
  return {ip.gw.addr};
}
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) {
  const ip_addr_t *dns_ip = dns_getserver(num);
#if LWIP_IPV6
  return {dns_ip->u_addr.ip4.addr};
#else
  return {dns_ip->addr};
#endif
}

}  // namespace wifi
}  // namespace esphome

#endif
