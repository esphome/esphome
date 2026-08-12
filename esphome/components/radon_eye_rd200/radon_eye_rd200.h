// RD200 radon sensor on the platform-neutral ble_client node interface -
// one implementation for every platform with a GATT client engine (esp32
// and rp2 / Pico W today).
//
// Poll cycle: enable the client (it connects on the peer's next sighting) →
// resolve the V1/V2 variant and handles from the service table during
// on_connected() → local notify registration → explicit CCCD write (the
// contract makes the CCCD the node's job) → write the read command → parse
// the notification → disable. The link is dropped after every reading so the
// vendor mobile app can connect between polls.

#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_CLIENT_GATT_NODES

#include "esphome/components/ble_client/ble_client_node.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32
#include "esphome/components/ble_client/ble_client.h"
#else
#include "esphome/components/ble_client/ble_client_gatt.h"
#endif

namespace esphome::radon_eye_rd200 {

class RadonEyeRD200 final : public PollingComponent, public ble_client::BLEClientNode {
 public:
  RadonEyeRD200();

  void dump_config() override;
  void update() override;

  void set_radon(sensor::Sensor *radon) { this->radon_sensor_ = radon; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { this->radon_long_term_sensor_ = radon_long_term; }

  // ---- ble_client::BLEClientNode (unused events keep the no-op defaults) ----
  void on_connected(const ble_device_base::GattServiceTable &table) override;
  void on_notify(uint16_t handle, const uint8_t *data, uint16_t len) override;
  void on_notify_state(uint16_t handle, bool enabled, int error) override;
  void on_write_result(uint16_t handle, int error) override;

 protected:
  bool resolve_handles_(const ble_device_base::GattServiceTable &table);
  void read_sensors_(const uint8_t *value, uint16_t value_len);

  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radon_long_term_sensor_{nullptr};

  uint16_t read_handle_{0};
  uint16_t write_handle_{0};
  uint16_t cccd_handle_{0};
  uint8_t write_command_{0};
};

}  // namespace esphome::radon_eye_rd200

#endif  // USE_BLE_CLIENT_GATT_NODES
