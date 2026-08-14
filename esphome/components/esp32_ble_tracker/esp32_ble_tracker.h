#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include <array>
#include <span>
#include <string>

#ifdef USE_ESP32

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "esphome/components/ble_device_base/ble_client_state.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ble_device_base/ble_hub.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/esp32_ble/ble_scan_result.h"

#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome::esp32_ble_tracker {

using namespace esp32_ble;

using adv_data_t = ble_device_base::adv_data_t;

#ifdef USE_ESP32_BLE_UUID
using ServiceData = ble_device_base::ServiceData;
#endif

#ifdef USE_ESP32_BLE_DEVICE
// The advertisement device types are owned by the platform-neutral
// ble_device_base layer; re-exported here (esp32 only) for backward
// compatibility. ESPBTDevice::parse_scan_rst() (esp32-only) adapts BLEScanResult.
using ESPBLEiBeacon = ble_device_base::ESPBLEiBeacon;
using ESPBTDevice = ble_device_base::ESPBTDevice;
#endif  // USE_ESP32_BLE_DEVICE

class ESP32BLETracker;

// esp32-flavored listener: the neutral parse_device/on_scan_end come from
// ble_device_base; this subclass adds the esp32-only raw-advertisement path
// (BLEScanResult batches) and the tracker back-pointer.
class ESPBTDeviceListener : public ble_device_base::ESPBTDeviceListener {
 public:
#ifndef USE_ESP32_BLE_DEVICE
  // Raw-only build: no parsed-device support is compiled in.
  bool parse_device(const ble_device_base::ESPBTDevice &device) override { return false; }
#endif
  void set_parent(ESP32BLETracker *parent) { parent_ = parent; }

 protected:
  ESP32BLETracker *parent_{nullptr};
};

struct ClientStateCounts {
  uint8_t connecting = 0;
  uint8_t discovered = 0;
  uint8_t disconnecting = 0;
  // CONNECTED + ESTABLISHED clients. Tracked so coex stays at PREFER_BT
  // while active connections may still need to send/receive GATT traffic.
  uint8_t active = 0;

  bool operator==(const ClientStateCounts &other) const {
    return connecting == other.connecting && discovered == other.discovered && disconnecting == other.disconnecting &&
           active == other.active;
  }

  bool operator!=(const ClientStateCounts &other) const { return !(*this == other); }
};

// The client connection state types are owned by the platform-neutral
// ble_device_base layer; re-exported here for backward compatibility.
using ClientState = ble_device_base::ClientState;
using ConnectionType = ble_device_base::ConnectionType;
using ble_device_base::client_state_to_string;

// Neutral scanner lifecycle re-exported for backward compatibility.
using ScannerState = ble_device_base::ScannerState;

/// Base class for BLE GATT clients that connect to remote devices.
///
/// State Change Tracking Design:
/// -----------------------------
/// ESP32BLETracker::loop() needs to know when client states change to avoid
/// expensive polling. Rather than checking all clients every iteration (~7000/min),
/// we use a version counter owned by ESP32BLETracker that clients increment on
/// state changes. The tracker compares versions to skip work when nothing changed.
///
/// Ownership: ESP32BLETracker owns state_version_. Clients hold a non-owning
/// pointer (tracker_state_version_) set during register_client(). Clients
/// increment the counter through this pointer when their state changes.
/// The pointer may be null if the client is not registered with a tracker.
class ESPBTClient : public ESPBTDeviceListener {
 public:
  /// False keeps the tracker from building parsed ESPBTDevice objects on
  /// this client's account (raw consumers use the hub callback).
  virtual bool wants_parsed_advertisements() { return true; }

  virtual bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) = 0;
  virtual void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) = 0;
  virtual void connect() = 0;
  virtual void disconnect() = 0;
  bool disconnect_pending() const { return this->want_disconnect_; }
  void cancel_pending_disconnect() { this->want_disconnect_ = false; }

  /// Set the client state with IDLE handling (clears want_disconnect_).
  /// Notifies the tracker of state change for loop optimization.
  virtual void set_state(ClientState st) {
    this->set_state_internal_(st);
    if (st == ClientState::IDLE) {
      this->want_disconnect_ = false;
    }
  }
  ClientState state() const { return this->state_; }

  /// Called by ESP32BLETracker::register_client() to enable state change notifications.
  /// The pointer must remain valid for the lifetime of the client (guaranteed since
  /// ESP32BLETracker is a singleton that outlives all clients).
  void set_tracker_state_version(uint8_t *version) { this->tracker_state_version_ = version; }

  // Memory optimized layout
  uint8_t app_id;  // App IDs are small integers assigned sequentially

 protected:
  /// Set state without IDLE handling - use for direct state transitions.
  /// Increments the tracker's state version counter to signal that loop()
  /// should do full processing on the next iteration.
  void set_state_internal_(ClientState st) {
    this->state_ = st;
    // Notify tracker that state changed (tracker_state_version_ is owned by ESP32BLETracker)
    if (this->tracker_state_version_ != nullptr) {
      (*this->tracker_state_version_)++;
    }
  }

  // want_disconnect_ is set to true when a disconnect is requested
  // while the client is connecting. This is used to disconnect the
  // client as soon as we get the connection id (conn_id_) from the
  // ESP_GATTC_OPEN_EVT event.
  bool want_disconnect_{false};

 private:
  ClientState state_{ClientState::INIT};
  /// Non-owning pointer to ESP32BLETracker::state_version_. When this client's
  /// state changes, we increment the tracker's counter to signal that loop()
  /// should perform full processing. Null if client not registered with tracker.
  uint8_t *tracker_state_version_{nullptr};
};

class ESP32BLETracker final : public Component,
#ifdef USE_OTA_STATE_LISTENER
                              public ota::OTAGlobalStateListener,
#endif
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

  // esp32-flavored path (unmigrated esp32 sensors; sets the tracker back-pointer).
  void register_listener(ESPBTDeviceListener *listener);
  void register_client(ESPBTClient *client);

  // ---- ble_device_base::BLEHub (the platform-neutral tracker contract) ----
  void register_listener(ble_device_base::ESPBTDeviceListener *listener);
  void set_raw_advertisement_callback(ble_device_base::RawAdvertisementCallback callback) {
    this->raw_advertisement_callback_ = callback;
  }
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  void set_scanner_state_callback(ble_device_base::ScannerStateCallback callback) {
    this->scanner_state_callback_ = callback;
  }
#endif
  static constexpr ble_device_base::HubCapabilities get_capabilities() {
    // scan_mode_switch is false: the mode is driven through this tracker's own
    // API (set_scan_active + restart), not the neutral request_scan_mode().
    return {/* active_scan = */ true, /* merges_scan_response = */ true, /* gatt = */ true,
            /* scan_mode_switch = */ false};
  }
  void get_adapter_mac(uint8_t out[MAC_ADDRESS_SIZE]) { this->parent_->get_mac_msb_first(out); }
  bool scan_running() { return this->scanner_state_ == ScannerState::RUNNING; }
  bool scan_active() { return this->scan_active_; }
  // The mode is driven through this tracker's own API (see get_capabilities);
  // the neutral request refuses without changing any state.
  bool request_scan_mode(bool active) { return false; }

#ifdef USE_ESP32_BLE_DEVICE
  void print_bt_device_info(const ESPBTDevice &device);
#endif

  void start_scan();
  void stop_scan();

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  void gap_scan_event_handler(const BLEScanResult &scan_result);
  void ble_before_disabled_event_handler();

#ifdef USE_OTA_STATE_LISTENER
  void on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) override;
#endif

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
        case ClientState::CONNECTED:
        case ClientState::ESTABLISHED:
          counts.active++;
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
  // Parsed listeners registered through the neutral BLEHub contract (migrated
  // sensors); dispatched alongside listeners_.
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  StaticVector<ble_device_base::ESPBTDeviceListener *, ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT> neutral_listeners_;
#endif
  ble_device_base::RawAdvertisementCallback raw_advertisement_callback_{};
#ifdef USE_BLE_SCANNER_STATE_CALLBACK
  ble_device_base::ScannerStateCallback scanner_state_callback_{};
#endif
#ifdef USE_ESP32_BLE_DEVICE
  /// Per-period "Found device" DEBUG log with MAC dedup (shared ble_device_base impl)
  ble_device_base::DiscoveredDeviceLog discovered_log_;
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
  /// Version counter for loop() fast-path optimization. Incremented when:
  /// - Scanner state changes (via set_scanner_state_())
  /// - Any registered client's state changes (clients hold pointer to this counter)
  /// Owned by this class; clients receive non-owning pointer via register_client().
  /// When loop() sees state_version_ == last_processed_version_, it skips expensive
  /// client state counting and takes the fast path (just timeout check + return).
  uint8_t state_version_{0};
  /// Last state_version_ value when loop() did full processing. Compared against
  /// state_version_ to detect if any state changed since last iteration.
  uint8_t last_processed_version_{0};
  ScannerState scanner_state_{ScannerState::IDLE};
  bool scan_continuous_;
  bool scan_active_;
#ifdef USE_OTA_STATE_LISTENER
  bool scan_continuous_before_ota_{false};
#endif
  bool ble_was_disabled_{true};
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
  /// Precomputed timeout value: scan_duration_ * 2000
  uint32_t scan_timeout_ms_{0};
  ScanTimeoutState scan_timeout_state_{ScanTimeoutState::INACTIVE};
};

// NOLINTNEXTLINE
extern ESP32BLETracker *global_esp32_ble_tracker;

}  // namespace esphome::esp32_ble_tracker

#endif
