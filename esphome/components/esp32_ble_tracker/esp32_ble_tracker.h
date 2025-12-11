#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include <array>
#include <string>
#include <vector>

#ifdef USE_ESP32

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/esp32_ble/ble_scan_result.h"

namespace esphome::esp32_ble_tracker {

using namespace esp32_ble;

using adv_data_t = std::vector<uint8_t>;

enum AdvertisementParserType {
  PARSED_ADVERTISEMENTS,
  RAW_ADVERTISEMENTS,
};

#ifdef USE_ESP32_BLE_UUID
struct ServiceData {
  ESPBTUUID uuid;
  adv_data_t data;
};
#endif

#ifdef USE_ESP32_BLE_DEVICE
class ESPBLEiBeacon {
 public:
  ESPBLEiBeacon() { memset(&this->beacon_data_, 0, sizeof(this->beacon_data_)); }
  ESPBLEiBeacon(const uint8_t *data);
  static optional<ESPBLEiBeacon> from_manufacturer_data(const ServiceData &data);

  uint16_t get_major() { return byteswap(this->beacon_data_.major); }
  uint16_t get_minor() { return byteswap(this->beacon_data_.minor); }
  int8_t get_signal_power() { return this->beacon_data_.signal_power; }
  ESPBTUUID get_uuid() { return ESPBTUUID::from_raw_reversed(this->beacon_data_.proximity_uuid); }

 protected:
  struct {
    uint8_t sub_type;
    uint8_t length;
    uint8_t proximity_uuid[16];
    uint16_t major;
    uint16_t minor;
    int8_t signal_power;
  } PACKED beacon_data_;
};

class ESPBTDevice {
 public:
  void parse_scan_rst(const BLEScanResult &scan_result);

  std::string address_str() const;

  uint64_t address_uint64() const;

  const uint8_t *address() const { return address_; }

  esp_ble_addr_type_t get_address_type() const { return this->address_type_; }
  int get_rssi() const { return rssi_; }
  const std::string &get_name() const { return this->name_; }

  const std::vector<int8_t> &get_tx_powers() const { return tx_powers_; }

  const optional<uint16_t> &get_appearance() const { return appearance_; }
  const optional<uint8_t> &get_ad_flag() const { return ad_flag_; }
  const std::vector<ESPBTUUID> &get_service_uuids() const { return service_uuids_; }

  const std::vector<ServiceData> &get_manufacturer_datas() const { return manufacturer_datas_; }

  const std::vector<ServiceData> &get_service_datas() const { return service_datas_; }

  // Exposed through a function for use in lambdas
  const BLEScanResult &get_scan_result() const { return *scan_result_; }

  bool resolve_irk(const uint8_t *irk) const;

  optional<ESPBLEiBeacon> get_ibeacon() const {
    for (auto &it : this->manufacturer_datas_) {
      auto res = ESPBLEiBeacon::from_manufacturer_data(it);
      if (res.has_value())
        return *res;
    }
    return {};
  }

 protected:
  void parse_adv_(const uint8_t *payload, uint8_t len);

  esp_bd_addr_t address_{
      0,
  };
  esp_ble_addr_type_t address_type_{BLE_ADDR_TYPE_PUBLIC};
  int rssi_{0};
  std::string name_{};
  std::vector<int8_t> tx_powers_{};
  optional<uint16_t> appearance_{};
  optional<uint8_t> ad_flag_{};
  std::vector<ESPBTUUID> service_uuids_{};
  std::vector<ServiceData> manufacturer_datas_{};
  std::vector<ServiceData> service_datas_{};
  const BLEScanResult *scan_result_{nullptr};
};
#endif  // USE_ESP32_BLE_DEVICE

class ESP32BLETracker;

class ESPBTDeviceListener {
 public:
  virtual void on_scan_end() {}
#ifdef USE_ESP32_BLE_DEVICE
  virtual bool parse_device(const ESPBTDevice &device) = 0;
#endif
  virtual bool parse_devices(const BLEScanResult *scan_results, size_t count) { return false; };
  virtual AdvertisementParserType get_advertisement_parser_type() {
    return AdvertisementParserType::PARSED_ADVERTISEMENTS;
  };
  void set_parent(ESP32BLETracker *parent) { parent_ = parent; }

 protected:
  ESP32BLETracker *parent_{nullptr};
};

struct ClientStateCounts {
  uint8_t connecting = 0;
  uint8_t discovered = 0;
  uint8_t disconnecting = 0;

  bool operator==(const ClientStateCounts &other) const {
    return connecting == other.connecting && discovered == other.discovered && disconnecting == other.disconnecting;
  }

  bool operator!=(const ClientStateCounts &other) const { return !(*this == other); }
};

enum class ClientState : uint8_t {
  // Connection is allocated
  INIT,
  // Client is disconnecting
  DISCONNECTING,
  // Connection is idle, no device detected.
  IDLE,
  // Device advertisement found.
  DISCOVERED,
  // Connection in progress.
  CONNECTING,
  // Initial connection established.
  CONNECTED,
  // The client and sub-clients have completed setup.
  ESTABLISHED,
};

enum class ScannerState {
  // Scanner is idle, init state
  IDLE,
  // Scanner is starting
  STARTING,
  // Scanner is running
  RUNNING,
  // Scanner failed to start
  FAILED,
  // Scanner is stopping
  STOPPING,
};

/** Listener interface for BLE scanner state changes.
 *
 * Components can implement this interface to receive scanner state updates
 * without the overhead of std::function callbacks.
 */
class BLEScannerStateListener {
 public:
  virtual void on_scanner_state(ScannerState state) = 0;
};

// Helper function to convert ClientState to string
const char *client_state_to_string(ClientState state);

enum class ConnectionType : uint8_t {
  // The default connection type, we hold all the services in ram
  // for the duration of the connection.
  V1,
  // The client has a cache of the services and mtu so we should not
  // fetch them again
  V3_WITH_CACHE,
  // The client does not need the services and mtu once we send them
  // so we should wipe them from memory as soon as we send them
  V3_WITHOUT_CACHE
};

class ESPBTClient : public ESPBTDeviceListener {
 public:
  virtual bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) = 0;
  virtual void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) = 0;
  virtual void connect() = 0;
  virtual void disconnect() = 0;
  bool disconnect_pending() const { return this->want_disconnect_; }
  void cancel_pending_disconnect() { this->want_disconnect_ = false; }
  virtual void set_state(ClientState st) {
    this->state_ = st;
    if (st == ClientState::IDLE) {
      this->want_disconnect_ = false;
    }
  }
  ClientState state() const { return state_; }

  // Memory optimized layout
  uint8_t app_id;  // App IDs are small integers assigned sequentially

 protected:
  // Group 1: 1-byte types
  ClientState state_{ClientState::INIT};
  // want_disconnect_ is set to true when a disconnect is requested
  // while the client is connecting. This is used to disconnect the
  // client as soon as we get the connection id (conn_id_) from the
  // ESP_GATTC_OPEN_EVT event.
  bool want_disconnect_{false};
  // 2 bytes used, 2 bytes padding
};

class ESP32BLETracker : public Component,
                        public GAPEventHandler,
                        public GAPScanEventHandler,
                        public GATTcEventHandler,
                        public BLEStatusEventHandler,
                        public Parented<ESP32BLE> {
 public:
  void set_scan_duration(uint32_t scan_duration) { scan_duration_ = scan_duration; }
  void set_scan_interval(uint32_t scan_interval) { scan_interval_ = scan_interval; }
  void set_scan_window(uint32_t scan_window) { scan_window_ = scan_window; }
  void set_scan_active(bool scan_active) { scan_active_ = scan_active; }
  bool get_scan_active() const { return scan_active_; }
  void set_scan_continuous(bool scan_continuous) { scan_continuous_ = scan_continuous; }

  /// Setup the FreeRTOS task and the Bluetooth stack.
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void loop() override;

  void register_listener(ESPBTDeviceListener *listener);
  void register_client(ESPBTClient *client);
  void recalculate_advertisement_parser_types();

#ifdef USE_ESP32_BLE_DEVICE
  void print_bt_device_info(const ESPBTDevice &device);
#endif

  void start_scan();
  void stop_scan();

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void gap_scan_event_handler(const BLEScanResult &scan_result) override;
  void ble_before_disabled_event_handler() override;

  /// Add a listener for scanner state changes
  void add_scanner_state_listener(BLEScannerStateListener *listener) {
    this->scanner_state_listeners_.push_back(listener);
  }
  ScannerState get_scanner_state() const { return this->scanner_state_; }

 protected:
  void stop_scan_();
  /// Start a single scan by setting up the parameters and doing some esp-idf calls.
  void start_scan_(bool first);
  /// Called when a `ESP_GAP_BLE_SCAN_RESULT_EVT` event is received.
  void gap_scan_result_(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param);
  /// Called when a `ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT` event is received.
  void gap_scan_set_param_complete_(const esp_ble_gap_cb_param_t::ble_scan_param_cmpl_evt_param &param);
  /// Called when a `ESP_GAP_BLE_SCAN_START_COMPLETE_EVT` event is received.
  void gap_scan_start_complete_(const esp_ble_gap_cb_param_t::ble_scan_start_cmpl_evt_param &param);
  /// Called when a `ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT` event is received.
  void gap_scan_stop_complete_(const esp_ble_gap_cb_param_t::ble_scan_stop_cmpl_evt_param &param);
  /// Called to set the scanner state. Will also call callbacks to let listeners know when state is changed.
  void set_scanner_state_(ScannerState state);
  /// Common cleanup logic when transitioning scanner to IDLE state
  void cleanup_scan_state_(bool is_stop_complete);
  /// Process a single scan result immediately
  void process_scan_result_(const BLEScanResult &scan_result);
  /// Handle scanner failure states
  void handle_scanner_failure_();
  /// Try to promote discovered clients to ready to connect
  void try_promote_discovered_clients_();
  /// Convert scanner state enum to string for logging
  const char *scanner_state_to_string_(ScannerState state) const;
  /// Log an unexpected scanner state
  void log_unexpected_state_(const char *operation, ScannerState expected_state) const;
#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
  /// Update BLE coexistence preference
  void update_coex_preference_(bool force_ble);
#endif
  /// Count clients in each state
  ClientStateCounts count_client_states_() const {
    ClientStateCounts counts;
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
    for (auto *client : this->clients_) {
      switch (client->state()) {
        case ClientState::DISCONNECTING:
          counts.disconnecting++;
          break;
        case ClientState::DISCOVERED:
          counts.discovered++;
          break;
        case ClientState::CONNECTING:
          counts.connecting++;
          break;
        default:
          break;
      }
    }
#endif
    return counts;
  }

  // Group 1: Large objects (12+ bytes) - vectors
#ifdef ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT
  StaticVector<ESPBTDeviceListener *, ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT> listeners_;
#endif
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
  StaticVector<ESPBTClient *, ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT> clients_;
#endif
  std::vector<BLEScannerStateListener *> scanner_state_listeners_;
#ifdef USE_ESP32_BLE_DEVICE
  /// Vector of addresses that have already been printed in print_bt_device_info
  std::vector<uint64_t> already_discovered_;
#endif

  // Group 2: Structs (aligned to 4 bytes)
  /// A structure holding the ESP BLE scan parameters.
  esp_ble_scan_params_t scan_params_;
  ClientStateCounts client_state_counts_;

  // Group 3: 4-byte types
  /// The interval in seconds to perform scans.
  uint32_t scan_duration_;
  uint32_t scan_interval_;
  uint32_t scan_window_;
  esp_bt_status_t scan_start_failed_{ESP_BT_STATUS_SUCCESS};
  esp_bt_status_t scan_set_param_failed_{ESP_BT_STATUS_SUCCESS};

  // Group 4: 1-byte types (enums, uint8_t, bool)
  uint8_t app_id_{0};
  uint8_t scan_start_fail_count_{0};
  ScannerState scanner_state_{ScannerState::IDLE};
  bool scan_continuous_;
  bool scan_active_;
  bool ble_was_disabled_{true};
  bool raw_advertisements_{false};
  bool parse_advertisements_{false};
#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
  bool coex_prefer_ble_{false};
#endif
  // Scan timeout state machine
  enum class ScanTimeoutState : uint8_t {
    INACTIVE,       // No timeout monitoring
    MONITORING,     // Actively monitoring for timeout
    EXCEEDED_WAIT,  // Timeout exceeded, waiting one loop before reboot
  };
  uint32_t scan_start_time_{0};
  ScanTimeoutState scan_timeout_state_{ScanTimeoutState::INACTIVE};
};

// NOLINTNEXTLINE
extern ESP32BLETracker *global_esp32_ble_tracker;

}  // namespace esphome::esp32_ble_tracker

#endif
