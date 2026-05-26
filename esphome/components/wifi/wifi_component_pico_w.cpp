#include "wifi_component.h"

#ifdef USE_WIFI
#ifdef USE_RP2040

#include <cassert>

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

// Check if STA is fully connected (WiFi joined + has IP address).
// Do NOT use WiFi.status() or WiFi.connected() for this — in AP-only mode they
// unconditionally return true regardless of STA state, causing false positives
// when the fallback AP is active.
static bool wifi_sta_connected() {
  int link = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
  IPAddress local = WiFi.localIP();
  if (link == CYW43_LINK_JOIN && local.isSet()) {
    // Verify the IP is a real STA IP, not the AP's IP leaking through
    IPAddress ap_ip = WiFi.softAPIP();
    if (local == ap_ip) {
      ESP_LOGV(TAG, "wifi_sta_connected: localIP %s matches AP IP, ignoring", local.toString().c_str());
      return false;
    }
    return true;
  }
  return false;
}

// Track previous state for detecting changes
static bool s_sta_was_connected = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_sta_had_ip = false;         // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static size_t s_scan_result_count = 0;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  if (sta.has_value()) {
    if (sta.value()) {
      cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true, CYW43_COUNTRY_WORLDWIDE);
    } else {
      // Leave the STA network so the radio is free for scanning.
      // Use cyw43_wifi_leave directly to avoid corrupting Arduino framework state.
      cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    }
  }

  if (ap.has_value()) {
    if (ap.value()) {
      cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, true, CYW43_COUNTRY_WORLDWIDE);
    } else {
      cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, false, CYW43_COUNTRY_WORLDWIDE);
    }
    this->ap_started_ = ap.value();
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

  // Use beginNoBlock to avoid WiFi.begin()'s additional 2x timeout wait loop on top of
  // CYW43::begin()'s internal blocking join. CYW43::begin() blocks for up to 10 seconds
  // (default timeout) to complete the join - this is required because the LwipIntfDev netif
  // setup depends on begin() succeeding. beginNoBlock() skips the outer wait loop, saving
  // up to 20 additional seconds of blocking per attempt.
  auto ret = WiFi.beginNoBlock(ap.ssid_.c_str(), ap.password_.c_str());
  if (ret == WL_IDLE_STATUS)
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

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() const {
  // Use cyw43_wifi_link_status instead of cyw43_tcpip_link_status because the Arduino
  // framework's __wrap_cyw43_cb_tcpip_init is a no-op — the SDK's internal netif
  // (cyw43_state.netif[]) is never initialized. cyw43_tcpip_link_status checks that netif's
  // flags and would only fall through to cyw43_wifi_link_status when the flags aren't set.
  // Using cyw43_wifi_link_status directly gives us the actual WiFi radio join state.
  int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
  switch (status) {
    case CYW43_LINK_JOIN:
      // WiFi joined, check if STA has an IP address via wifi_sta_connected()
      if (wifi_sta_connected()) {
        return WiFiSTAConnectStatus::CONNECTED;
      }
      return WiFiSTAConnectStatus::CONNECTING;
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
  s_scan_result_count++;

  // CYW43 scan results have ssid as a 32-byte buffer that is NOT null-terminated.
  // Use ssid_len to create a properly terminated copy for string operations.
  uint8_t len = std::min(result->ssid_len, static_cast<uint8_t>(sizeof(result->ssid)));
  char ssid_buf[33];  // 32 max + null terminator
  memcpy(ssid_buf, result->ssid, len);
  ssid_buf[len] = '\0';

  // Skip networks that don't match any configured network (unless full results needed)
  if (!this->needs_full_scan_results_() && !this->matches_configured_network_(ssid_buf, result->bssid)) {
    this->log_discarded_scan_result_(ssid_buf, result->bssid, result->rssi, result->channel);
    return;
  }

  bssid_t bssid;
  std::copy(result->bssid, result->bssid + 6, bssid.begin());
  WiFiScanResult res(bssid, ssid_buf, len, result->channel, result->rssi, result->auth_mode != CYW43_AUTH_OPEN,
                     len == 0);
  if (std::find(this->scan_result_.begin(), this->scan_result_.end(), res) == this->scan_result_.end()) {
    this->scan_result_.push_back(res);
  }
}

bool WiFiComponent::wifi_scan_start_(bool passive) {
  this->scan_result_.clear();
  this->scan_done_ = false;
  s_scan_result_count = 0;
  cyw43_wifi_scan_options_t scan_options = {0};
  scan_options.scan_type = passive ? 1 : 0;
  int err = cyw43_wifi_scan(&cyw43_state, &scan_options, nullptr, &s_wifi_scan_result);
  if (err) {
    ESP_LOGV(TAG, "cyw43_wifi_scan failed");
  }
  return err == 0;
}

#ifdef USE_WIFI_AP
bool WiFiComponent::wifi_ap_ip_config_(const optional<ManualIP> &manual_ip) {
  // AP IP is configured by WiFi.beginAP() internally using defaults (192.168.4.1).
  // Manual AP IP has never worked on RP2040 — WiFi.config() configures the STA
  // interface, not the AP. This is now rejected at config validation time.
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

  // Pass nullptr for empty password — CYW43 uses the password pointer (not length)
  // to choose between OPEN and WPA2 auth mode.
  const char *ap_password = ap.password_.empty() ? nullptr : ap.password_.c_str();
  WiFi.beginAP(ap.ssid_.c_str(), ap_password, ap.has_channel() ? ap.get_channel() : 1);

  return true;
}

network::IPAddress WiFiComponent::wifi_soft_ap_ip() { return {(const ip_addr_t *) WiFi.softAPIP()}; }
#endif  // USE_WIFI_AP

bool WiFiComponent::wifi_disconnect_() {
  // Use cyw43_wifi_leave() directly instead of WiFi.disconnect().
  // WiFi.disconnect() sets _wifiHWInitted=false in the Arduino framework. beginAP()
  // uses _wifiHWInitted to determine AP+STA vs AP-only mode — with it false,
  // beginAP() enters AP-only mode (IP 192.168.42.1) instead of AP_STA mode
  // (IP 192.168.4.1). In AP-only mode, _beginInternal() redirects all subsequent
  // STA connect attempts to beginAP(), creating an infinite loop.
  cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
  return true;
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
int8_t WiFiComponent::wifi_rssi() { return this->is_connected_() ? WiFi.RSSI() : WIFI_RSSI_DISCONNECTED; }
int32_t WiFiComponent::get_wifi_channel() { return WiFi.channel(); }

network::IPAddresses WiFiComponent::wifi_sta_ip_addresses() {
  network::IPAddresses addresses;
  uint8_t index = 0;
  // Filter out AP interface addresses — addrList includes all lwIP netifs.
  // The AP netif IP lingers even after the AP radio is disabled.
  IPAddress ap_ip = WiFi.softAPIP();
  for (auto addr : addrList) {
    IPAddress ip(addr.ipFromNetifNum());
    if (ip == ap_ip) {
      continue;
    }
    assert(index < addresses.size());
    addresses[index++] = ip;
  }
  return addresses;
}
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return {(const ip_addr_t *) WiFi.subnetMask()}; }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return {(const ip_addr_t *) WiFi.gatewayIP()}; }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) {
  const ip_addr_t *dns_ip = dns_getserver(num);
  return network::IPAddress(dns_ip);
}

// Pico W uses polling for connection state detection.
// Connect state listener notifications are deferred until after the state machine
// transitions (in check_connecting_finished) so that conditions like wifi.connected
// return correct values in automations.
bool WiFiComponent::wifi_loop_() {
  // Handle scan completion
  if (this->state_ == WIFI_COMPONENT_STATE_STA_SCANNING && !cyw43_wifi_scan_active(&cyw43_state)) {
    this->scan_done_ = true;
    bool needs_full = this->needs_full_scan_results_();
    ESP_LOGV(TAG, "Scan complete: %zu found, %zu stored%s", s_scan_result_count, this->scan_result_.size(),
             needs_full ? "" : " (filtered)");
#ifdef USE_WIFI_SCAN_RESULTS_LISTENERS
    this->notify_scan_results_listeners_();
#endif
  }

  // Poll for connection state changes
  // The arduino-pico WiFi library doesn't have event callbacks like ESP8266/ESP32,
  // so we need to poll the link status to detect state changes.
  bool is_connected = wifi_sta_connected();

  // Detect connection state change
  if (is_connected && !s_sta_was_connected) {
    // Just connected
    s_sta_was_connected = true;
    ESP_LOGV(TAG, "Connected");
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
    // Defer listener notification until state machine reaches STA_CONNECTED
    // This ensures wifi.connected condition returns true in listener automations
    this->pending_.connect_state = true;
#endif
    // For static IP configurations, notify IP listeners immediately as the IP is already configured
#if defined(USE_WIFI_IP_STATE_LISTENERS) && defined(USE_WIFI_MANUAL_IP)
    if (const WiFiAP *config = this->get_selected_sta_(); config && config->get_manual_ip().has_value()) {
      s_sta_had_ip = true;
      this->notify_ip_state_listeners_();
    }
#endif
  } else if (!is_connected && s_sta_was_connected) {
    // Just disconnected
    s_sta_was_connected = false;
    s_sta_had_ip = false;
    ESP_LOGV(TAG, "Disconnected");
    // Refresh is_connected() cache; driver link status reports disconnected.
    this->update_connected_state_();
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
    this->notify_disconnect_state_listeners_();
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
      this->notify_ip_state_listeners_();
#endif
    }
  }
  return true;
}

void WiFiComponent::wifi_pre_setup_() {}

}  // namespace esphome::wifi
#endif
#endif
