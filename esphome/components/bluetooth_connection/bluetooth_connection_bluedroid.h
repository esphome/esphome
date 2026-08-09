// Bluedroid (esp32) GATT client backend: the esp32 arm of the
// ble_device_base::BLEGattConnection alias for the hub BluetoothConnection
// wrapper. Not a BLEClientBase: the tracker's promote loop owns
// scan-stop/coex/one-connect-at-a-time, so the contract's connect() only
// parks the address in DISCOVERED and the real esp_ble_gattc_open happens in
// the tracker-invoked shim connect(). The shim exists because the tracker's
// ESPBTClient::disconnect() returns void while the contract's returns int -
// one class cannot carry both.

#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32_BLE) && defined(USE_BLE_GATT_CLIENT)

#include "esphome/components/ble_device_base/ble_gatt_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

namespace esphome::bluetooth_connection {

class BluetoothConnection;
class BluedroidGattClient;

// The tracker-facing half: owns the ClientState the promote loop reads and
// forwards events/commands to the engine.
class BluedroidTrackerShim final : public esp32_ble_tracker::ESPBTClient {
 public:
  explicit BluedroidTrackerShim(BluedroidGattClient *engine) : engine_(engine) {}
  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void connect() override;
  void disconnect() override;
  bool wants_parsed_advertisements() override { return false; }
  void on_scan_end() override {}
  bool parse_device(const ble_device_base::ESPBTDevice &device) override { return false; }

  void schedule_disconnect() { this->want_disconnect_ = true; }

 protected:
  BluedroidGattClient *engine_;
};

class BluedroidGattClient final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void set_listener(BluetoothConnection *listener) { this->listener_ = listener; }
  esp32_ble_tracker::ESPBTClient *tracker_client() { return &this->shim_; }

  // ---- ble_device_base::BLEGattConnection contract ----
  int connect(uint64_t address, uint8_t addr_type);
  int disconnect();
  int discover_services();
  int read_characteristic(uint16_t handle);
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response);
  int read_descriptor(uint16_t handle);
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len);
  int notify_characteristic(uint16_t handle, bool enable);
  int pair();
  int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);
  ble_device_base::GattServiceTable get_service_table(uint16_t first_service);
  void release_services();

  void set_connection_type(esp32_ble_tracker::ConnectionType ct) { this->connection_type_ = ct; }
  bool disconnect_pending() const { return this->shim_.disconnect_pending(); }
  void cancel_pending_disconnect() { this->shim_.cancel_pending_disconnect(); }

 protected:
  friend class BluedroidTrackerShim;

  esp32_ble_tracker::ClientState state_() const { return this->shim_.state(); }
  void set_state_(esp32_ble_tracker::ClientState st) { this->shim_.set_state(st); }
  bool check_addr_(const esp_bd_addr_t &addr) const;
  void tracker_connect_();
  bool handle_gattc_event_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  void handle_gap_event_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  void handle_open_evt_(esp_ble_gattc_cb_param_t *param);
  void handle_disconnect_evt_(esp_ble_gattc_cb_param_t *param);
  void handle_search_cmpl_();
  bool materialize_services_();
  bool build_window_(uint16_t first_service, ble_device_base::GattServiceTable &out);
  uint8_t *ensure_capacity_(uint8_t *&buf, size_t &cap, size_t needed);
  void unconditional_disconnect_();
  void set_idle_();
  void set_disconnecting_();
  void report_connection_state_(bool connected, int error);
  esp_err_t update_conn_params_(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout,
                                const char *param_type);
  int check_and_log_error_(const char *operation, esp_err_t err);
  void log_gattc_warning_(const char *operation, int code);

  // Group 1: pointers / composed objects
  BluedroidTrackerShim shim_{this};
  BluetoothConnection *listener_{nullptr};
  // The full services array (small) lives for the whole streaming window;
  // characteristics/descriptors are materialized per batch into grow-only
  // scratch buffers so the peak stays bounded to one batch (the old
  // streamer's crash-driven budget). All freed by release_services().
  ble_device_base::GattService *services_{nullptr};
  uint8_t *window_{nullptr};
  uint8_t *elems_{nullptr};
  size_t window_cap_{0};
  size_t elems_cap_{0};

  // Group 2: 4-byte types
  int gattc_if_{ESP_GATT_IF_NONE};
  uint32_t disconnecting_started_{0};

  // Group 3: arrays
  esp_bd_addr_t remote_bda_{};

  // Group 4: 2-byte types
  uint16_t conn_id_{0xFFFF};
  uint16_t mtu_{23};

  // Group 5: 1-byte types
  // Stored narrow (the enum is 4 bytes); widened at the esp_ble_gattc_open call.
  uint8_t remote_addr_type_{0};
  esp32_ble_tracker::ConnectionType connection_type_{esp32_ble_tracker::ConnectionType::V3_WITHOUT_CACHE};
  uint8_t connection_index_;
  uint8_t service_total_{0};
  // Set only when release_services() cleans the stack's GATT cache, which no
  // walk may then touch (Bluedroid asserts rather than erroring).
  bool services_released_{false};
  // The connected report waits for the MTU exchange; OPEN_EVT alone would
  // hand HA the default 23.
  bool seen_mtu_{false};
};

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32_BLE && USE_BLE_GATT_CLIENT
