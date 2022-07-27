
#include "wifi_component.h"

#ifdef USE_RP2040

#include "cyw43.h"
#include "cyw43_country.h"
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
      cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true, CYW43_COUNTRY_WORLDWIDE);
    }
  }
  return true;
}

bool WiFiComponent::wifi_apply_power_save_() {
  uint32_t pm;
  switch (this->power_save_) {
    case WIFI_POWER_SAVE_NONE:
      pm = CYW43_PERFORMANCE_PM;
      break;
    case WIFI_POWER_SAVE_LIGHT:
      pm = CYW43_DEFAULT_PM;
      break;
    case WIFI_POWER_SAVE_HIGH:
      pm = CYW43_PERFORMANCE_PM;
      break;
  }
  int ret = cyw43_wifi_pm(&cyw43_state, pm);
  return ret == 0;
}

bool WiFiComponent::wifi_apply_output_power_(float output_power) {
  // TODO:
  return true;
}

bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  uint32_t auth_mode = CYW43_AUTH_OPEN;
  if (!ap.get_password().empty()) {
    auth_mode = CYW43_AUTH_WPA2_MIXED_PSK;
  }

  const uint8_t *bssid = nullptr;
  if (ap.get_bssid().has_value()) {
    bssid = ap.get_bssid().value().data();
  }

  int err = cyw43_wifi_join(&cyw43_state, ap.get_ssid().length(), (const uint8_t *) ap.get_ssid().c_str(),
                            ap.get_password().length(), (const uint8_t *) ap.get_password().c_str(), auth_mode, bssid,
                            ap.get_channel().value_or(0));

  return err == 0;
}
bool WiFiComponent::wifi_sta_pre_setup_() { return this->wifi_mode_(true, {}); }

bool WiFiComponent::wifi_sta_ip_config_(optional<ManualIP> manual_ip) {
  if (!manual_ip.has_value()) {
    return true;
  }

  // TODO:
  return true;
}

bool WiFiComponent::wifi_apply_hostname_() {
  WiFi.setHostname(App.get_name().c_str());
  return true;
}
const char *get_auth_mode_str(uint8_t mode) {
  // TODO:
  return "UNKNOWN";
}
const char *get_disconnect_reason_str(uint8_t reason) {
  // TODO:
  return "UNKNOWN";
}

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() {
  int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
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

int WiFiComponent::s_wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result) {
  global_wifi_component->wifi_scan_result(env, result);
  return 0;
}

void WiFiComponent::wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result) {
  bssid_t bssid;
  std::copy(result->bssid, result->bssid + 6, bssid.begin());
  std::string ssid(reinterpret_cast<const char *>(result->ssid));
  WiFiScanResult res(bssid, ssid, result->channel, result->rssi, result->auth_mode != CYW43_AUTH_OPEN, ssid.empty());
  if (std::find(this->scan_result_.begin(), this->scan_result_.end(), res) == this->scan_result_.end()) {
    this->scan_result_.push_back(res);
  }
}

bool WiFiComponent::wifi_scan_start_() {
  this->scan_result_.clear();
  this->scan_done_ = false;
  cyw43_wifi_scan_options_t scan_options = {0};
  int err = cyw43_wifi_scan(&cyw43_state, &scan_options, nullptr, &s_wifi_scan_result);
  if (err) {
    ESP_LOGV(TAG, "cyw43_wifi_scan failed!");
  }
  return err == 0;
}

bool WiFiComponent::wifi_ap_ip_config_(optional<ManualIP> manual_ip) {
  // TODO:
  return false;
}

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

  cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, true, CYW43_COUNTRY_WORLDWIDE);
  return true;
}
network::IPAddress WiFiComponent::wifi_soft_ap_ip() { return {WiFi.localIP()}; }

bool WiFiComponent::wifi_disconnect_() {
  int err = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
  return err == 0;
}
bssid_t WiFiComponent::wifi_bssid() {
  // TODO:
  return {};
}
std::string WiFiComponent::wifi_ssid() {
  // TODO:
  return "";
}
int8_t WiFiComponent::wifi_rssi() {
  // TODO:
  return 0;
}
int32_t WiFiComponent::wifi_channel_() {
  // TODO:
  return 0;
}
network::IPAddress WiFiComponent::wifi_sta_ip() { return {WiFi.localIP()}; }
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return {WiFi.subnetMask()}; }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return {WiFi.gatewayIP()}; }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) {
  const ip_addr_t *dns_ip = dns_getserver(num);
  return {dns_ip->addr};
}

void WiFiComponent::wifi_loop_() {
  cyw43_arch_poll();

  if (this->state_ == WIFI_COMPONENT_STATE_STA_SCANNING && !cyw43_wifi_scan_active(&cyw43_state)) {
    this->scan_done_ = true;
    ESP_LOGV(TAG, "Scan done!");
  }
}

void WiFiComponent::wifi_pre_setup_() {
  if (cyw43_arch_init()) {
    ESP_LOGE(TAG, "Failed to initialize CYW43");
    return;
  }
}

}  // namespace wifi
}  // namespace esphome

#endif
