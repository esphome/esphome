#pragma once

#include "esphome/components/wifi/wifi_component.h"

namespace esphome::wifi::testing {

/// What the stubbed platform hooks in host_hooks.cpp were asked to do; tests reset it.
struct HostHooks {
  int scan_starts{0};
  int connects{0};
  int disconnects{0};
  bool last_connect_has_bssid{false};
  bssid_t last_connect_bssid{};
  WiFiSTAConnectStatus connect_status{WiFiSTAConnectStatus::IDLE};
};

extern HostHooks host_hooks;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::wifi::testing
