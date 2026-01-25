#include "wifi_component.h"

#ifdef USE_WIFI
#ifdef USE_RP2040

#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include <AddrList.h>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

namespace esphome::wifi {

static const char *const TAG = "wifi_pico_w";

// Track previous state for detecting changes
static bool s_sta_was_connected = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_had_ip = false;         // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  if (sta.has_value()) {
    if (sta.value()) {
      cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true, CYW43_COUNTRY_WORLDWIDE);
    }
  }

  bool ap_state = false;
  if (ap.has_value()) {
    if (ap.value()) {
      cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, true, CYW43_COUNTRY_WORLDWIDE);
      ap_state = true;
    }
  }
  this->ap_started_ = ap_state;
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
      pm = CYW43_AGGRESSIVE_PM;
      break;
  }
  int ret = cyw43_wifi_pm(&cyw43_state, pm);
  bool success = ret == 0;
#ifdef USE_WIFI_POWER_SAVE_LISTENERS
  if (success) {
    for (auto *listener : this->power_save_listeners_) {
      listener->on_wifi_power_save(this->power_save_);
    }
  }
#endif
  return success;
}

// TODO: The driver doesn't seem to have an API for this
bool WiFiComponent::wifi_apply_output_power_(float output_power) { return true; }

bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
#ifdef USE_WIFI_MANUAL_IP
  if (!this->wifi_sta_ip_config_(ap.get_manual_ip()))
    return false;
#else
  if (!this->wifi_sta_ip_config_({}))
    return false;
#endif

  auto ret = WiFi.begin(ap.get_ssid().c_str(), ap.get_password().c_str());
  if (ret != WL_CONNECTED)
    return false;

  return true;
}

bool WiFiComponent::wifi_sta_pre_setup_() { return this->wifi_mode_(true, {}); }

bool WiFiComponent::wifi_sta_ip_config_(const optional<ManualIP> &manual_ip) {
  if (!manual_ip.has_value()) {
    return true;
  }

  IPAddress ip_address = manual_ip->static_ip;
  IPAddress gateway = manual_ip->gateway;
  IPAddress subnet = manual_ip->subnet;

  IPAddress dns = manual_ip->dns1;

  WiFi.config(ip_address, dns, gateway, subnet);
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

bool WiFiComponent::wifi_scan_start_(bool passive) {
  this->scan_result_.clear();
  this->scan_done_ = false;
  cyw43_wifi_scan_options_t scan_options = {0};
  scan_options.scan_type = passive ? 1 : 0;
  int err = cyw43_wifi_scan(&cyw43_state, &scan_options, nullptr, &s_wifi_scan_result);
  if (err) {
    ESP_LOGV(TAG, "cyw43_wifi_scan failed");
  }
  return err == 0;
  return true;
}

#ifdef USE_WIFI_AP
bool WiFiComponent::wifi_ap_ip_config_(const optional<ManualIP> &manual_ip) {
  esphome::network::IPAddress ip_address, gateway, subnet, dns;
  if (manual_ip.has_value()) {
    ip_address = manual_ip->static_ip;
    gateway = manual_ip->gateway;
    subnet = manual_ip->subnet;
    dns = manual_ip->static_ip;
  } else {
    ip_address = network::IPAddress(192, 168, 4, 1);
    gateway = network::IPAddress(192, 168, 4, 1);
    subnet = network::IPAddress(255, 255, 255, 0);
    dns = network::IPAddress(192, 168, 4, 1);
  }
  WiFi.config(ip_address, dns, gateway, subnet);
  return true;
}

bool WiFiComponent::wifi_start_ap_(const WiFiAP &ap) {
  if (!this->wifi_mode_({}, true))
    return false;
#ifdef USE_WIFI_MANUAL_IP
  if (!this->wifi_ap_ip_config_(ap.get_manual_ip())) {
    ESP_LOGV(TAG, "wifi_ap_ip_config_ failed");
    return false;
  }
#else
  if (!this->wifi_ap_ip_config_({})) {
    ESP_LOGV(TAG, "wifi_ap_ip_config_ failed");
    return false;
  }
#endif

  WiFi.beginAP(ap.get_ssid().c_str(), ap.get_password().c_str(), ap.has_channel() ? ap.get_channel() : 1);

  return true;
}

network::IPAddress WiFiComponent::wifi_soft_ap_ip() { return {(const ip_addr_t *) WiFi.localIP()}; }
#endif  // USE_WIFI_AP

bool WiFiComponent::wifi_disconnect_() {
  int err = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
  return err == 0;
}

bssid_t WiFiComponent::wifi_bssid() {
  bssid_t bssid{};
  uint8_t raw_bssid[6];
  WiFi.BSSID(raw_bssid);
  for (size_t i = 0; i < bssid.size(); i++)
    bssid[i] = raw_bssid[i];
  return bssid;
}
std::string WiFiComponent::wifi_ssid() { return WiFi.SSID().c_str(); }
const char *WiFiComponent::wifi_ssid_to(std::span<char, SSID_BUFFER_SIZE> buffer) {
  // TODO: Find direct CYW43 API to avoid Arduino String allocation
  String ssid = WiFi.SSID();
  size_t len = std::min(static_cast<size_t>(ssid.length()), SSID_BUFFER_SIZE - 1);
  memcpy(buffer.data(), ssid.c_str(), len);
  buffer[len] = '\0';
  return buffer.data();
}
int8_t WiFiComponent::wifi_rssi() { return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : WIFI_RSSI_DISCONNECTED; }
int32_t WiFiComponent::get_wifi_channel() { return WiFi.channel(); }

network::IPAddresses WiFiComponent::wifi_sta_ip_addresses() {
  network::IPAddresses addresses;
  uint8_t index = 0;
  for (auto addr : addrList) {
    addresses[index++] = addr.ipFromNetifNum();
  }
  return addresses;
}
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return {(const ip_addr_t *) WiFi.subnetMask()}; }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return {(const ip_addr_t *) WiFi.gatewayIP()}; }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) {
  const ip_addr_t *dns_ip = dns_getserver(num);
  return network::IPAddress(dns_ip);
}

void WiFiComponent::wifi_loop_() {
  // Handle scan completion
  if (this->state_ == WIFI_COMPONENT_STATE_STA_SCANNING && !cyw43_wifi_scan_active(&cyw43_state)) {
    this->scan_done_ = true;
    ESP_LOGV(TAG, "Scan done");
#ifdef USE_WIFI_SCAN_RESULTS_LISTENERS
    for (auto *listener : this->scan_results_listeners_) {
      listener->on_wifi_scan_results(this->scan_result_);
    }
#endif
  }

  // Poll for connection state changes
  // The arduino-pico WiFi library doesn't have event callbacks like ESP8266/ESP32,
  // so we need to poll the link status to detect state changes
  auto status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
  bool is_connected = (status == CYW43_LINK_UP);

  // Detect connection state change
  if (is_connected && !s_sta_was_connected) {
    // Just connected
    s_sta_was_connected = true;
    ESP_LOGV(TAG, "Connected");
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
    String ssid = WiFi.SSID();
    bssid_t bssid = this->wifi_bssid();
    for (auto *listener : this->connect_state_listeners_) {
      listener->on_wifi_connect_state(StringRef(ssid.c_str(), ssid.length()), bssid);
    }
#endif
    // For static IP configurations, notify IP listeners immediately as the IP is already configured
#if defined(USE_WIFI_IP_STATE_LISTENERS) && defined(USE_WIFI_MANUAL_IP)
    if (const WiFiAP *config = this->get_selected_sta_(); config && config->get_manual_ip().has_value()) {
      s_sta_had_ip = true;
      for (auto *listener : this->ip_state_listeners_) {
        listener->on_ip_state(this->wifi_sta_ip_addresses(), this->get_dns_address(0), this->get_dns_address(1));
      }
    }
#endif
  } else if (!is_connected && s_sta_was_connected) {
    // Just disconnected
    s_sta_was_connected = false;
    s_sta_had_ip = false;
    ESP_LOGV(TAG, "Disconnected");
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
    static constexpr uint8_t EMPTY_BSSID[6] = {};
    for (auto *listener : this->connect_state_listeners_) {
      listener->on_wifi_connect_state(StringRef(), EMPTY_BSSID);
    }
#endif
  }

  // Detect IP address changes (only when connected)
  if (is_connected) {
    bool has_ip = false;
    // Check for any IP address (IPv4 or IPv6)
    for (auto addr : addrList) {
      has_ip = true;
      break;
    }

    if (has_ip && !s_sta_had_ip) {
      // Just got IP address
      s_sta_had_ip = true;
      ESP_LOGV(TAG, "Got IP address");
#ifdef USE_WIFI_IP_STATE_LISTENERS
      for (auto *listener : this->ip_state_listeners_) {
        listener->on_ip_state(this->wifi_sta_ip_addresses(), this->get_dns_address(0), this->get_dns_address(1));
      }
#endif
    }
  }
}

void WiFiComponent::wifi_pre_setup_() {}

}  // namespace esphome::wifi
#endif
#endif
