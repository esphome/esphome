#ifdef USE_ESP32

#include "esp32_ble_tracker.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_bt.h>
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

#ifdef USE_ARDUINO
#include <esp32-hal-bt.h>
#endif

#define MBEDTLS_AES_ALT
#include <aes_alt.h>

// bt_trace.h
#undef TAG

namespace esphome {
namespace esp32_ble_tracker {

static const char *const TAG = "esp32_ble_tracker";

ESP32BLETracker *global_esp32_ble_tracker = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

float ESP32BLETracker::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void ESP32BLETracker::setup() {
  if (this->parent_->is_failed()) {
    this->mark_failed();
    ESP_LOGE(TAG, "BLE Tracker was marked failed by ESP32BLE");
    return;
  }
  ExternalRAMAllocator<esp_ble_gap_cb_param_t::ble_scan_result_evt_param> allocator(
      ExternalRAMAllocator<esp_ble_gap_cb_param_t::ble_scan_result_evt_param>::ALLOW_FAILURE);
  this->scan_result_buffer_ = allocator.allocate(ESP32BLETracker::SCAN_RESULT_BUFFER_SIZE);

  if (this->scan_result_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for BLE Tracker!");
    this->mark_failed();
  }

  global_esp32_ble_tracker = this;
  this->scan_result_lock_ = xSemaphoreCreateMutex();

#ifdef USE_OTA
  ota::get_global_ota_callback()->add_on_state_callback(
      [this](ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) {
        if (state == ota::OTA_STARTED) {
          this->stop_scan();
          for (auto *client : this->clients_) {
            client->disconnect();
          }
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
  int connecting = 0;
  int discovered = 0;
  int searching = 0;
  int disconnecting = 0;
  for (auto *client : this->clients_) {
    switch (client->state()) {
      case ClientState::DISCONNECTING:
        disconnecting++;
        break;
      case ClientState::DISCOVERED:
        discovered++;
        break;
      case ClientState::SEARCHING:
        searching++;
        break;
      case ClientState::CONNECTING:
      case ClientState::READY_TO_CONNECT:
        connecting++;
        break;
      default:
        break;
    }
  }
  if (connecting != connecting_ || discovered != discovered_ || searching != searching_ ||
      disconnecting != disconnecting_) {
    connecting_ = connecting;
    discovered_ = discovered;
    searching_ = searching;
    disconnecting_ = disconnecting;
    ESP_LOGD(TAG, "connecting: %d, discovered: %d, searching: %d, disconnecting: %d", connecting_, discovered_,
             searching_, disconnecting_);
  }
  bool promote_to_connecting = discovered && !searching && !connecting;

  if (this->scanner_state_ == ScannerState::RUNNING &&
      this->scan_result_index_ &&  // if it looks like we have a scan result we will take the lock
      xSemaphoreTake(this->scan_result_lock_, 0)) {
    uint32_t index = this->scan_result_index_;
    if (index >= ESP32BLETracker::SCAN_RESULT_BUFFER_SIZE) {
      ESP_LOGW(TAG, "Too many BLE events to process. Some devices may not show up.");
    }

    if (this->raw_advertisements_) {
      for (auto *listener : this->listeners_) {
        listener->parse_devices(this->scan_result_buffer_, this->scan_result_index_);
      }
      for (auto *client : this->clients_) {
        client->parse_devices(this->scan_result_buffer_, this->scan_result_index_);
      }
    }

    if (this->parse_advertisements_) {
      for (size_t i = 0; i < index; i++) {
        ESPBTDevice device;
        device.parse_scan_rst(this->scan_result_buffer_[i]);

        bool found = false;
        for (auto *listener : this->listeners_) {
          if (listener->parse_device(device))
            found = true;
        }

        for (auto *client : this->clients_) {
          if (client->parse_device(device)) {
            found = true;
            if (!connecting && client->state() == ClientState::DISCOVERED) {
              promote_to_connecting = true;
            }
          }
        }

        if (!found && !this->scan_continuous_) {
          this->print_bt_device_info(device);
        }
      }
    }
    this->scan_result_index_ = 0;
    xSemaphoreGive(this->scan_result_lock_);
  }
  if (this->scanner_state_ == ScannerState::STOPPED) {
    this->end_of_scan_();  // Change state to IDLE
  }
  if (this->scanner_state_ == ScannerState::FAILED ||
      (this->scan_set_param_failed_ && this->scanner_state_ == ScannerState::RUNNING)) {
    this->stop_scan_();
    if (this->scan_start_fail_count_ == std::numeric_limits<uint8_t>::max()) {
      ESP_LOGE(TAG, "ESP-IDF BLE scan could not restart after %d attempts, rebooting to restore BLE stack...",
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
  /*

    Avoid starting the scanner if:
    - we are already scanning
    - we are connecting to a device
    - we are disconnecting from a device

    Otherwise the scanner could fail to ever start again
    and our only way to recover is to reboot.

    https://github.com/espressif/esp-idf/issues/6688

  */
  if (this->scanner_state_ == ScannerState::IDLE && !connecting && !disconnecting && !promote_to_connecting) {
#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
    if (this->coex_prefer_ble_) {
      this->coex_prefer_ble_ = false;
      ESP_LOGD(TAG, "Setting coexistence preference to balanced.");
      esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);  // Reset to default
    }
#endif
    if (this->scan_continuous_) {
      this->start_scan_(false);  // first = false
    }
  }
  // If there is a discovered client and no connecting
  // clients and no clients using the scanner to search for
  // devices, then stop scanning and promote the discovered
  // client to ready to connect.
  if (promote_to_connecting &&
      (this->scanner_state_ == ScannerState::RUNNING || this->scanner_state_ == ScannerState::IDLE)) {
    for (auto *client : this->clients_) {
      if (client->state() == ClientState::DISCOVERED) {
        if (this->scanner_state_ == ScannerState::RUNNING) {
          ESP_LOGD(TAG, "Stopping scan to make connection...");
          this->stop_scan_();
        } else if (this->scanner_state_ == ScannerState::IDLE) {
          ESP_LOGD(TAG, "Promoting client to connect...");
          // We only want to promote one client at a time.
          // once the scanner is fully stopped.
#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
          ESP_LOGD(TAG, "Setting coexistence to Bluetooth to make connection.");
          if (!this->coex_prefer_ble_) {
            this->coex_prefer_ble_ = true;
            esp_coex_preference_set(ESP_COEX_PREFER_BT);  // Prioritize Bluetooth
          }
#endif
          client->set_state(ClientState::READY_TO_CONNECT);
        }
        break;
      }
    }
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
    if (this->scanner_state_ == ScannerState::IDLE) {
      ESP_LOGE(TAG, "Scan is already stopped while trying to stop.");
    } else if (this->scanner_state_ == ScannerState::STARTING) {
      ESP_LOGE(TAG, "Scan is starting while trying to stop.");
    } else if (this->scanner_state_ == ScannerState::STOPPING) {
      ESP_LOGE(TAG, "Scan is already stopping while trying to stop.");
    } else if (this->scanner_state_ == ScannerState::STOPPED) {
      ESP_LOGE(TAG, "Scan is already stopped while trying to stop.");
    }
    return;
  }
  this->cancel_timeout("scan");
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
    if (this->scanner_state_ == ScannerState::STARTING) {
      ESP_LOGE(TAG, "Cannot start scan while already starting.");
    } else if (this->scanner_state_ == ScannerState::RUNNING) {
      ESP_LOGE(TAG, "Cannot start scan while already running.");
    } else if (this->scanner_state_ == ScannerState::STOPPING) {
      ESP_LOGE(TAG, "Cannot start scan while already stopping.");
    } else if (this->scanner_state_ == ScannerState::FAILED) {
      ESP_LOGE(TAG, "Cannot start scan while already failed.");
    } else if (this->scanner_state_ == ScannerState::STOPPED) {
      ESP_LOGE(TAG, "Cannot start scan while already stopped.");
    }
    return;
  }
  this->set_scanner_state_(ScannerState::STARTING);
  ESP_LOGD(TAG, "Starting scan, set scanner state to STARTING.");
  if (!first) {
    for (auto *listener : this->listeners_)
      listener->on_scan_end();
  }
  this->already_discovered_.clear();
  this->scan_params_.scan_type = this->scan_active_ ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE;
  this->scan_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  this->scan_params_.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  this->scan_params_.scan_interval = this->scan_interval_;
  this->scan_params_.scan_window = this->scan_window_;

  // Start timeout before scan is started. Otherwise scan never starts if any error.
  this->set_timeout("scan", this->scan_duration_ * 2000, []() {
    ESP_LOGE(TAG, "ESP-IDF BLE scan never terminated, rebooting to restore BLE stack...");
    App.reboot();
  });

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

void ESP32BLETracker::end_of_scan_() {
  // The lock must be held when calling this function.
  if (this->scanner_state_ != ScannerState::STOPPED) {
    ESP_LOGE(TAG, "end_of_scan_ called while scanner is not stopped.");
    return;
  }
  ESP_LOGD(TAG, "End of scan, set scanner state to IDLE.");
  this->already_discovered_.clear();
  this->cancel_timeout("scan");

  for (auto *listener : this->listeners_)
    listener->on_scan_end();
  this->set_scanner_state_(ScannerState::IDLE);
}

void ESP32BLETracker::register_client(ESPBTClient *client) {
  client->app_id = ++this->app_id_;
  this->clients_.push_back(client);
  this->recalculate_advertisement_parser_types();
}

void ESP32BLETracker::register_listener(ESPBTDeviceListener *listener) {
  listener->set_parent(this);
  this->listeners_.push_back(listener);
  this->recalculate_advertisement_parser_types();
}

void ESP32BLETracker::recalculate_advertisement_parser_types() {
  this->raw_advertisements_ = false;
  this->parse_advertisements_ = false;
  for (auto *listener : this->listeners_) {
    if (listener->get_advertisement_parser_type() == AdvertisementParserType::PARSED_ADVERTISEMENTS) {
      this->parse_advertisements_ = true;
    } else {
      this->raw_advertisements_ = true;
    }
  }
  for (auto *client : this->clients_) {
    if (client->get_advertisement_parser_type() == AdvertisementParserType::PARSED_ADVERTISEMENTS) {
      this->parse_advertisements_ = true;
    } else {
      this->raw_advertisements_ = true;
    }
  }
}

void ESP32BLETracker::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
      this->gap_scan_result_(param->scan_rst);
      break;
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
  for (auto *client : this->clients_) {
    client->gap_event_handler(event, param);
  }
}

void ESP32BLETracker::gap_scan_set_param_complete_(const esp_ble_gap_cb_param_t::ble_scan_param_cmpl_evt_param &param) {
  ESP_LOGV(TAG, "gap_scan_set_param_complete - status %d", param.status);
  if (param.status == ESP_BT_STATUS_DONE) {
    this->scan_set_param_failed_ = ESP_BT_STATUS_SUCCESS;
  } else {
    this->scan_set_param_failed_ = param.status;
  }
}

void ESP32BLETracker::gap_scan_start_complete_(const esp_ble_gap_cb_param_t::ble_scan_start_cmpl_evt_param &param) {
  ESP_LOGV(TAG, "gap_scan_start_complete - status %d", param.status);
  this->scan_start_failed_ = param.status;
  if (this->scanner_state_ != ScannerState::STARTING) {
    if (this->scanner_state_ == ScannerState::RUNNING) {
      ESP_LOGE(TAG, "Scan was already running when start complete.");
    } else if (this->scanner_state_ == ScannerState::STOPPING) {
      ESP_LOGE(TAG, "Scan was stopping when start complete.");
    } else if (this->scanner_state_ == ScannerState::FAILED) {
      ESP_LOGE(TAG, "Scan was in failed state when start complete.");
    } else if (this->scanner_state_ == ScannerState::IDLE) {
      ESP_LOGE(TAG, "Scan was idle when start complete.");
    } else if (this->scanner_state_ == ScannerState::STOPPED) {
      ESP_LOGE(TAG, "Scan was stopped when start complete.");
    }
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
  ESP_LOGV(TAG, "gap_scan_stop_complete - status %d", param.status);
  if (this->scanner_state_ != ScannerState::STOPPING) {
    if (this->scanner_state_ == ScannerState::RUNNING) {
      ESP_LOGE(TAG, "Scan was not running when stop complete.");
    } else if (this->scanner_state_ == ScannerState::STARTING) {
      ESP_LOGE(TAG, "Scan was not started when stop complete.");
    } else if (this->scanner_state_ == ScannerState::FAILED) {
      ESP_LOGE(TAG, "Scan was in failed state when stop complete.");
    } else if (this->scanner_state_ == ScannerState::IDLE) {
      ESP_LOGE(TAG, "Scan was idle when stop complete.");
    } else if (this->scanner_state_ == ScannerState::STOPPED) {
      ESP_LOGE(TAG, "Scan was stopped when stop complete.");
    }
  }
  this->set_scanner_state_(ScannerState::STOPPED);
}

void ESP32BLETracker::gap_scan_result_(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param) {
  ESP_LOGV(TAG, "gap_scan_result - event %d", param.search_evt);
  if (param.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
    if (xSemaphoreTake(this->scan_result_lock_, 0)) {
      if (this->scan_result_index_ < ESP32BLETracker::SCAN_RESULT_BUFFER_SIZE) {
        this->scan_result_buffer_[this->scan_result_index_++] = param;
      }
      xSemaphoreGive(this->scan_result_lock_);
    }
  } else if (param.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
    // Scan finished on its own
    if (this->scanner_state_ != ScannerState::RUNNING) {
      if (this->scanner_state_ == ScannerState::STOPPING) {
        ESP_LOGE(TAG, "Scan was not running when scan completed.");
      } else if (this->scanner_state_ == ScannerState::STARTING) {
        ESP_LOGE(TAG, "Scan was not started when scan completed.");
      } else if (this->scanner_state_ == ScannerState::FAILED) {
        ESP_LOGE(TAG, "Scan was in failed state when scan completed.");
      } else if (this->scanner_state_ == ScannerState::IDLE) {
        ESP_LOGE(TAG, "Scan was idle when scan completed.");
      } else if (this->scanner_state_ == ScannerState::STOPPED) {
        ESP_LOGE(TAG, "Scan was stopped when scan completed.");
      }
    }
    this->set_scanner_state_(ScannerState::STOPPED);
  }
}

void ESP32BLETracker::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  for (auto *client : this->clients_) {
    client->gattc_event_handler(event, gattc_if, param);
  }
}

void ESP32BLETracker::set_scanner_state_(ScannerState state) {
  this->scanner_state_ = state;
  this->scanner_state_callbacks_.call(state);
}

ESPBLEiBeacon::ESPBLEiBeacon(const uint8_t *data) { memcpy(&this->beacon_data_, data, sizeof(beacon_data_)); }
optional<ESPBLEiBeacon> ESPBLEiBeacon::from_manufacturer_data(const ServiceData &data) {
  if (!data.uuid.contains(0x4C, 0x00))
    return {};

  if (data.data.size() != 23)
    return {};
  return ESPBLEiBeacon(data.data.data());
}

void ESPBTDevice::parse_scan_rst(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param) {
  this->scan_result_ = param;
  for (uint8_t i = 0; i < ESP_BD_ADDR_LEN; i++)
    this->address_[i] = param.bda[i];
  this->address_type_ = param.ble_addr_type;
  this->rssi_ = param.rssi;
  this->parse_adv_(param);

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

  ESP_LOGVV(TAG, "  Adv data: %s", format_hex_pretty(param.ble_adv, param.adv_data_len + param.scan_rsp_len).c_str());
#endif
}
void ESPBTDevice::parse_adv_(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param) {
  size_t offset = 0;
  const uint8_t *payload = param.ble_adv;
  uint8_t len = param.adv_data_len + param.scan_rsp_len;

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
  char mac[24];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", this->address_[0], this->address_[1], this->address_[2],
           this->address_[3], this->address_[4], this->address_[5]);
  return mac;
}
uint64_t ESPBTDevice::address_uint64() const { return esp32_ble::ble_addr_to_uint64(this->address_); }

void ESP32BLETracker::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Tracker:");
  ESP_LOGCONFIG(TAG, "  Scan Duration: %" PRIu32 " s", this->scan_duration_);
  ESP_LOGCONFIG(TAG, "  Scan Interval: %.1f ms", this->scan_interval_ * 0.625f);
  ESP_LOGCONFIG(TAG, "  Scan Window: %.1f ms", this->scan_window_ * 0.625f);
  ESP_LOGCONFIG(TAG, "  Scan Type: %s", this->scan_active_ ? "ACTIVE" : "PASSIVE");
  ESP_LOGCONFIG(TAG, "  Continuous Scanning: %s", YESNO(this->scan_continuous_));
  switch (this->scanner_state_) {
    case ScannerState::IDLE:
      ESP_LOGCONFIG(TAG, "  Scanner State: IDLE");
      break;
    case ScannerState::STARTING:
      ESP_LOGCONFIG(TAG, "  Scanner State: STARTING");
      break;
    case ScannerState::RUNNING:
      ESP_LOGCONFIG(TAG, "  Scanner State: RUNNING");
      break;
    case ScannerState::STOPPING:
      ESP_LOGCONFIG(TAG, "  Scanner State: STOPPING");
      break;
    case ScannerState::STOPPED:
      ESP_LOGCONFIG(TAG, "  Scanner State: STOPPED");
      break;
    case ScannerState::FAILED:
      ESP_LOGCONFIG(TAG, "  Scanner State: FAILED");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Connecting: %d, discovered: %d, searching: %d, disconnecting: %d", connecting_, discovered_,
                searching_, disconnecting_);
  if (this->scan_start_fail_count_) {
    ESP_LOGCONFIG(TAG, "  Scan Start Fail Count: %d", this->scan_start_fail_count_);
  }
}

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

}  // namespace esp32_ble_tracker
}  // namespace esphome

#endif
