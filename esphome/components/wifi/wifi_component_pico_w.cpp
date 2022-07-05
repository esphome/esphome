
#include "wifi_component.h"

#ifdef USE_RP2040

#include "cyw43.h"
#include "pico/cyw43_arch.h"

#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netif.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

namespace esphome {
namespace wifi {

static const char *const TAG = "wifi_pico_w";

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  if (sta.has_value()) {
    if (sta.value()) {
      cyw43_arch_enable_sta_mode();
    }
  }
  return true;
}
bool WiFiComponent::wifi_apply_power_save_() { return true; }
bool WiFiComponent::wifi_apply_output_power_(float output_power) { return true; }

bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  uint32_t auth_mode = CYW43_AUTH_OPEN;
  const char *pass = nullptr;
  if (!ap.get_password().empty()) {
    pass = ap.get_password().c_str();
    auth_mode = CYW43_AUTH_WPA2_MIXED_PSK;
  }

  cyw43_arch_wifi_connect_async(ap.get_ssid().c_str(), pass, auth_mode);
  return true;
}
bool WiFiComponent::wifi_sta_pre_setup_() { return this->wifi_mode_(true, {}); }

bool WiFiComponent::wifi_sta_ip_config_(optional<ManualIP> manual_ip) {
  if (!manual_ip.has_value()) {
    return true;
  }

  return true;
}

bool WiFiComponent::wifi_apply_hostname_() {
  cyw43_state.netif[CYW43_ITF_STA].hostname = App.get_name().c_str();
  return true;
}
const char *get_auth_mode_str(uint8_t mode) { return "UNKNOWN"; }
const char *get_disconnect_reason_str(uint8_t reason) { return "UNKNOWN"; }

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() {
  int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
  switch (status) {
    case CYW43_LINK_JOIN:
    case CYW43_LINK_NOIP:
      return WiFiSTAConnectStatus::CONNECTING;
    case CYW43_LINK_UP:
      return WiFiSTAConnectStatus::CONNECTED;
    case CYW43_LINK_FAIL:
    case CYW43_LINK_BADAUTH:
      return WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
    case CYW43_LINK_NONET:
      return WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND;
  }
  return WiFiSTAConnectStatus::IDLE;
}

bool WiFiComponent::wifi_scan_start_() { return false; }
bool WiFiComponent::wifi_ap_ip_config_(optional<ManualIP> manual_ip) { return false; }
bool WiFiComponent::wifi_start_ap_(const WiFiAP &ap) {
  if (!this->wifi_mode_({}, true))
    return false;

  const char *ssid = ap.get_ssid().c_str();

  cyw43_wifi_ap_set_ssid(&cyw43_state, strlen(ssid), (const uint8_t *) ssid);

  if (!ap.get_password().empty()) {
    const char *password = ap.get_password().c_str();
    cyw43_wifi_ap_set_password(&cyw43_state, strlen(password), (const uint8_t *) password);
    cyw43_wifi_ap_set_auth(&cyw43_state, CYW43_AUTH_WPA2_MIXED_PSK);
  } else {
    cyw43_wifi_ap_set_auth(&cyw43_state, CYW43_AUTH_OPEN);
  }

  if (ap.get_channel().has_value()) {
    cyw43_wifi_ap_set_channel(&cyw43_state, ap.get_channel().value());
  }

  cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, true, cyw43_arch_get_country_code());
  return true;
}
network::IPAddress WiFiComponent::wifi_soft_ap_ip() {
  return {cyw43_state.netif[CYW43_ITF_AP].ip_addr.addr};
  ;
}

bool WiFiComponent::wifi_disconnect_() { return false; }
bssid_t WiFiComponent::wifi_bssid() { return {}; }
std::string WiFiComponent::wifi_ssid() { return "Test"; }
int8_t WiFiComponent::wifi_rssi() { return 0; }
int32_t WiFiComponent::wifi_channel_() { return 0; }
network::IPAddress WiFiComponent::wifi_sta_ip() { return {cyw43_state.netif[CYW43_ITF_STA].ip_addr.addr}; }
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return {cyw43_state.netif[CYW43_ITF_STA].netmask.addr}; }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return {cyw43_state.netif[CYW43_ITF_STA].gw.addr}; }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) {
  const ip_addr_t *dns_ip = dns_getserver(num);
  return {dns_ip->addr};
}

void WiFiComponent::wifi_loop_() { cyw43_arch_poll(); }

void WiFiComponent::wifi_pre_setup_() {
  if (cyw43_arch_init()) {
    ESP_LOGE(TAG, "Failed to initialize CYW43");
    return;
  }
}

}  // namespace wifi
}  // namespace esphome

#endif
