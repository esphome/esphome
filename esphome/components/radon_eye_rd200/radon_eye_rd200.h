// RD200 radon sensor over the platform-neutral GATT client contract
// (ble_device_base/ble_gatt_client.h). The component owns a dedicated
// backend instance and is its event sink — the first ble_client-family
// component migrated off the esp32-only BLEClientNode, which also enables it
// on every platform with a GATT backend (esp32 and rp2 today).
//
// Poll cycle: connect → discover → resolve handles by UUID from the service
// table → subscribe (local registration, then an explicit CCCD write — the
// contract makes the CCCD the client's job) → write the read command → parse
// the notification → disconnect. The link is dropped after every reading so
// the vendor mobile app can connect between polls.

#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_GATT_CLIENT

#include "esphome/components/ble_device_base/ble_gatt_client.h"
#include "esphome/components/bluetooth_connection/bluetooth_connection_gatt_backend.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::radon_eye_rd200 {

class RadonEyeRD200 final : public PollingComponent {
 public:
  RadonEyeRD200(ble_device_base::BLEGattConnection *backend, uint64_t address) : backend_(backend), address_(address) {
    backend->set_sink(ble_device_base::make_gatt_sink(this));
  }

  void dump_config() override;
  void update() override;

  void set_radon(sensor::Sensor *radon) { this->radon_sensor_ = radon; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { this->radon_long_term_sensor_ = radon_long_term; }

  // ---- backend event sink ----
  void on_connection_state(bool connected, uint16_t mtu, int error);
  void on_service_discovery_done(int error);
  void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) {}
  void on_write_result(uint16_t handle, int error);
  void on_notify_state(uint16_t handle, bool enabled, int error);
  void on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len);
  void on_pairing_result(int status) {}

 protected:
  bool resolve_handles_();
  void read_sensors_(const uint8_t *value, uint16_t value_len);

  // Group 1: pointers first - PollingComponent's size is 4 mod 8, so three
  // pointers bring the 8-byte address to a naturally aligned offset.
  ble_device_base::BLEGattConnection *backend_;
  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radon_long_term_sensor_{nullptr};

  // Group 2: 8-byte types
  uint64_t address_;

  // Group 3: 2-byte types
  uint16_t read_handle_{0};
  uint16_t write_handle_{0};
  uint16_t cccd_handle_{0};

  // Group 4: 1-byte types
  uint8_t write_command_{0};
};

}  // namespace esphome::radon_eye_rd200

#endif  // USE_BLE_GATT_CLIENT
