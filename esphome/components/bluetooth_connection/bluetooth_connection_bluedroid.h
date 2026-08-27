// Bluedroid (esp32) GATT client backend: the esp32 arm of the
// ble_device_base::BLEGattConnection alias for the hub BluetoothConnection
// wrapper. Not a BLEClientBase: the tracker's promote loop owns
// scan-stop/coex/one-connect-at-a-time, so the contract's connect() only
// parks the address in DISCOVERED; the real esp_ble_gattc_open happens in
// the tracker-invoked connect() override.

#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32_BLE) && defined(USE_BLE_GATT_CLIENT)

#include "esphome/components/ble_device_base/ble_gatt_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

namespace esphome::bluetooth_connection {

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
class BluetoothConnection;
#endif

// One class carries both halves: the tracker's ESPBTClient surface (its
// promote loop owns scan-stop/coex/one-connect-at-a-time and calls the
// virtual connect()/disconnect()) and the neutral contract ops. The
// contract's teardown op is named gatt_disconnect() because the tracker's
// void disconnect() cannot overload with an int-returning twin.
class BluedroidGattClient final : public esp32_ble_tracker::ESPBTClient, public Component {
 public:
  static constexpr uint16_t UNSET_CONN_ID = 0xFFFF;

  // Lifecycle of one connection attempt's service search.
  enum class SearchState : uint8_t {
    NONE,           // no search this attempt
    PRESTARTED,     // issued at OPEN_EVT, no claimant yet
    PRESTART_DONE,  // completed with search_status_ latched, no claimant yet
    CLAIMED,        // in flight with a claimant (pre-started or direct)
    REPORT_PENDING  // completed and claimed: deliver on the next flush
  };

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  // Wired by codegen before setup and invariant for the device lifetime.
  void set_listener(ble_device_base::GattClientListener *listener) { this->listener_ = listener; }

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
  bool cancel_gatt_disconnect();
  int discover_services();
  int read_characteristic(uint16_t handle);
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response);
  int read_descriptor(uint16_t handle);
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len);
  int notify_characteristic(uint16_t handle, bool enable);
  int pair();
  int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);
  // Contract stub: the proxy streams in place; the on-demand materializer
  // for direct consumers lands with #18205. NOTE: a direct consumer reaching
  // this stub gets an empty table indistinguishable from a service-less
  // peer - do not ship one against this backend before the materializer.
  ble_device_base::GattServiceTable get_service_table() { return {}; }
  void release_services();

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
  /// In-place service streamer (the proxy wrapper detects and prefers it):
  /// builds one api response batch directly from Bluedroid's cached database,
  /// so the streaming peak is the response itself - the old esp32 model.
  void stream_service_batch(BluetoothConnection &conn);
#endif

  void set_connection_type(ble_device_base::ConnectionType ct) { this->connection_type_ = ct; }

 protected:
  bool check_addr_(const esp_bd_addr_t &addr) const;
  void tracker_connect_();
  void handle_open_evt_(esp_ble_gattc_cb_param_t *param);
  void handle_disconnect_evt_(esp_ble_gattc_cb_param_t *param);
  int handle_search_cmpl_(esp_gatt_status_t status);
  void deliver_pending_search_();
  void unconditional_disconnect_();
  void set_idle_();
  void set_disconnecting_();
  esp_err_t update_conn_params_(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout,
                                const char *param_type);
  int check_and_log_error_(const char *operation, esp_err_t err);
  void log_gattc_warning_(const char *operation, int code);

  // Group 1: pointers / composed objects
  ble_device_base::GattClientListener *listener_{nullptr};
  // Group 2: 4-byte types
  uint32_t disconnecting_started_{0};

  // Group 3: arrays
  esp_bd_addr_t remote_bda_{};

  // Group 4: 2-byte types
  uint16_t conn_id_{UNSET_CONN_ID};
  uint16_t service_total_{0};

  // Group 5: 1-byte types
  esp_gatt_if_t gattc_if_{ESP_GATT_IF_NONE};  // uint8_t width keeps the object at 48 bytes
  // Stored narrow (the enum is 4 bytes); widened at the esp_ble_gattc_open call.
  uint8_t remote_addr_type_{0};
  esp32_ble_tracker::ConnectionType connection_type_{esp32_ble_tracker::ConnectionType::V3_WITHOUT_CACHE};
  uint8_t connection_index_{0};
  // Terminates an in-flight stream (never send a partial list as authoritative)
  // and marks a cleaned cache unsafe to walk (Bluedroid asserts).
  bool services_released_ : 1 {false};
  // The connected report waits for the MTU exchange; OPEN_EVT alone would
  // hand HA the default 23.
  bool seen_mtu_ : 1 {false};
  // The MTU request was refused at CONNECT_EVT; OPEN_EVT reports instead.
  bool mtu_failed_ : 1 {false};
  // Search issued at OPEN_EVT overlaps the MTU exchange; discover_services()
  // completes from it. Reset by set_idle_().
  static_assert(static_cast<uint8_t>(SearchState::REPORT_PENDING) < (1 << 4), "search_state_ bitfield too narrow");
  SearchState search_state_ : 4 {SearchState::NONE};
  // esp_gatt_status_t of the completed search, held until claimed.
  uint8_t search_status_{0};
};

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32_BLE && USE_BLE_GATT_CLIENT
