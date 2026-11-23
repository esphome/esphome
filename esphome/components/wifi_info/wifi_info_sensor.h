#pragma once

#include "esphome/core/defines.h"
#if defined(USE_WIFI_CONNECT_STATE_LISTENERS) && defined(USE_SENSOR)
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/wifi/wifi_component.h"
#include <span>

namespace esphome::wifi_info {

class ChannelWiFiInfo final : public Component, public sensor::Sensor, public wifi::WiFiConnectStateListener {
 public:
  void setup() override;
  void dump_config() override;

  // WiFiConnectStateListener interface
  void on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) override;
};

}  // namespace esphome::wifi_info
#endif
