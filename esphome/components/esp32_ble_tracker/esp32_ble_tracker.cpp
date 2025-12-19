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

#define MBEDTLS_AES_ALT
#include <aes_alt.h>

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

#ifdef USE_OTA
  ota::get_global_ota_callback()->add_on_state_callback(
      [this](ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) {
        if (state == ota::OTA_STARTED) {
          this->stop_scan();
#ifdef ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT
          for (auto *client : this->clients_) {
            client->disconnect();
          }
#endif
        }
      });
#endif
}

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
  // when the loop is blocked
  if (this->scanner_state_ == ScannerState::RUNNING) {
    switch (this->scan_timeout_state_) {
      case ScanTimeoutState::MONITORING: {
        uint32_t now = App.get_loop_component_start_time();
        uint32_t timeout_ms = this->scan_duration_ * 2000;
        // Robust time comparison that handles rollover correctly
        // This works because unsigned arithmetic wraps around predictably
        if ((now - this->scan_start_time_) > timeout_ms) {
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
        // This case should be unreachable - scanner and timeout states are always synchronized
        break;
    }
  }

  ClientStateCounts counts = this->count_client_states_();
  if (counts != this->client_state_counts_) {
    this->client_state_counts_ = counts;
    ESP_LOGD(TAG, "connecting: %d, discovered: %d, disconnecting: %d", this->client_state_counts_.connecting,
             this->client_state_counts_.discovered, this->client_state_counts_.disconnecting);
  }

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

  if (this->scanner_state_ == ScannerState::IDLE && !counts.connecting && !counts.disconnecting && !counts.discovered) {
#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
    this->update_coex_preference_(false);
#endif
    if (this->scan_continuous_) {
      this->start_scan_(false);  // first = false
    }
  }
  // If there is a discovered client and no connecting
  // clients, then promote the discovered client to ready to connect.
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
  ESP_LOGD(TAG, "Starting scan, set scanner state to STARTING.");
  if (!first) {
#ifdef ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT
    for (auto *listener : this->listeners_)
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
  this->clients_.push_back(client);
  this->recalculate_advertisement_parser_types();
#endif
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
  for (auto *listener : this->scanner_state_listeners_) {
    listener->on_scanner_state(state);
  }
}

#ifdef USE_ESP32_BLE_DEVICE
ESPBLEiBeacon::ESPBLEiBeacon(const uint8_t *data) { memcpy(&this->beacon_data_, data, sizeof(beacon_data_)); }
optional<ESPBLEiBeacon> ESPBLEiBeacon::from_manufacturer_data(const ServiceData &data) {
  if (!data.uuid.contains(0x4C, 0x00))
    return {};

  if (data.data.size() != 23)
    return {};
  return ESPBLEiBeacon(data.data.data());
}

void ESPBTDevice::parse_scan_rst(const BLEScanResult &scan_result) {
  this->scan_result_ = &scan_result;
  for (uint8_t i = 0; i < ESP_BD_ADDR_LEN; i++)
    this->address_[i] = scan_result.bda[i];
  this->address_type_ = static_cast<esp_ble_addr_type_t>(scan_result.ble_addr_type);
  this->rssi_ = scan_result.rssi;

  // Parse advertisement data directly
  uint8_t total_len = scan_result.adv_data_len + scan_result.scan_rsp_len;
  this->parse_adv_(scan_result.ble_adv, total_len);

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "Parse Result:");
  const char *address_type;
  switch (this->address_type_) {
    case BLE_ADDR_TYPE_PUBLIC:
      address_type = "PUBLIC";
      break;
    case BLE_ADDR_TYPE_RANDOM:
      address_type = "RANDOM";
      break;
    case BLE_ADDR_TYPE_RPA_PUBLIC:
      address_type = "RPA_PUBLIC";
      break;
    case BLE_ADDR_TYPE_RPA_RANDOM:
      address_type = "RPA_RANDOM";
      break;
    default:
      address_type = "UNKNOWN";
      break;
  }
  ESP_LOGVV(TAG, "  Address: %02X:%02X:%02X:%02X:%02X:%02X (%s)", this->address_[0], this->address_[1],
            this->address_[2], this->address_[3], this->address_[4], this->address_[5], address_type);

  ESP_LOGVV(TAG, "  RSSI: %d", this->rssi_);
  ESP_LOGVV(TAG, "  Name: '%s'", this->name_.c_str());
  for (auto &it : this->tx_powers_) {
    ESP_LOGVV(TAG, "  TX Power: %d", it);
  }
  if (this->appearance_.has_value()) {
    ESP_LOGVV(TAG, "  Appearance: %u", *this->appearance_);
  }
  if (this->ad_flag_.has_value()) {
    ESP_LOGVV(TAG, "  Ad Flag: %u", *this->ad_flag_);
  }
  for (auto &uuid : this->service_uuids_) {
    ESP_LOGVV(TAG, "  Service UUID: %s", uuid.to_string().c_str());
  }
  for (auto &data : this->manufacturer_datas_) {
    auto ibeacon = ESPBLEiBeacon::from_manufacturer_data(data);
    if (ibeacon.has_value()) {
      ESP_LOGVV(TAG, "  Manufacturer iBeacon:");
      ESP_LOGVV(TAG, "    UUID: %s", ibeacon.value().get_uuid().to_string().c_str());
      ESP_LOGVV(TAG, "    Major: %u", ibeacon.value().get_major());
      ESP_LOGVV(TAG, "    Minor: %u", ibeacon.value().get_minor());
      ESP_LOGVV(TAG, "    TXPower: %d", ibeacon.value().get_signal_power());
    } else {
      ESP_LOGVV(TAG, "  Manufacturer ID: %s, data: %s", data.uuid.to_string().c_str(),
                format_hex_pretty(data.data).c_str());
    }
  }
  for (auto &data : this->service_datas_) {
    ESP_LOGVV(TAG, "  Service data:");
    ESP_LOGVV(TAG, "    UUID: %s", data.uuid.to_string().c_str());
    ESP_LOGVV(TAG, "    Data: %s", format_hex_pretty(data.data).c_str());
  }

  ESP_LOGVV(TAG, "  Adv data: %s",
            format_hex_pretty(scan_result.ble_adv, scan_result.adv_data_len + scan_result.scan_rsp_len).c_str());
#endif
}

void ESPBTDevice::parse_adv_(const uint8_t *payload, uint8_t len) {
  size_t offset = 0;

  while (offset + 2 < len) {
    const uint8_t field_length = payload[offset++];  // First byte is length of adv record
    if (field_length == 0) {
      continue;  // Possible zero padded advertisement data
    }

    // first byte of adv record is adv record type
    const uint8_t record_type = payload[offset++];
    const uint8_t *record = &payload[offset];
    const uint8_t record_length = field_length - 1;
    offset += record_length;

    // See also Generic Access Profile Assigned Numbers:
    // https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/ See also ADVERTISING AND SCAN
    // RESPONSE DATA FORMAT: https://www.bluetooth.com/specifications/bluetooth-core-specification/ (vol 3, part C, 11)
    // See also Core Specification Supplement: https://www.bluetooth.com/specifications/bluetooth-core-specification/
    // (called CSS here)

    switch (record_type) {
      case ESP_BLE_AD_TYPE_NAME_SHORT:
      case ESP_BLE_AD_TYPE_NAME_CMPL: {
        // CSS 1.2 LOCAL NAME
        // "The Local Name data type shall be the same as, or a shortened version of, the local name assigned to the
        // device." CSS 1: Optional in this context; shall not appear more than once in a block.
        // SHORTENED LOCAL NAME
        // "The Shortened Local Name data type defines a shortened version of the Local Name data type. The Shortened
        // Local Name data type shall not be used to advertise a name that is longer than the Local Name data type."
        if (record_length > this->name_.length()) {
          this->name_ = std::string(reinterpret_cast<const char *>(record), record_length);
        }
        break;
      }
      case ESP_BLE_AD_TYPE_TX_PWR: {
        // CSS 1.5 TX POWER LEVEL
        // "The TX Power Level data type indicates the transmitted power level of the packet containing the data type."
        // CSS 1: Optional in this context (may appear more than once in a block).
        this->tx_powers_.push_back(*payload);
        break;
      }
      case ESP_BLE_AD_TYPE_APPEARANCE: {
        // CSS 1.12 APPEARANCE
        // "The Appearance data type defines the external appearance of the device."
        // See also https://www.bluetooth.com/specifications/gatt/characteristics/
        // CSS 1: Optional in this context; shall not appear more than once in a block and shall not appear in both
        // the AD and SRD of the same extended advertising interval.
        this->appearance_ = *reinterpret_cast<const uint16_t *>(record);
        break;
      }
      case ESP_BLE_AD_TYPE_FLAG: {
        // CSS 1.3 FLAGS
        // "The Flags data type contains one bit Boolean flags. The Flags data type shall be included when any of the
        // Flag bits are non-zero and the advertising packet is connectable, otherwise the Flags data type may be
        // omitted."
        // CSS 1: Optional in this context; shall not appear more than once in a block.
        this->ad_flag_ = *record;
        break;
      }
      // CSS 1.1 SERVICE UUID
      // The Service UUID data type is used to include a list of Service or Service Class UUIDs.
      // There are six data types defined for the three sizes of Service UUIDs that may be returned:
      // CSS 1: Optional in this context (may appear more than once in a block).
      case ESP_BLE_AD_TYPE_16SRV_CMPL:
      case ESP_BLE_AD_TYPE_16SRV_PART: {
        // • 16-bit Bluetooth Service UUIDs
        for (uint8_t i = 0; i < record_length / 2; i++) {
          this->service_uuids_.push_back(ESPBTUUID::from_uint16(*reinterpret_cast<const uint16_t *>(record + 2 * i)));
        }
        break;
      }
      case ESP_BLE_AD_TYPE_32SRV_CMPL:
      case ESP_BLE_AD_TYPE_32SRV_PART: {
        // • 32-bit Bluetooth Service UUIDs
        for (uint8_t i = 0; i < record_length / 4; i++) {
          this->service_uuids_.push_back(ESPBTUUID::from_uint32(*reinterpret_cast<const uint32_t *>(record + 4 * i)));
        }
        break;
      }
      case ESP_BLE_AD_TYPE_128SRV_CMPL:
      case ESP_BLE_AD_TYPE_128SRV_PART: {
        // • Global 128-bit Service UUIDs
        this->service_uuids_.push_back(ESPBTUUID::from_raw(record));
        break;
      }
      case ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE: {
        // CSS 1.4 MANUFACTURER SPECIFIC DATA
        // "The Manufacturer Specific data type is used for manufacturer specific data. The first two data octets shall
        // contain a company identifier from Assigned Numbers. The interpretation of any other octets within the data
        // shall be defined by the manufacturer specified by the company identifier."
        // CSS 1: Optional in this context (may appear more than once in a block).
        if (record_length < 2) {
          ESP_LOGV(TAG, "Record length too small for ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE");
          break;
        }
        ServiceData data{};
        data.uuid = ESPBTUUID::from_uint16(*reinterpret_cast<const uint16_t *>(record));
        data.data.assign(record + 2UL, record + record_length);
        this->manufacturer_datas_.push_back(data);
        break;
      }

      // CSS 1.11 SERVICE DATA
      // "The Service Data data type consists of a service UUID with the data associated with that service."
      // CSS 1: Optional in this context (may appear more than once in a block).
      case ESP_BLE_AD_TYPE_SERVICE_DATA: {
        // «Service Data - 16 bit UUID»
        // Size: 2 or more octets
        // The first 2 octets contain the 16 bit Service UUID fol- lowed by additional service data
        if (record_length < 2) {
          ESP_LOGV(TAG, "Record length too small for ESP_BLE_AD_TYPE_SERVICE_DATA");
          break;
        }
        ServiceData data{};
        data.uuid = ESPBTUUID::from_uint16(*reinterpret_cast<const uint16_t *>(record));
        data.data.assign(record + 2UL, record + record_length);
        this->service_datas_.push_back(data);
        break;
      }
      case ESP_BLE_AD_TYPE_32SERVICE_DATA: {
        // «Service Data - 32 bit UUID»
        // Size: 4 or more octets
        // The first 4 octets contain the 32 bit Service UUID fol- lowed by additional service data
        if (record_length < 4) {
          ESP_LOGV(TAG, "Record length too small for ESP_BLE_AD_TYPE_32SERVICE_DATA");
          break;
        }
        ServiceData data{};
        data.uuid = ESPBTUUID::from_uint32(*reinterpret_cast<const uint32_t *>(record));
        data.data.assign(record + 4UL, record + record_length);
        this->service_datas_.push_back(data);
        break;
      }
      case ESP_BLE_AD_TYPE_128SERVICE_DATA: {
        // «Service Data - 128 bit UUID»
        // Size: 16 or more octets
        // The first 16 octets contain the 128 bit Service UUID followed by additional service data
        if (record_length < 16) {
          ESP_LOGV(TAG, "Record length too small for ESP_BLE_AD_TYPE_128SERVICE_DATA");
          break;
        }
        ServiceData data{};
        data.uuid = ESPBTUUID::from_raw(record);
        data.data.assign(record + 16UL, record + record_length);
        this->service_datas_.push_back(data);
        break;
      }
      case ESP_BLE_AD_TYPE_INT_RANGE:
        // Avoid logging this as it's very verbose
        break;
      default: {
        ESP_LOGV(TAG, "Unhandled type: advType: 0x%02x", record_type);
        break;
      }
    }
  }
}

std::string ESPBTDevice::address_str() const {
  char mac[18];
  format_mac_addr_upper(this->address_, mac);
  return mac;
}

uint64_t ESPBTDevice::address_uint64() const { return esp32_ble::ble_addr_to_uint64(this->address_); }
#endif  // USE_ESP32_BLE_DEVICE

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
  ESP_LOGCONFIG(TAG, "  Scanner State: %s", this->scanner_state_to_string_(this->scanner_state_));
  ESP_LOGCONFIG(TAG, "  Connecting: %d, discovered: %d, disconnecting: %d", this->client_state_counts_.connecting,
                this->client_state_counts_.discovered, this->client_state_counts_.disconnecting);
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

  ESP_LOGD(TAG, "Found device %s RSSI=%d", device.address_str().c_str(), device.get_rssi());

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

bool ESPBTDevice::resolve_irk(const uint8_t *irk) const {
  uint8_t ecb_key[16];
  uint8_t ecb_plaintext[16];
  uint8_t ecb_ciphertext[16];

  uint64_t addr64 = esp32_ble::ble_addr_to_uint64(this->address_);

  memcpy(&ecb_key, irk, 16);
  memset(&ecb_plaintext, 0, 16);

  ecb_plaintext[13] = (addr64 >> 40) & 0xff;
  ecb_plaintext[14] = (addr64 >> 32) & 0xff;
  ecb_plaintext[15] = (addr64 >> 24) & 0xff;

  mbedtls_aes_context ctx = {0, 0, {0}};
  mbedtls_aes_init(&ctx);

  if (mbedtls_aes_setkey_enc(&ctx, ecb_key, 128) != 0) {
    mbedtls_aes_free(&ctx);
    return false;
  }

  if (mbedtls_aes_crypt_ecb(&ctx, ESP_AES_ENCRYPT, ecb_plaintext, ecb_ciphertext) != 0) {
    mbedtls_aes_free(&ctx);
    return false;
  }

  mbedtls_aes_free(&ctx);

  return ecb_ciphertext[15] == (addr64 & 0xff) && ecb_ciphertext[14] == ((addr64 >> 8) & 0xff) &&
         ecb_ciphertext[13] == ((addr64 >> 16) & 0xff);
}

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
  ESP_LOGD(TAG, "Scan %scomplete, set scanner state to IDLE.", is_stop_complete ? "stop " : "");
#ifdef USE_ESP32_BLE_DEVICE
  this->already_discovered_.clear();
#endif
  // Reset timeout state machine instead of cancelling scheduler timeout
  this->scan_timeout_state_ = ScanTimeoutState::INACTIVE;

#ifdef ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT
  for (auto *listener : this->listeners_)
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
