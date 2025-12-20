#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/wifi/wifi_component.h"
#ifdef USE_WIFI
namespace esphome::wifi_signal {

#ifdef USE_WIFI_LISTENERS
class WiFiSignalSensor : public sensor::Sensor, public PollingComponent, public wifi::WiFiConnectStateListener {
#else
class WiFiSignalSensor : public sensor::Sensor, public PollingComponent {
#endif
 public:
#ifdef USE_WIFI_LISTENERS
  void setup() override { wifi::global_wifi_component->add_connect_state_listener(this); }
#endif
  void update() override {
    int8_t rssi = wifi::global_wifi_component->wifi_rssi();
    if (rssi != wifi::WIFI_RSSI_DISCONNECTED) {
      this->publish_state(rssi);
    }
  }
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

#ifdef USE_WIFI_LISTENERS
  // WiFiConnectStateListener interface - update RSSI immediately on connect
  void on_wifi_connect_state(const std::string &ssid, const wifi::bssid_t &bssid) override { this->update(); }
#endif
};

}  // namespace esphome::wifi_signal
#endif
