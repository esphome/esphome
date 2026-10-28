#pragma once

#include <vector>

#include "esphome/components/wifi/wifi_component.h"

namespace esphome::wifi::testing {

/// Feeds and records the stubbed platform hooks in host_hooks.cpp; tests reset it.
struct HostHooks {
  /// What the next scan finds.
  std::vector<WiFiScanResult> scan_results;
  int connects{0};
  WiFiAP last_connect{};
  /// Times the station was switched off, as restart_adapter() does.
  int station_offs{0};
};

extern HostHooks host_hooks;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::wifi::testing
