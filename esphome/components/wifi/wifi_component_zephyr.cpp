#include "wifi_component.h"

#ifdef USE_WIFI
#ifdef USE_ZEPHYR

#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/hostname.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include <iterator>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::wifi {

static const char *const TAG = "wifi_zephyr";

// Must match the forward declaration in wifi_component.h -- not in the anonymous namespace below.
struct ZephyrWiFiEvent {
  enum class Type : uint8_t {
    CONNECT_RESULT,
    DISCONNECT_RESULT,
    AP_ENABLE_RESULT,
    AP_DISABLE_RESULT,
    SCAN_RESULT,
    SCAN_DONE,
    IPV4_ADDR_ADD,
#if USE_NETWORK_IPV6
    IPV6_ADDR_ADD,
#endif
  };
  Type type;
  // Only one member is valid, matching type (see wifi_loop_()'s switch) -- union avoids
  // paying ~50 bytes for wifi_scan_result on the other 7 event types.
  union {
    int status;
    wifi_scan_result scan_result;
  };
};

namespace {

net_if *sta_iface = nullptr;              // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
net_if *ap_iface = nullptr;               // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
net_mgmt_event_callback wifi_event_cb{};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
net_mgmt_event_callback ipv4_event_cb{};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#if USE_NETWORK_IPV6
net_mgmt_event_callback ipv6_event_cb{};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

// Written only from the net_mgmt event thread (single producer, enforced via
// CONFIG_NET_MGMT_EVENT_THREAD), read only from the main loop after wifi_loop_()
// drains the event -- no concurrent access window.
WiFiSTAConnectStatus g_sta_status = WiFiSTAConnectStatus::IDLE;  // NOLINT
bool g_ap_enabled = false;                                       // NOLINT
// Power-save requests fail until the driver has actually started (a side effect of
// connect/scan/AP-enable, not interface registration) -- deferred until after.
bool g_wifi_started = false;  // NOLINT
// Set once from WiFiComponent::wifi_pre_setup_(); read from the producer thread to
// drop weak scan results before they ever reach the event queue.
int8_t g_min_scan_rssi = -128;  // NOLINT

// Fixed pool backing event_queue_ instead of heap-allocating each event (a dense scan can
// push 15-20 back to back). next_pool_slot only advances on a successful push, so it can
// never wrap onto a still-live slot -- the queue's ring buffer always keeps one free.
ZephyrWiFiEvent event_pool[17];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
uint8_t next_pool_slot = 0;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

ZephyrWiFiEvent *claim_pool_slot() { return &event_pool[next_pool_slot]; }

void commit_pool_slot() { next_pool_slot = (next_pool_slot + 1) % std::size(event_pool); }

void push_event(ZephyrWiFiEvent::Type type, int status = 0) {
  ZephyrWiFiEvent *event = claim_pool_slot();
  event->type = type;
  event->status = status;
  if (global_wifi_component != nullptr && global_wifi_component->push_zephyr_wifi_event(event)) {
    commit_pool_slot();
  }
}

void wifi_event_handler(net_mgmt_event_callback *cb, uint64_t mgmt_event, net_if *iface) {
  switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT: {
      const auto *status = static_cast<const wifi_status *>(cb->info);
      push_event(ZephyrWiFiEvent::Type::CONNECT_RESULT, status != nullptr ? status->status : -1);
      break;
    }
    case NET_EVENT_WIFI_DISCONNECT_RESULT: {
      const auto *status = static_cast<const wifi_status *>(cb->info);
      push_event(ZephyrWiFiEvent::Type::DISCONNECT_RESULT, status != nullptr ? status->status : -1);
      break;
    }
    case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
      const auto *status = static_cast<const wifi_status *>(cb->info);
      push_event(ZephyrWiFiEvent::Type::AP_ENABLE_RESULT, status != nullptr ? status->status : -1);
      break;
    }
    case NET_EVENT_WIFI_AP_DISABLE_RESULT: {
      const auto *status = static_cast<const wifi_status *>(cb->info);
      push_event(ZephyrWiFiEvent::Type::AP_DISABLE_RESULT, status != nullptr ? status->status : -1);
      break;
    }
    case NET_EVENT_WIFI_SCAN_RESULT: {
      const auto *result = static_cast<const wifi_scan_result *>(cb->info);
      ESP_LOGV(TAG, "Scan result: ssid='%.*s' rssi=%d min=%d", result->ssid_length,
               reinterpret_cast<const char *>(result->ssid), result->rssi, g_min_scan_rssi);
      if (result->rssi < g_min_scan_rssi) {
        break;  // too weak -- drop before it ever reaches the queue
      }
      ZephyrWiFiEvent *event = claim_pool_slot();
      event->type = ZephyrWiFiEvent::Type::SCAN_RESULT;
      event->scan_result = *result;
      if (global_wifi_component != nullptr && global_wifi_component->push_zephyr_wifi_event(event)) {
        commit_pool_slot();
      }
      break;
    }
    case NET_EVENT_WIFI_SCAN_DONE:
      push_event(ZephyrWiFiEvent::Type::SCAN_DONE);
      break;
    default:
      break;
  }
}

void ipv4_event_handler(net_mgmt_event_callback *cb, uint64_t mgmt_event, net_if *iface) {
  if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD && iface == sta_iface) {
    push_event(ZephyrWiFiEvent::Type::IPV4_ADDR_ADD);
  }
}

#if USE_NETWORK_IPV6
void ipv6_event_handler(net_mgmt_event_callback *cb, uint64_t mgmt_event, net_if *iface) {
  if (mgmt_event == NET_EVENT_IPV6_ADDR_ADD && iface == sta_iface) {
    push_event(ZephyrWiFiEvent::Type::IPV6_ADDR_ADD);
  }
}
#endif

#if USE_NETWORK_IPV6
network::IPAddress ipv4_to_ip_address(const in_addr &addr4) {
  in6_addr mapped{};
  net_ipv6_addr_create_v4_mapped(&addr4, &mapped);
  return network::IPAddress(&mapped);
}
#else
network::IPAddress ipv4_to_ip_address(const in_addr &addr4) { return network::IPAddress(&addr4); }
#endif

}  // namespace

bool WiFiComponent::push_zephyr_wifi_event(void *event) {
  return this->event_queue_.push(static_cast<ZephyrWiFiEvent *>(event));
}

void WiFiComponent::wifi_pre_setup_() {
  g_min_scan_rssi = this->min_scan_rssi_;
  net_mgmt_init_event_callback(&wifi_event_cb, wifi_event_handler,
                               NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |
                                   NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT |
                                   NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);
  net_mgmt_add_event_callback(&wifi_event_cb);
  net_mgmt_init_event_callback(&ipv4_event_cb, ipv4_event_handler, NET_EVENT_IPV4_ADDR_ADD);
  net_mgmt_add_event_callback(&ipv4_event_cb);
#if USE_NETWORK_IPV6
  net_mgmt_init_event_callback(&ipv6_event_cb, ipv6_event_handler, NET_EVENT_IPV6_ADDR_ADD);
  net_mgmt_add_event_callback(&ipv6_event_cb);
#endif

  sta_iface = net_if_get_wifi_sta();
  ap_iface = net_if_get_wifi_sap();
}

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  // Zephyr has no STA up/down request -- enabling is implicit in wifi_sta_connect_();
  // disabling approximates ESP-IDF's esp_wifi_stop() with a disconnect, so
  // restart_adapter()'s recovery path isn't a no-op here.
  if (sta.has_value() && !sta.value()) {
    this->wifi_disconnect_();
  }
  if (ap.has_value()) {
    if (ap_iface == nullptr) {
      ESP_LOGE(TAG, "AP interface not available");
      return false;
    }
    if (ap.value() == g_ap_enabled) {
      return true;
    }
    if (ap.value()) {
      // Actual enable happens in wifi_start_ap_() once SSID/password are known;
      // this branch only handles disabling.
      return true;
    }
    int ret = net_mgmt(NET_REQUEST_WIFI_AP_DISABLE, ap_iface, nullptr, 0);
    if (ret != 0) {
      ESP_LOGE(TAG, "NET_REQUEST_WIFI_AP_DISABLE failed: %d", ret);
      return false;
    }
    g_ap_enabled = false;
  }
  return true;
}

bool WiFiComponent::wifi_sta_pre_setup_() { return true; }

// Zephyr's net_mgmt WiFi API has no TX power control request at all.
bool WiFiComponent::wifi_apply_output_power_(float output_power) { return true; }

bool WiFiComponent::wifi_apply_power_save_() {
  if (sta_iface == nullptr) {
    return false;
  }
  if (!g_wifi_started) {
    // Nothing to apply yet -- re-applied once CONNECT_RESULT confirms the driver started
    // (see wifi_loop_()); report success so callers don't log a spurious failure.
    return true;
  }
  wifi_ps_params params{};
  params.type = WIFI_PS_PARAM_STATE;
  switch (this->power_save_) {
    case WIFI_POWER_SAVE_NONE:
      params.enabled = WIFI_PS_DISABLED;
      break;
    case WIFI_POWER_SAVE_LIGHT:
      // No exact equivalent to ESP-IDF's three levels -- Zephyr only has a listen
      // interval, so "light" uses a short one and "high" a longer, more aggressive one.
      params.enabled = WIFI_PS_ENABLED;
      params.mode = WIFI_PS_MODE_WMM;
      params.listen_interval = 1;
      break;
    case WIFI_POWER_SAVE_HIGH:
      params.enabled = WIFI_PS_ENABLED;
      params.mode = WIFI_PS_MODE_WMM;
      params.listen_interval = 10;
      break;
  }
  int ret = net_mgmt(NET_REQUEST_WIFI_PS, sta_iface, &params, sizeof(params));
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

bool WiFiComponent::wifi_sta_ip_config_(const optional<ManualIP> &manual_ip) {
  if (sta_iface == nullptr) {
    return false;
  }
  if (!manual_ip.has_value()) {
    // ESP32_WIFI_STA_AUTO_DHCPV4 (enabled via NET_DHCPV4 in wifi/__init__.py) starts DHCPv4
    // automatically once STA connects -- nothing to do here.
    return true;
  }
  // DHCPv4 auto-starts on connect regardless of manual_ip (NET_DHCPV4 is unconditionally
  // enabled); stop it before adding the static address so it can't race in and overwrite
  // it, matching ESP-IDF's esp_netif_dhcpc_stop(). No-op if the client was never started.
  net_dhcpv4_stop(sta_iface);
  char buf[network::IP_ADDRESS_BUFFER_SIZE];
  in_addr addr{};
  net_addr_pton(AF_INET, manual_ip->static_ip.str_to(buf), &addr);
  net_if_addr *ifaddr = net_if_ipv4_addr_add(sta_iface, &addr, NET_ADDR_MANUAL, 0);
  if (ifaddr == nullptr) {
    ESP_LOGE(TAG, "Failed to set static IP");
    return false;
  }
  in_addr gw{};
  net_addr_pton(AF_INET, manual_ip->gateway.str_to(buf), &gw);
  net_if_ipv4_set_gw(sta_iface, &gw);
  in_addr mask{};
  net_addr_pton(AF_INET, manual_ip->subnet.str_to(buf), &mask);
  net_if_ipv4_set_netmask_by_addr(sta_iface, &addr, &mask);
  return true;
}

bool WiFiComponent::wifi_apply_hostname_() {
#ifdef CONFIG_NET_HOSTNAME_DYNAMIC
  const std::string &name = App.get_name();
  return net_hostname_set(name.c_str(), name.size()) == 0;
#else
  return true;
#endif
}

namespace {

optional<ManualIP> get_manual_ip_or_none(const WiFiAP &ap) {
#ifdef USE_WIFI_MANUAL_IP
  return ap.get_manual_ip();
#else
  return {};
#endif
}

// Takes ssid_/password_ already extracted by the caller rather than a WiFiAP reference --
// WiFiAP only grants field access to WiFiComponent (friend class), not to this free function.
void fill_ssid_and_security(wifi_connect_req_params &params, const CompactString &ssid, const CompactString &password) {
  params.ssid = reinterpret_cast<const uint8_t *>(ssid.c_str());
  params.ssid_length = ssid.size();
  params.band = WIFI_FREQ_BAND_2_4_GHZ;
  if (password.empty()) {
    params.security = WIFI_SECURITY_TYPE_NONE;
  } else {
    params.security = WIFI_SECURITY_TYPE_PSK;
    params.psk = reinterpret_cast<const uint8_t *>(password.c_str());
    params.psk_length = password.size();
  }
}

}  // namespace

bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  if (sta_iface == nullptr) {
    return false;
  }
  if (!this->wifi_sta_ip_config_(get_manual_ip_or_none(ap))) {
    return false;
  }

  wifi_connect_req_params params{};
  fill_ssid_and_security(params, ap.ssid_, ap.password_);
  params.channel = WIFI_CHANNEL_ANY;
  params.timeout = SYS_FOREVER_MS;

  g_sta_status = WiFiSTAConnectStatus::CONNECTING;
  g_wifi_started = true;
#if USE_NETWORK_IPV4
  this->got_ipv4_address_ = false;
#endif
#if USE_NETWORK_IPV6
  this->num_ipv6_addresses_ = 0;
#endif
  int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &params, sizeof(params));
  if (ret != 0) {
    ESP_LOGW(TAG, "NET_REQUEST_WIFI_CONNECT failed: %d", ret);
    g_sta_status = WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
    return false;
  }
  return true;
}

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() const {
#if USE_NETWORK_IPV4
  if (g_sta_status == WiFiSTAConnectStatus::CONNECTED && !this->got_ipv4_address_) {
    return WiFiSTAConnectStatus::CONNECTING;
  }
#endif
#if USE_NETWORK_IPV6 && (USE_NETWORK_MIN_IPV6_ADDR_COUNT > 0)
  if (g_sta_status == WiFiSTAConnectStatus::CONNECTED && this->num_ipv6_addresses_ < USE_NETWORK_MIN_IPV6_ADDR_COUNT) {
    return WiFiSTAConnectStatus::CONNECTING;
  }
#endif
  return g_sta_status;
}

bool WiFiComponent::wifi_scan_start_(bool passive) {
  if (sta_iface == nullptr) {
    return false;
  }
  this->scan_result_.clear();
  this->scan_done_ = false;
  wifi_scan_params params{};
  params.scan_type = passive ? WIFI_SCAN_TYPE_PASSIVE : WIFI_SCAN_TYPE_ACTIVE;
  g_wifi_started = true;
  int ret = net_mgmt(NET_REQUEST_WIFI_SCAN, sta_iface, &params, sizeof(params));
  if (ret != 0) {
    ESP_LOGV(TAG, "NET_REQUEST_WIFI_SCAN failed: %d", ret);
  }
  return ret == 0;
}

#ifdef USE_WIFI_AP
bool WiFiComponent::wifi_ap_ip_config_(const optional<ManualIP> &manual_ip) {
  if (ap_iface == nullptr) {
    return false;
  }
  char buf[network::IP_ADDRESS_BUFFER_SIZE];
  in_addr addr{};
  in_addr mask{};
  if (manual_ip.has_value()) {
    net_addr_pton(AF_INET, manual_ip->static_ip.str_to(buf), &addr);
    net_addr_pton(AF_INET, manual_ip->subnet.str_to(buf), &mask);
  } else {
    net_addr_pton(AF_INET, "192.168.4.1", &addr);
    net_addr_pton(AF_INET, "255.255.255.0", &mask);
  }
  net_if_ipv4_set_gw(ap_iface, &addr);
  net_if_addr *ifaddr = net_if_ipv4_addr_add(ap_iface, &addr, NET_ADDR_MANUAL, 0);
  if (ifaddr == nullptr) {
    ESP_LOGE(TAG, "Failed to set AP IP address");
    return false;
  }
  net_if_ipv4_set_netmask_by_addr(ap_iface, &addr, &mask);
#ifdef CONFIG_NET_DHCPV4_SERVER
  in_addr pool_start = addr;
  // Clamp so the whole pool (base_addr .. base_addr + ADDR_COUNT - 1) stays inside the
  // subnet: .255 is the broadcast address, and starting any later than
  // 254 - (ADDR_COUNT - 1) would let the range overflow past .255 into the next subnet.
  constexpr int max_pool_start = 254 - (CONFIG_NET_DHCPV4_SERVER_ADDR_COUNT - 1);
  pool_start.s4_addr[3] = std::min(pool_start.s4_addr[3] + 10, max_pool_start);
  if (net_dhcpv4_server_start(ap_iface, &pool_start) != 0) {
    ESP_LOGW(TAG, "Failed to start DHCPv4 server on AP interface");
  }
#endif
  return true;
}

bool WiFiComponent::wifi_start_ap_(const WiFiAP &ap) {
  if (ap_iface == nullptr) {
    ESP_LOGE(TAG, "AP interface not available");
    return false;
  }
  if (!this->wifi_ap_ip_config_(get_manual_ip_or_none(ap))) {
    return false;
  }

  wifi_connect_req_params params{};
  fill_ssid_and_security(params, ap.ssid_, ap.password_);
  params.channel = ap.has_channel() ? ap.get_channel() : WIFI_CHANNEL_ANY;

  g_wifi_started = true;
  int ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, ap_iface, &params, sizeof(params));
  if (ret != 0) {
    ESP_LOGE(TAG, "NET_REQUEST_WIFI_AP_ENABLE failed: %d", ret);
    return false;
  }
  g_ap_enabled = true;
  return true;
}

network::IPAddress WiFiComponent::wifi_soft_ap_ip() {
  if (ap_iface == nullptr) {
    return {};
  }
  net_if_ipv4 *ipv4 = ap_iface->config.ip.ipv4;
  if (ipv4 == nullptr) {
    return {};
  }
  for (const auto &unicast : ipv4->unicast) {
    if (unicast.ipv4.is_used) {
      return ipv4_to_ip_address(unicast.ipv4.address.in_addr);
    }
  }
  return {};
}
#endif  // USE_WIFI_AP

bool WiFiComponent::wifi_disconnect_() {
  if (sta_iface == nullptr) {
    return false;
  }
  int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, sta_iface, nullptr, 0);
  return ret == 0;
}

namespace {
bool get_iface_status(net_if *iface, wifi_iface_status *status) {
  if (iface == nullptr) {
    return false;
  }
  return net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, status, sizeof(*status)) == 0;
}
}  // namespace

bssid_t WiFiComponent::wifi_bssid() {
  bssid_t bssid{};
  wifi_iface_status status{};
  if (get_iface_status(sta_iface, &status)) {
    std::copy(std::begin(status.bssid), std::begin(status.bssid) + 6, bssid.begin());
  }
  return bssid;
}

std::string WiFiComponent::wifi_ssid() {
  char buf[SSID_BUFFER_SIZE];
  return this->wifi_ssid_to(buf);
}

const char *WiFiComponent::wifi_ssid_to(std::span<char, SSID_BUFFER_SIZE> buffer) {
  wifi_iface_status status{};
  if (get_iface_status(sta_iface, &status)) {
    size_t len = std::min(static_cast<size_t>(status.ssid_len), SSID_BUFFER_SIZE - 1);
    memcpy(buffer.data(), status.ssid, len);
    buffer[len] = '\0';
  } else {
    buffer[0] = '\0';
  }
  return buffer.data();
}

int8_t WiFiComponent::wifi_rssi() {
  wifi_iface_status status{};
  if (this->is_connected_() && get_iface_status(sta_iface, &status)) {
    return static_cast<int8_t>(status.rssi);
  }
  return WIFI_RSSI_DISCONNECTED;
}

int32_t WiFiComponent::get_wifi_channel() {
  wifi_iface_status status{};
  return get_iface_status(sta_iface, &status) ? static_cast<int32_t>(status.channel) : 0;
}

network::IPAddresses WiFiComponent::wifi_sta_ip_addresses() {
  network::IPAddresses addresses{};
  if (sta_iface == nullptr) {
    return addresses;
  }
  net_if_ipv4 *ipv4 = sta_iface->config.ip.ipv4;
  if (ipv4 == nullptr) {
    return addresses;
  }
  uint8_t index = 0;
  for (const auto &unicast : ipv4->unicast) {
    if (index >= addresses.size()) {
      break;
    }
    if (unicast.ipv4.is_used) {
      addresses[index++] = ipv4_to_ip_address(unicast.ipv4.address.in_addr);
    }
  }
  return addresses;
}

network::IPAddress WiFiComponent::wifi_subnet_mask_() {
  if (sta_iface == nullptr || sta_iface->config.ip.ipv4 == nullptr) {
    return {};
  }
  for (const auto &unicast : sta_iface->config.ip.ipv4->unicast) {
    if (unicast.ipv4.is_used) {
      in_addr mask = net_if_ipv4_get_netmask_by_addr(sta_iface, &unicast.ipv4.address.in_addr);
      return ipv4_to_ip_address(mask);
    }
  }
  return {};
}

network::IPAddress WiFiComponent::wifi_gateway_ip_() {
  if (sta_iface == nullptr || sta_iface->config.ip.ipv4 == nullptr) {
    return {};
  }
  return ipv4_to_ip_address(sta_iface->config.ip.ipv4->gw);
}

network::IPAddress WiFiComponent::wifi_dns_ip_(int num) {
  // Not implemented -- ESPHome's DNS lookups go through getaddrinfo() regardless.
  return {};
}

bool WiFiComponent::wifi_loop_() {
  ZephyrWiFiEvent *event = this->event_queue_.pop();
  if (event == nullptr) {
    return false;
  }

  do {
    switch (event->type) {
      case ZephyrWiFiEvent::Type::CONNECT_RESULT:
        if (event->status == 0) {
          g_sta_status = WiFiSTAConnectStatus::CONNECTED;
          // Deferred from setup() -- the driver wasn't started yet at that point (see
          // wifi_apply_power_save_()'s g_wifi_started guard), so apply it now that it is.
          if (!this->wifi_apply_power_save_()) {
            ESP_LOGV(TAG, "Setting Power Save Option failed");
          }
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
          this->pending_.connect_state = true;
#endif
        } else {
          g_sta_status = WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
        }
        break;
      case ZephyrWiFiEvent::Type::DISCONNECT_RESULT:
        g_sta_status = WiFiSTAConnectStatus::IDLE;
#if USE_NETWORK_IPV4
        this->got_ipv4_address_ = false;
#endif
        this->update_connected_state_();
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
        this->notify_disconnect_state_listeners_();
#endif
        break;
      case ZephyrWiFiEvent::Type::AP_ENABLE_RESULT:
        if (event->status == 0) {
          ESP_LOGD(TAG, "AP enabled");
        } else {
          ESP_LOGE(TAG, "AP enable failed: %d", event->status);
          g_ap_enabled = false;
        }
        break;
      case ZephyrWiFiEvent::Type::AP_DISABLE_RESULT:
        if (event->status == 0) {
          ESP_LOGD(TAG, "AP disabled");
        } else {
          ESP_LOGW(TAG, "AP disable failed: %d", event->status);
          g_ap_enabled = true;
        }
        break;
      case ZephyrWiFiEvent::Type::SCAN_RESULT: {
        const wifi_scan_result &result = event->scan_result;
        char ssid_buf[SSID_BUFFER_SIZE];
        size_t len = std::min(static_cast<size_t>(result.ssid_length), sizeof(ssid_buf) - 1);
        memcpy(ssid_buf, result.ssid, len);
        ssid_buf[len] = '\0';
        if (!this->needs_full_scan_results_() && !this->matches_configured_network_(ssid_buf, result.mac)) {
          this->log_discarded_scan_result_(ssid_buf, result.mac, result.rssi, result.channel);
          break;
        }
        bssid_t bssid{};
        std::copy(std::begin(result.mac), std::begin(result.mac) + 6, bssid.begin());
        WiFiScanResult scan_result(bssid, ssid_buf, len, result.channel, result.rssi,
                                   result.security != WIFI_SECURITY_TYPE_NONE, len == 0);
        if (std::find(this->scan_result_.begin(), this->scan_result_.end(), scan_result) == this->scan_result_.end()) {
          this->scan_result_.push_back(scan_result);
        }
        break;
      }
      case ZephyrWiFiEvent::Type::SCAN_DONE:
        this->scan_done_ = true;
#ifdef USE_WIFI_SCAN_RESULTS_LISTENERS
        this->notify_scan_results_listeners_();
#endif
        break;
      case ZephyrWiFiEvent::Type::IPV4_ADDR_ADD:
#if USE_NETWORK_IPV4
        this->got_ipv4_address_ = true;
#endif
        this->update_connected_state_();
#ifdef USE_WIFI_IP_STATE_LISTENERS
        this->notify_ip_state_listeners_();
#endif
        break;
#if USE_NETWORK_IPV6
      case ZephyrWiFiEvent::Type::IPV6_ADDR_ADD:
        this->num_ipv6_addresses_++;
        this->update_connected_state_();
#ifdef USE_WIFI_IP_STATE_LISTENERS
        this->notify_ip_state_listeners_();
#endif
        break;
#endif
    }
    // event points into the static pool (see push_event()/claim_pool_slot()), not owned here.
  } while ((event = this->event_queue_.pop()) != nullptr);

  uint16_t dropped = this->event_queue_.get_and_reset_dropped_count();
  if (dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u WiFi events due to buffer overflow", dropped);
  }
  return true;
}

}  // namespace esphome::wifi
#endif  // USE_ZEPHYR
#endif  // USE_WIFI
