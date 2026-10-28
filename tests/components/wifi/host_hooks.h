#pragma once

#include "esphome/components/wifi/wifi_component.h"

namespace esphome::wifi::testing {

/// What the stubbed platform hooks in host_hooks.cpp were asked to do; tests reset it.
struct HostHooks {
  int connects{0};
  WiFiAP last_connect{};
};

extern HostHooks host_hooks;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::wifi::testing
