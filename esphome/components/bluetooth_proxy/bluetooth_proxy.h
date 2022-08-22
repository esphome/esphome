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
  void add_callback(std::function<void(const esp32_ble_tracker::ESPBTDevice &, std::string)> &&callback) {
    this->callback_.add(std::move(callback));
  }
  void dump_config() override;
  void add_device(uint64_t mac_address, const std::string &name) { this->devices_[mac_address] = name; }

  void set_discovery(bool discovery) { this->discovery_ = discovery; }
  bool get_discovery() { return this->discovery_; }

 protected:
  void send_api_packet_(const esp32_ble_tracker::ESPBTDevice &device);

  std::map<uint64_t, std::string> devices_{};
  CallbackManager<void(const esp32_ble_tracker::ESPBTDevice &, std::string)> callback_{};
  bool discovery_{false};
};

class BLEAdvertisementTrigger : public Trigger<const esp32_ble_tracker::ESPBTDevice &, std::string> {
 public:
  explicit BLEAdvertisementTrigger(BluetoothProxy *parent) {
    parent->add_callback(
        [this](const esp32_ble_tracker::ESPBTDevice &device, std::string packet) { this->trigger(device, packet); });
  }
};

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
