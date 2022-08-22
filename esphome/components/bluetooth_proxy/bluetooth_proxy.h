#pragma once

#ifdef USE_ESP32

#include <map>

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace bluetooth_proxy {

class BluetoothProxy : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;

 protected:
  void send_api_packet_(const esp32_ble_tracker::ESPBTDevice &device);
};

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
