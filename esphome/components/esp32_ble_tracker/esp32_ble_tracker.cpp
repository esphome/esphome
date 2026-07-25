#ifdef USE_ESP32

#include "esp32_ble_tracker.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_defs.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <cinttypes>

#ifdef USE_OTA
#include "esphome/components/ota/ota_backend.h"
#endif

#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
#include <esp_coexist.h>
#endif

// bt_trace.h
#undef TAG

namespace esphome::esp32_ble_tracker {

static const char *const TAG = "esp32_ble_tracker";

ESP32BLETracker *global_esp32_ble_tracker = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const char *client_state_to_string(ClientState state) {
  switch (state) {
    case ClientState::INIT:
      return "INIT";
    case ClientState::DISCONNECTING:
      return "DISCONNECTING";
    case ClientState::IDLE:
      return "IDLE";
    case ClientState::DISCOVERED:
      return "DISCOVERED";
    case ClientState::CONNECTING:
      return "CONNECTING";
    case ClientState::CONNECTED:
      return "CONNECTED";
    case ClientState::ESTABLISHED:
      return "ESTABLISHED";
    default:
      return "UNKNOWN";
  }
}

float ESP32BLETracker::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void ESP32BLETracker::setup() {
  if (this->parent_->is_failed()) {
    this->mark_failed();
    ESP_LOGE(TAG, "BLE Tracker was marked failed by ESP32BLE");
    return;
  }

  global_esp32_ble_tracker = this;

#ifdef USE_OTA_STATE_LISTENER
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
}

#ifdef USE_OTA_STATE_LISTENER
void ESP32BLETracker::on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) {
  if (state == ota::OTA_STARTED) {
    this->scan_continuous_before_ota_ = this->scan_continuous_;
    this->stop_scan();
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
    for (auto *client : this->clients_) {
      client->disconnect();
    }
#endif
  } else if ((state == ota::OTA_ERROR || state == ota::OTA_ABORT) && this->scan_continuous_before_ota_) {
    this->scan_continuous_before_ota_ = false;
    this->scan_continuous_ = true;
    // Do not restart scanning immediately here; allow loop() to
    // safely restart scanning once the scanner and all clients are idle.
  }
}
#endif

void ESP32BLETracker::loop() {
  if (!this->parent_->is_active()) {
    this->ble_was_disabled_ = true;
    return;
  } else if (this->ble_was_disabled_) {
    this->ble_was_disabled_ = false;
    // If the BLE stack was disabled, we need to start the scan again.
    if (this->scan_continuous_) {
      this->start_scan();
    }
  }

  // Check for scan timeout - moved here from scheduler to avoid false reboots
  // when the loop is blocked. This must run every iteration for safety.
  if (this->scanner_state_ == ScannerState::RUNNING) {
    switch (this->scan_timeout_state_) {
      case ScanTimeoutState::MONITORING: {
        // Robust time comparison that handles rollover correctly
        // This works because unsigned arithmetic wraps around predictably
        if ((App.get_loop_component_start_time() - this->scan_start_time_) > this->scan_timeout_ms_) {
          // First time we've seen the timeout exceeded - wait one more loop iteration
          // This ensures all components have had a chance to process pending events
          // This is because esp32_ble may not have run yet and called
          // gap_scan_event_handler yet when the loop unblocks
          ESP_LOGW(TAG, "Scan timeout exceeded");
          this->scan_timeout_state_ = ScanTimeoutState::EXCEEDED_WAIT;
        }
        break;
      }
      case ScanTimeoutState::EXCEEDED_WAIT:
        // We've waited at least one full loop iteration, and scan is still running
        ESP_LOGE(TAG, "Scan never terminated, rebooting");
        App.reboot();
        break;
      case ScanTimeoutState::INACTIVE:
        break;
    }
  }

  // Fast path: skip expensive client state counting and processing
  // if no state has changed since last loop iteration.
  //
  // How state changes ensure we reach the code below:
  // - handle_scanner_failure_(): scanner_state_ becomes FAILED via set_scanner_state_(), or
  //   scan_set_param_failed_ requires scanner_state_==RUNNING which can only be reached via
  //   set_scanner_state_(RUNNING) in gap_scan_start_complete_() (scan params are set during
  //   STARTING, not RUNNING, so version is always incremented before this condition is true)
  // - start_scan_(): scanner_state_ becomes IDLE via set_scanner_state_() in cleanup_scan_state_()
  // - try_promote_discovered_clients_(): client enters DISCOVERED via set_state(), or
  //   connecting client finishes (state change), or scanner reaches RUNNING/IDLE
  //
  // All conditions that affect the logic below are tied to state changes that increment
  // state_version_, so the fast path is safe.
  if (this->state_version_ == this->last_processed_version_) {
    return;
  }
  this->last_processed_version_ = this->state_version_;

  // State changed - do full processing
  ClientStateCounts counts = this->count_client_states_();
  if (counts != this->client_state_counts_) {
    this->client_state_counts_ = counts;
    ESP_LOGD(TAG, "connecting: %d, discovered: %d, disconnecting: %d, active: %d",
             this->client_state_counts_.connecting, this->client_state_counts_.discovered,
             this->client_state_counts_.disconnecting, this->client_state_counts_.active);
  }

  // Scanner failure: reached when set_scanner_state_(FAILED) or scan_set_param_failed_ set
  if (this->scanner_state_ == ScannerState::FAILED ||
      (this->scan_set_param_failed_ && this->scanner_state_ == ScannerState::RUNNING)) {
    this->handle_scanner_failure_();
  }
  /*

    Avoid starting the scanner if:
    - we are already scanning
    - we are connecting to a device
    - we are disconnecting from a device

    Otherwise the scanner could fail to ever start again
    and our only way to recover is to reboot.

    https://github.com/espressif/esp-idf/issues/6688

  */

  // Start scan: reached when scanner_state_ becomes IDLE (via set_scanner_state_()) and
  // no clients are in the transient CONNECTING / DISCOVERED / DISCONNECTING states
  // (their state changes increment version when they finish). CONNECTED / ESTABLISHED
  // clients do NOT block this branch — the coex revert below has its own active-count gate.
  if (this->scanner_state_ == ScannerState::IDLE && !counts.connecting && !counts.disconnecting && !counts.discovered) {
#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
    // Only revert to BALANCE when no connections are active. Established connections
    // continue to need PREFER_BT so peer GATT responses can reach us while WiFi traffic
    // (advertisement upload, log streaming) competes for the shared radio. Reverting too
    // early causes Bluedroid to time out at ~20s and synthesize status=133.
    if (!counts.active) {
      this->update_coex_preference_(false);
    }
#endif
    if (this->scan_continuous_) {
      this->start_scan_(false);  // first = false
    }
  }
  // Promote discovered clients: reached when a client's state becomes DISCOVERED (via set_state()),
  // or when a blocking condition clears (connecting client finishes, scanner reaches RUNNING/IDLE).
  // All these trigger state_version_ increment, so we'll process and check promotion eligibility.
  // We check both RUNNING and IDLE states because:
  // - RUNNING: gap_scan_event_handler initiates stop_scan_() but promotion can happen immediately
  // - IDLE: Scanner has already stopped (naturally or by gap_scan_event_handler)
  if (counts.discovered && !counts.connecting &&
      (this->scanner_state_ == ScannerState::RUNNING || this->scanner_state_ == ScannerState::IDLE)) {
    this->try_promote_discovered_clients_();
  }
}

void ESP32BLETracker::start_scan() { this->start_scan_(true); }

void ESP32BLETracker::stop_scan() {
  ESP_LOGD(TAG, "Stopping scan.");
  this->scan_continuous_ = false;
  this->stop_scan_();
}

void ESP32BLETracker::ble_before_disabled_event_handler() { this->stop_scan_(); }

void ESP32BLETracker::stop_scan_() {
  if (this->scanner_state_ != ScannerState::RUNNING && this->scanner_state_ != ScannerState::FAILED) {
    // If scanner is already idle, there's nothing to stop - this is not an error
    if (this->scanner_state_ != ScannerState::IDLE) {
      ESP_LOGE(TAG, "Cannot stop scan: %s", this->scanner_state_to_string_(this->scanner_state_));
    }
    return;
  }
  // Reset timeout state machine when stopping scan
  this->scan_timeout_state_ = ScanTimeoutState::INACTIVE;
  this->set_scanner_state_(ScannerState::STOPPING);
  esp_err_t err = esp_ble_gap_stop_scanning();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_stop_scanning failed: %d", err);
    return;
  }
}

void ESP32BLETracker::start_scan_(bool first) {
  if (!this->parent_->is_active()) {
    ESP_LOGW(TAG, "Cannot start scan while ESP32BLE is disabled.");
    return;
  }
  if (this->scanner_state_ != ScannerState::IDLE) {
    this->log_unexpected_state_("start scan", ScannerState::IDLE);
    return;
  }
  this->set_scanner_state_(ScannerState::STARTING);
  ESP_LOGV(TAG, "Starting scan, set scanner state to STARTING.");
  if (!first) {
#ifdef ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT
    for (auto *listener : this->listeners_)
      listener->on_scan_end();
#endif
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
    for (auto *listener : this->neutral_listeners_)
      listener->on_scan_end();
#endif
  }
#ifdef USE_ESP32_BLE_DEVICE
  this->already_discovered_.clear();
#endif
  this->scan_params_.scan_type = this->scan_active_ ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE;
  this->scan_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  this->scan_params_.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  this->scan_params_.scan_interval = this->scan_interval_;
  this->scan_params_.scan_window = this->scan_window_;

  // Start timeout monitoring in loop() instead of using scheduler
  // This prevents false reboots when the loop is blocked
  this->scan_start_time_ = App.get_loop_component_start_time();
  this->scan_timeout_ms_ = this->scan_duration_ * 2000;
  this->scan_timeout_state_ = ScanTimeoutState::MONITORING;

  esp_err_t err = esp_ble_gap_set_scan_params(&this->scan_params_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_set_scan_params failed: %d", err);
    return;
  }
  err = esp_ble_gap_start_scanning(this->scan_duration_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_start_scanning failed: %d", err);
    return;
  }
}

void ESP32BLETracker::register_client(ESPBTClient *client) {
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
  client->app_id = ++this->app_id_;
  // Give client a pointer to our state_version_ so it can notify us of state changes.
  // This enables loop() fast-path optimization - we skip expensive work when no state changed.
  // Safe because ESP32BLETracker (singleton) outlives all registered clients.
  client->set_tracker_state_version(&this->state_version_);
  this->clients_.push_back(client);
  this->recalculate_advertisement_parser_types();
#endif
}

void ESP32BLETracker::register_listener(ble_device_base::ESPBTDeviceListener *listener) {
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  // Neutral BLEHub path (migrated sensors): parsed-advertisement consumers only.
  this->neutral_listeners_.push_back(listener);
  this->parse_advertisements_ = true;
#endif
}

void ESP32BLETracker::get_adapter_mac(uint8_t out[6]) {
  get_mac_address_raw(out);  // WiFi base MAC, MSB-first
  // BT MAC = base MAC + 2 on the last octet only, wrapping without carry —
  // exactly ESP-IDF's esp_read_mac(ESP_MAC_BT): mac[5] += MAC_ADDR_UNIVERSE_BT_OFFSET.
  out[5] += 2;
}

void ESP32BLETracker::register_listener(ESPBTDeviceListener *listener) {
#ifdef ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT
  listener->set_parent(this);
  this->listeners_.push_back(listener);
  this->recalculate_advertisement_parser_types();
#endif
}

void ESP32BLETracker::recalculate_advertisement_parser_types() {
  this->raw_advertisements_ = false;
  this->parse_advertisements_ = false;
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  // Neutral (BLEHub) listeners are parsed-advertisement consumers and are not in
  // listeners_; without this, any later esp32-path registration (e.g. the proxy's
  // GATT clients) would recompute the flags and silently drop parsed dispatch.
  if (!this->neutral_listeners_.empty())
    this->parse_advertisements_ = true;
#endif
#ifdef ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT
  for (auto *listener : this->listeners_) {
    if (listener->get_advertisement_parser_type() == AdvertisementParserType::PARSED_ADVERTISEMENTS) {
      this->parse_advertisements_ = true;
    } else {
      this->raw_advertisements_ = true;
    }
  }
#endif
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
  for (auto *client : this->clients_) {
    if (client->get_advertisement_parser_type() == AdvertisementParserType::PARSED_ADVERTISEMENTS) {
      this->parse_advertisements_ = true;
    } else {
      this->raw_advertisements_ = true;
    }
  }
#endif
}

void ESP32BLETracker::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  // Note: This handler is called from the main loop context, not directly from the BT task.
  // The esp32_ble component queues events via enqueue_ble_event() and processes them in loop().
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      this->gap_scan_set_param_complete_(param->scan_param_cmpl);
      break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      this->gap_scan_start_complete_(param->scan_start_cmpl);
      break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      this->gap_scan_stop_complete_(param->scan_stop_cmpl);
      break;
    default:
      break;
  }
    // Forward all events to clients (scan results are handled separately via gap_scan_event_handler)
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
  for (auto *client : this->clients_) {
    client->gap_event_handler(event, param);
  }
#endif
}

void ESP32BLETracker::gap_scan_event_handler(const BLEScanResult &scan_result) {
  // Note: This handler is called from the main loop context via esp32_ble's event queue.
  // We process advertisements immediately instead of buffering them.
  ESP_LOGVV(TAG, "gap_scan_result - event %d", scan_result.search_evt);

  if (scan_result.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
    // Process the scan result immediately
    this->process_scan_result_(scan_result);
  } else if (scan_result.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
    // Scan finished on its own
    if (this->scanner_state_ != ScannerState::RUNNING) {
      this->log_unexpected_state_("scan complete", ScannerState::RUNNING);
    }
    // Scan completed naturally, perform cleanup and transition to IDLE
    this->cleanup_scan_state_(false);
  }
}

void ESP32BLETracker::gap_scan_set_param_complete_(const esp_ble_gap_cb_param_t::ble_scan_param_cmpl_evt_param &param) {
  // Called from main loop context via gap_event_handler after being queued from BT task
  ESP_LOGV(TAG, "gap_scan_set_param_complete - status %d", param.status);
  if (param.status == ESP_BT_STATUS_DONE) {
    this->scan_set_param_failed_ = ESP_BT_STATUS_SUCCESS;
  } else {
    this->scan_set_param_failed_ = param.status;
  }
}

void ESP32BLETracker::gap_scan_start_complete_(const esp_ble_gap_cb_param_t::ble_scan_start_cmpl_evt_param &param) {
  // Called from main loop context via gap_event_handler after being queued from BT task
  ESP_LOGV(TAG, "gap_scan_start_complete - status %d", param.status);
  this->scan_start_failed_ = param.status;
  if (this->scanner_state_ != ScannerState::STARTING) {
    this->log_unexpected_state_("start complete", ScannerState::STARTING);
  }
  if (param.status == ESP_BT_STATUS_SUCCESS) {
    this->scan_start_fail_count_ = 0;
    this->set_scanner_state_(ScannerState::RUNNING);
  } else {
    this->set_scanner_state_(ScannerState::FAILED);
    if (this->scan_start_fail_count_ != std::numeric_limits<uint8_t>::max()) {
      this->scan_start_fail_count_++;
    }
  }
}

void ESP32BLETracker::gap_scan_stop_complete_(const esp_ble_gap_cb_param_t::ble_scan_stop_cmpl_evt_param &param) {
  // Called from main loop context via gap_event_handler after being queued from BT task
  // This allows us to safely transition to IDLE state and perform cleanup without race conditions
  ESP_LOGV(TAG, "gap_scan_stop_complete - status %d", param.status);
  if (this->scanner_state_ != ScannerState::STOPPING) {
    this->log_unexpected_state_("stop complete", ScannerState::STOPPING);
  }

  // Perform cleanup and transition to IDLE
  this->cleanup_scan_state_(true);
}

void ESP32BLETracker::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
  for (auto *client : this->clients_) {
    client->gattc_event_handler(event, gattc_if, param);
  }
#endif
}

void ESP32BLETracker::set_scanner_state_(ScannerState state) {
  this->scanner_state_ = state;
  this->state_version_++;
  for (auto *listener : this->scanner_state_listeners_) {
    listener->on_scanner_state(state);
  }
}

void ESP32BLETracker::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Tracker:");
  ESP_LOGCONFIG(TAG,
                "  Scan Duration: %" PRIu32 " s\n"
                "  Scan Interval: %.1f ms\n"
                "  Scan Window: %.1f ms\n"
                "  Scan Type: %s\n"
                "  Continuous Scanning: %s",
                this->scan_duration_, this->scan_interval_ * 0.625f, this->scan_window_ * 0.625f,
                this->scan_active_ ? "ACTIVE" : "PASSIVE", YESNO(this->scan_continuous_));
  ESP_LOGCONFIG(TAG,
                "  Scanner State: %s\n"
                "  Connecting: %d, discovered: %d, disconnecting: %d, active: %d",
                this->scanner_state_to_string_(this->scanner_state_), this->client_state_counts_.connecting,
                this->client_state_counts_.discovered, this->client_state_counts_.disconnecting,
                this->client_state_counts_.active);
  if (this->scan_start_fail_count_) {
    ESP_LOGCONFIG(TAG, "  Scan Start Fail Count: %d", this->scan_start_fail_count_);
  }
}

#ifdef USE_ESP32_BLE_DEVICE
void ESP32BLETracker::print_bt_device_info(const ESPBTDevice &device) {
  const uint64_t address = device.address_uint64();
  for (auto &disc : this->already_discovered_) {
    if (disc == address)
      return;
  }
  this->already_discovered_.push_back(address);

  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGD(TAG, "Found device %s RSSI=%d", device.address_str_to(addr_buf), device.get_rssi());

  const char *address_type_s;
  switch (device.get_address_type()) {
    case BLE_ADDR_TYPE_PUBLIC:
      address_type_s = "PUBLIC";
      break;
    case BLE_ADDR_TYPE_RANDOM:
      address_type_s = "RANDOM";
      break;
    case BLE_ADDR_TYPE_RPA_PUBLIC:
      address_type_s = "RPA_PUBLIC";
      break;
    case BLE_ADDR_TYPE_RPA_RANDOM:
      address_type_s = "RPA_RANDOM";
      break;
    default:
      address_type_s = "UNKNOWN";
      break;
  }

  ESP_LOGD(TAG, "  Address Type: %s", address_type_s);
  if (!device.get_name().empty()) {
    ESP_LOGD(TAG, "  Name: '%s'", device.get_name().c_str());
  }
  for (auto &tx_power : device.get_tx_powers()) {
    ESP_LOGD(TAG, "  TX Power: %d", tx_power);
  }
}

// resolve_irk() is provided by ble_device_base (portable software AES).

#endif  // USE_ESP32_BLE_DEVICE

void ESP32BLETracker::process_scan_result_(const BLEScanResult &scan_result) {
  // Process raw advertisements
  if (this->raw_advertisements_) {
#ifdef ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT
    for (auto *listener : this->listeners_) {
      listener->parse_devices(&scan_result, 1);
    }
#endif
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
    for (auto *client : this->clients_) {
      client->parse_devices(&scan_result, 1);
    }
#endif
  }

  // Process parsed advertisements
  if (this->parse_advertisements_) {
#ifdef USE_ESP32_BLE_DEVICE
    ESPBTDevice device;
    device.parse_scan_rst(scan_result);

    bool found = false;
#ifdef ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT
    for (auto *listener : this->listeners_) {
      if (listener->parse_device(device))
        found = true;
    }
#endif
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
    for (auto *listener : this->neutral_listeners_) {
      if (listener->parse_device(device))
        found = true;
    }
#endif

#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
    for (auto *client : this->clients_) {
      if (client->parse_device(device)) {
        found = true;
      }
    }
#endif

    if (!found && !this->scan_continuous_) {
      this->print_bt_device_info(device);
    }
#endif  // USE_ESP32_BLE_DEVICE
  }
}

void ESP32BLETracker::cleanup_scan_state_(bool is_stop_complete) {
  ESP_LOGV(TAG, "Scan %scomplete, set scanner state to IDLE.", is_stop_complete ? "stop " : "");
#ifdef USE_ESP32_BLE_DEVICE
  this->already_discovered_.clear();
#endif
  // Reset timeout state machine instead of cancelling scheduler timeout
  this->scan_timeout_state_ = ScanTimeoutState::INACTIVE;

#ifdef ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT
  for (auto *listener : this->listeners_)
    listener->on_scan_end();
#endif
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  for (auto *listener : this->neutral_listeners_)
    listener->on_scan_end();
#endif

  this->set_scanner_state_(ScannerState::IDLE);
}

void ESP32BLETracker::handle_scanner_failure_() {
  this->stop_scan_();
  if (this->scan_start_fail_count_ == std::numeric_limits<uint8_t>::max()) {
    ESP_LOGE(TAG, "Scan could not restart after %d attempts, rebooting to restore stack (IDF)",
             std::numeric_limits<uint8_t>::max());
    App.reboot();
  }
  if (this->scan_start_failed_) {
    ESP_LOGE(TAG, "Scan start failed: %d", this->scan_start_failed_);
    this->scan_start_failed_ = ESP_BT_STATUS_SUCCESS;
  }
  if (this->scan_set_param_failed_) {
    ESP_LOGE(TAG, "Scan set param failed: %d", this->scan_set_param_failed_);
    this->scan_set_param_failed_ = ESP_BT_STATUS_SUCCESS;
  }
}

void ESP32BLETracker::try_promote_discovered_clients_() {
  // Only promote the first discovered client to avoid multiple simultaneous connections
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
  for (auto *client : this->clients_) {
    if (client->state() != ClientState::DISCOVERED) {
      continue;
    }

    if (this->scanner_state_ == ScannerState::RUNNING) {
      ESP_LOGD(TAG, "Stopping scan to make connection");
      this->stop_scan_();
      // Don't wait for scan stop complete - promote immediately.
      // This is safe because ESP-IDF processes BLE commands sequentially through its internal mailbox queue.
      // This guarantees that the stop scan command will be fully processed before any subsequent connect command,
      // preventing race conditions or overlapping operations.
    }

    ESP_LOGD(TAG, "Promoting client to connect");
#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
    this->update_coex_preference_(true);
#endif
    client->connect();
    break;
  }
#endif
}

const char *ESP32BLETracker::scanner_state_to_string_(ScannerState state) const {
  switch (state) {
    case ScannerState::IDLE:
      return "IDLE";
    case ScannerState::STARTING:
      return "STARTING";
    case ScannerState::RUNNING:
      return "RUNNING";
    case ScannerState::STOPPING:
      return "STOPPING";
    case ScannerState::FAILED:
      return "FAILED";
    default:
      return "UNKNOWN";
  }
}

void ESP32BLETracker::log_unexpected_state_(const char *operation, ScannerState expected_state) const {
  ESP_LOGE(TAG, "Unexpected state: %s on %s, expected: %s", this->scanner_state_to_string_(this->scanner_state_),
           operation, this->scanner_state_to_string_(expected_state));
}

#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
void ESP32BLETracker::update_coex_preference_(bool force_ble) {
#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
  if (force_ble && !this->coex_prefer_ble_) {
    ESP_LOGD(TAG, "Setting coexistence to Bluetooth to make connection.");
    this->coex_prefer_ble_ = true;
    esp_coex_preference_set(ESP_COEX_PREFER_BT);  // Prioritize Bluetooth
  } else if (!force_ble && this->coex_prefer_ble_) {
    ESP_LOGD(TAG, "Setting coexistence preference to balanced.");
    this->coex_prefer_ble_ = false;
    esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);  // Reset to default
  }
#endif  // CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
}
#endif

}  // namespace esphome::esp32_ble_tracker

#endif  // USE_ESP32
