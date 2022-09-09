#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome {
namespace mqtt_room {
class MqttRoom : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_topic(const std::string &topic);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device);
  void send_tracker_update(const std::string id, int rssi, int signal_power);

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  std::string mqtt_topic_;

  // String helper function
  static std::string format_device_name_(const std::string &device_name);
  static std::string format_device_address_(const uint8_t *device_address);

  // Device manufacture UUID
  const esp32_ble_tracker::ESPBTUUID APPLE_UUID = esp32_ble_tracker::ESPBTUUID::from_uint32(0x004C);
  const esp32_ble_tracker::ESPBTUUID SONOS_UUID = esp32_ble_tracker::ESPBTUUID::from_uint32(0x05A7);
  const esp32_ble_tracker::ESPBTUUID SAMSUNG_UUID = esp32_ble_tracker::ESPBTUUID::from_uint32(0x0075);
  const esp32_ble_tracker::ESPBTUUID GOOGLE_UUID = esp32_ble_tracker::ESPBTUUID::from_uint32(0x00E0);
};
}  // namespace mqtt_room
}  // namespace esphome

#endif
