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

class BluedroidGattClient;
#ifdef USE_BLUETOOTH_PROXY
class BluetoothConnection;
#endif

// One class carries both halves: the tracker's ESPBTClient surface (its
// promote loop owns scan-stop/coex/one-connect-at-a-time and calls the
// virtual connect()/disconnect()) and the neutral contract ops. The
// contract's teardown op is named gatt_disconnect() because the tracker's
// void disconnect() cannot overload with an int-returning twin.
class BluedroidGattClient final : public esp32_ble_tracker::ESPBTClient, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  // Wired by codegen before setup and invariant for the device lifetime.
  void set_listener(ble_device_base::GattClientListener *listener) { this->listener_ = listener; }
  esp32_ble_tracker::ESPBTClient *tracker_client() { return this; }

  // ---- esp32_ble_tracker::ESPBTClient ----
  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void connect() override;
  void disconnect() override;
  bool wants_parsed_advertisements() override { return false; }
  void on_scan_end() override {}
  bool parse_device(const ble_device_base::ESPBTDevice &device) override { return false; }

  // ---- ble_device_base::BLEGattConnection contract ----
  int connect(uint64_t address, uint8_t addr_type);
  int gatt_disconnect();
  int discover_services();
  int read_characteristic(uint16_t handle);
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response);
  int read_descriptor(uint16_t handle);
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len);
  int notify_characteristic(uint16_t handle, bool enable);
  int pair();
  int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);
  // Materialized on demand from Bluedroid's cached database for direct
  // consumers that resolve handles by UUID. The streaming consumer (the
  // proxy wrapper) never calls this - it uses stream_service_batch - so the
  // materializer only compiles when codegen declares a direct consumer
  // (USE_BLE_GATT_SERVICE_TABLE) and proxy-only builds keep the old
  // footprint; a direct consumer's peak is bounded by its one known device.
#ifdef USE_BLE_GATT_SERVICE_TABLE
  ble_device_base::GattServiceTable get_service_table();
#else
  ble_device_base::GattServiceTable get_service_table() { return {}; }
#endif
  void release_services();

#ifdef USE_BLUETOOTH_PROXY
  /// In-place service streamer (the proxy wrapper detects and prefers it):
  /// builds one api response batch directly from Bluedroid's cached database,
  /// so the streaming peak is the response itself - the old esp32 model.
  void stream_service_batch(BluetoothConnection &conn);
#endif

  void set_connection_type(ble_device_base::ConnectionType ct) { this->connection_type_ = ct; }

 protected:
  esp32_ble_tracker::ClientState state_() const { return this->state(); }
  void set_state_(esp32_ble_tracker::ClientState st) { this->set_state(st); }
  bool check_addr_(const esp_bd_addr_t &addr) const;
  void tracker_connect_();
  bool handle_gattc_event_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  void handle_gap_event_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  void handle_open_evt_(esp_ble_gattc_cb_param_t *param);
  void handle_disconnect_evt_(esp_ble_gattc_cb_param_t *param);
  void handle_search_cmpl_();
  void unconditional_disconnect_();
  void set_idle_();
  void set_disconnecting_();
  void report_connection_state_(bool connected, uint16_t mtu, int error);
  esp_err_t update_conn_params_(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout,
                                const char *param_type);
  int check_and_log_error_(const char *operation, esp_err_t err);
  void log_gattc_warning_(const char *operation, int code);
#ifdef USE_BLE_GATT_SERVICE_TABLE
  template<typename ServiceFn, typename CharFn, typename DescFn>
  bool walk_database_(ServiceFn &&on_service, CharFn &&on_char, DescFn &&on_desc);
  bool build_service_table_();
  void free_service_table_();
  ble_device_base::GattServiceTable table_view_() const;
#endif

  // Group 1: pointers / composed objects
  ble_device_base::GattClientListener *listener_{nullptr};
#ifdef USE_BLE_GATT_SERVICE_TABLE
  // One exact-size block carved into the table's three arrays; owned here,
  // freed by release_services(). Null when no table is materialized. The
  // GattServiceTable view is rebuilt from this pointer and the counts on
  // each (cold) get_service_table() call instead of being cached.
  uint8_t *table_storage_{nullptr};
#endif
  // Group 2: 4-byte types
  int gattc_if_{ESP_GATT_IF_NONE};

  // Group 3: arrays
  esp_bd_addr_t remote_bda_{};

  // Group 4: 2-byte types
  uint16_t conn_id_{0xFFFF};
  uint16_t service_total_{0};
  // 256 ms ticks (millis() >> 8), wrap-safe for the 10 s net; a uint16 keeps
  // the object on the 48-byte boundary (a full uint32 costs 4 B of field
  // plus 4 B of per-slot storage padding).
  uint16_t disconnecting_tick_{0};
#ifdef USE_BLE_GATT_SERVICE_TABLE
  // Filled element counts of the materialized table (0 when none).
  uint16_t table_char_total_{0};
  uint16_t table_desc_total_{0};
#endif

  // Group 5: 1-byte types
  // Stored narrow (the enum is 4 bytes); widened at the esp_ble_gattc_open call.
  uint8_t remote_addr_type_{0};
  esp32_ble_tracker::ConnectionType connection_type_{esp32_ble_tracker::ConnectionType::V3_WITHOUT_CACHE};
  uint8_t connection_index_;
  // Set by every release_services(): terminates an in-flight service stream
  // (a partial list must never be sent as authoritative) and, when the cache
  // was cleaned, marks the database unsafe to walk (Bluedroid asserts rather
  // than erroring).
  bool services_released_{false};
  // The connected report waits for the MTU exchange; OPEN_EVT alone would
  // hand HA the default 23.
  bool seen_mtu_{false};
};

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32_BLE && USE_BLE_GATT_CLIENT
