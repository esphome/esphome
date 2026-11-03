// Host stand-ins for the platform hooks wifi_component.cpp calls, so the retry logic links and runs in a gtest.
#include "host_hooks.h"

#ifdef USE_WIFI

namespace esphome::wifi {

namespace testing {
HostHooks host_hooks;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace testing

using testing::host_hooks;

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  if (sta.has_value() && !*sta)
    host_hooks.station_offs++;
  return true;
}
bool WiFiComponent::wifi_sta_pre_setup_() { return true; }
bool WiFiComponent::wifi_apply_output_power_(float output_power) { return true; }
bool WiFiComponent::wifi_apply_power_save_() { return true; }
bool WiFiComponent::wifi_apply_hostname_() { return true; }
void WiFiComponent::wifi_pre_setup_() {}
bool WiFiComponent::wifi_loop_() { return false; }

bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  host_hooks.connects++;
  host_hooks.last_connect = ap;
  return true;
}

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() const { return WiFiSTAConnectStatus::IDLE; }
// Completes at once with the results the test set, as a platform scan-done callback would
bool WiFiComponent::wifi_scan_start_(bool passive) {
  this->scan_result_.init(host_hooks.scan_results.size());
  for (const auto &res : host_hooks.scan_results)
    this->scan_result_.push_back(res);
  this->scan_done_ = true;
  return true;
}
bool WiFiComponent::wifi_disconnect_() { return true; }

network::IPAddresses WiFiComponent::wifi_sta_ip_addresses() { return {}; }
const char *WiFiComponent::wifi_ssid_to(std::span<char, SSID_BUFFER_SIZE> buffer) {
  buffer[0] = '\0';
  return buffer.data();
}
bssid_t WiFiComponent::wifi_bssid() { return {}; }
int8_t WiFiComponent::wifi_rssi() { return WIFI_RSSI_DISCONNECTED; }
int32_t WiFiComponent::get_wifi_channel() { return 0; }
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return {}; }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return {}; }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) { return {}; }

}  // namespace esphome::wifi

#endif  // USE_WIFI
