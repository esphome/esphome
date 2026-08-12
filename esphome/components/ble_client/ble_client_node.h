// The single BLEClientNode both ble_client engines share. The neutral
// callback surface is the one interface node components build on; the raw
// esp32 surface below it remains for components that have not migrated yet.

#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_GATT_CLIENT
#include "esphome/components/ble_device_base/ble_gatt_client.h"
#endif
#ifdef USE_ESP32
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#endif

#include <cstdint>

namespace esphome::ble_client {

class BLEClient;

class BLEClientNode {
 public:
#ifdef USE_BLE_CLIENT_GATT_NODES
  // Neutral surface, delivered by both engines. The table is borrowed: copy
  // handles during on_connected(). All nodes see all completions; filter by
  // handle.
  virtual void on_connected(const ble_device_base::GattServiceTable &table) {}
  virtual void on_disconnected() {}
  virtual void on_notify(uint16_t handle, const uint8_t *data, uint16_t len) {}
  virtual void on_notify_state(uint16_t handle, bool enabled, int error) {}
  virtual void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) {}
  virtual void on_write_result(uint16_t handle, int error) {}
  virtual void on_pairing_result(int status) {}
#endif
#ifdef USE_ESP32
  // Legacy raw surface (esp32 engine only); components overriding these are
  // esp32-only until migrated to the neutral surface above.
  virtual void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) {}
  virtual void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {}
  virtual void loop() {}
  // This should be transitioned to Established once the node no longer needs
  // the services/descriptors/characteristics of the parent client. This will
  // allow some memory to be freed.
  // The parent frees the peer's GATT cache once every node reports Established.
  // Never report Established while an operation that reads that cache is outstanding.
  // - esp_ble_gattc_register_for_notify() completes asynchronously.
  // - Register from ESP_GATTC_SEARCH_CMPL_EVT, then set this from ESP_GATTC_REG_FOR_NOTIFY_EVT.
  // - BLEClientBase::register_for_notify() holds the release until the registration completes.
  esp32_ble_tracker::ClientState node_state;
#endif

  BLEClient *parent() const { return this->parent_; }
  void set_ble_client_parent(BLEClient *parent) { this->parent_ = parent; }

 protected:
  BLEClient *parent_{nullptr};
};

}  // namespace esphome::ble_client
