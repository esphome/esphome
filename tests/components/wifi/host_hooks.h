#pragma once

#include <cstring>
#include <vector>

#include "esphome/components/wifi/wifi_component.h"

namespace esphome::wifi::testing {

/// Feeds and records the stubbed platform hooks in host_hooks.cpp; tests reset it.
struct HostHooks {
  /// What the next scan finds.
  std::vector<WiFiScanResult> scan_results;
  int scans{0};
  int connects{0};
  WiFiAP last_connect{};
  /// Times the station was switched off, as restart_adapter() does.
  int station_offs{0};
  /// What the next AP start returns, and how many were tried.
  bool ap_start_ok{true};
  int ap_starts{0};
  /// Output power values applied, in order.
  std::vector<float> output_powers;
  /// Times wifi_apply_power_save_() ran.
  int power_save_applies{0};
};

extern HostHooks host_hooks;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

inline WiFiAP make_ap(const char *ssid) {
  WiFiAP ap;
  ap.set_ssid(ssid);
  ap.set_password("password1");
  return ap;
}

inline WiFiScanResult make_result(const char *ssid, const bssid_t &bssid) {
  return WiFiScanResult(bssid, ssid, strlen(ssid), 6, -50, true, false);
}

}  // namespace esphome::wifi::testing
