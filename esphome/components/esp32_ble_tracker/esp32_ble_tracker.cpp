#include "esp32_ble_tracker.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

#ifdef ARDUINO_ARCH_ESP32

#include <nvs_flash.h>
#include <freertos/FreeRTOSConfig.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <freertos/task.h>
#include <esp_gap_ble_api.h>
#include <esp_bt_defs.h>

// bt_trace.h
#undef TAG

namespace esphome {
namespace esp32_ble_tracker {

static const char *TAG = "esp32_ble_tracker";

ESP32BLETracker *global_esp32_ble_tracker = nullptr;

uint64_t ble_addr_to_uint64(const esp_bd_addr_t address) {
  uint64_t u = 0;
  u |= uint64_t(address[0] & 0xFF) << 40;
  u |= uint64_t(address[1] & 0xFF) << 32;
  u |= uint64_t(address[2] & 0xFF) << 24;
  u |= uint64_t(address[3] & 0xFF) << 16;
  u |= uint64_t(address[4] & 0xFF) << 8;
  u |= uint64_t(address[5] & 0xFF) << 0;
  return u;
}

void ESP32BLETracker::setup() {
  global_esp32_ble_tracker = this;
  this->scan_result_lock_ = xSemaphoreCreateMutex();
  this->scan_end_lock_ = xSemaphoreCreateMutex();

  if (!ESP32BLETracker::ble_setup()) {
    this->mark_failed();
    return;
  }

  global_esp32_ble_tracker->start_scan(true);
}

void ESP32BLETracker::loop() {
  if (xSemaphoreTake(this->scan_end_lock_, 0L)) {
    xSemaphoreGive(this->scan_end_lock_);
    global_esp32_ble_tracker->start_scan(false);
  }

  if (xSemaphoreTake(this->scan_result_lock_, 5L / portTICK_PERIOD_MS)) {
    uint32_t index = this->scan_result_index_;
    xSemaphoreGive(this->scan_result_lock_);

    if (index >= 16) {
      ESP_LOGW(TAG, "Too many BLE events to process. Some devices may not show up.");
    }
    for (size_t i = 0; i < index; i++) {
      ESPBTDevice device;
      device.parse_scan_rst(this->scan_result_buffer_[i]);

      bool found = false;
      for (auto *listener : this->listeners_)
        if (listener->parse_device(device))
          found = true;

      if (!found) {
        this->print_bt_device_info(device);
      }
    }

    if (xSemaphoreTake(this->scan_result_lock_, 10L / portTICK_PERIOD_MS)) {
      this->scan_result_index_ = 0;
      xSemaphoreGive(this->scan_result_lock_);
    }
  }

  if (this->scan_set_param_failed_) {
    ESP_LOGE(TAG, "Scan set param failed: %d", this->scan_set_param_failed_);
    this->scan_set_param_failed_ = ESP_BT_STATUS_SUCCESS;
  }

  if (this->scan_start_failed_) {
    ESP_LOGE(TAG, "Scan start failed: %d", this->scan_start_failed_);
    this->scan_start_failed_ = ESP_BT_STATUS_SUCCESS;
  }
}

bool ESP32BLETracker::ble_setup() {
  // Initialize non-volatile storage for the bluetooth controller
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %d", err);
    return false;
  }

  // Initialize the bluetooth controller with the default configuration
  if (!btStart()) {
    ESP_LOGE(TAG, "btStart failed: %d", esp_bt_controller_get_status());
    return false;
  }

  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  err = esp_bluedroid_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", err);
    return false;
  }
  err = esp_bluedroid_enable();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", err);
    return false;
  }
  err = esp_ble_gap_register_callback(ESP32BLETracker::gap_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", err);
    return false;
  }

  // Empty name
  esp_ble_gap_set_device_name("");

  esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
  err = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_set_security_param failed: %d", err);
    return false;
  }

  // BLE takes some time to be fully set up, 200ms should be more than enough
  delay(200);  // NOLINT

  return true;
}

void ESP32BLETracker::start_scan(bool first) {
  if (!xSemaphoreTake(this->scan_end_lock_, 0L)) {
    ESP_LOGW(TAG, "Cannot start scan!");
    return;
  }

  ESP_LOGD(TAG, "Starting scan...");
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

  esp_ble_gap_set_scan_params(&this->scan_params_);
  esp_ble_gap_start_scanning(this->scan_duration_);

  this->set_timeout("scan", this->scan_duration_ * 2000, []() {
    ESP_LOGW(TAG, "ESP-IDF BLE scan never terminated, rebooting to restore BLE stack...");
    App.reboot();
  });
}

void ESP32BLETracker::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
      global_esp32_ble_tracker->gap_scan_result(param->scan_rst);
      break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      global_esp32_ble_tracker->gap_scan_set_param_complete(param->scan_param_cmpl);
      break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      global_esp32_ble_tracker->gap_scan_start_complete(param->scan_start_cmpl);
      break;
    default:
      break;
  }
}

void ESP32BLETracker::gap_scan_set_param_complete(const esp_ble_gap_cb_param_t::ble_scan_param_cmpl_evt_param &param) {
  this->scan_set_param_failed_ = param.status;
}

void ESP32BLETracker::gap_scan_start_complete(const esp_ble_gap_cb_param_t::ble_scan_start_cmpl_evt_param &param) {
  this->scan_start_failed_ = param.status;
}

void ESP32BLETracker::gap_scan_result(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param) {
  if (param.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
    if (xSemaphoreTake(this->scan_result_lock_, 0L)) {
      if (this->scan_result_index_ < 16) {
        this->scan_result_buffer_[this->scan_result_index_++] = param;
      }
      xSemaphoreGive(this->scan_result_lock_);
    }
  } else if (param.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
    xSemaphoreGive(this->scan_end_lock_);
  }
}

std::string hexencode_string(const std::string &raw_data) {
  return hexencode(reinterpret_cast<const uint8_t *>(raw_data.c_str()), raw_data.size());
}

ESPBTUUID::ESPBTUUID() : uuid_() {}
ESPBTUUID ESPBTUUID::from_uint16(uint16_t uuid) {
  ESPBTUUID ret;
  ret.uuid_.len = ESP_UUID_LEN_16;
  ret.uuid_.uuid.uuid16 = uuid;
  return ret;
}
ESPBTUUID ESPBTUUID::from_uint32(uint32_t uuid) {
  ESPBTUUID ret;
  ret.uuid_.len = ESP_UUID_LEN_32;
  ret.uuid_.uuid.uuid32 = uuid;
  return ret;
}
ESPBTUUID ESPBTUUID::from_raw(const uint8_t *data) {
  ESPBTUUID ret;
  ret.uuid_.len = ESP_UUID_LEN_128;
  for (size_t i = 0; i < ESP_UUID_LEN_128; i++)
    ret.uuid_.uuid.uuid128[i] = data[i];
  return ret;
}
bool ESPBTUUID::contains(uint8_t data1, uint8_t data2) const {
  if (this->uuid_.len == ESP_UUID_LEN_16) {
    return (this->uuid_.uuid.uuid16 >> 8) == data2 || (this->uuid_.uuid.uuid16 & 0xFF) == data1;
  } else if (this->uuid_.len == ESP_UUID_LEN_32) {
    for (uint8_t i = 0; i < 3; i++) {
      bool a = ((this->uuid_.uuid.uuid32 >> i * 8) & 0xFF) == data1;
      bool b = ((this->uuid_.uuid.uuid32 >> (i + 1) * 8) & 0xFF) == data2;
      if (a && b)
        return true;
    }
  } else {
    for (uint8_t i = 0; i < 15; i++) {
      if (this->uuid_.uuid.uuid128[i] == data1 && this->uuid_.uuid.uuid128[i + 1] == data2)
        return true;
    }
  }
  return false;
}
std::string ESPBTUUID::to_string() {
  char sbuf[64];
  switch (this->uuid_.len) {
    case ESP_UUID_LEN_16:
      sprintf(sbuf, "%02X:%02X", this->uuid_.uuid.uuid16 >> 8, this->uuid_.uuid.uuid16);
      break;
    case ESP_UUID_LEN_32:
      sprintf(sbuf, "%02X:%02X:%02X:%02X", this->uuid_.uuid.uuid32 >> 24, this->uuid_.uuid.uuid32 >> 16,
              this->uuid_.uuid.uuid32 >> 8, this->uuid_.uuid.uuid32);
      break;
    default:
    case ESP_UUID_LEN_128:
      for (uint8_t i = 0; i < 16; i++)
        sprintf(sbuf + i * 3, "%02X:", this->uuid_.uuid.uuid128[i]);
      sbuf[47] = '\0';
      break;
  }
  return sbuf;
}

ESPBLEiBeacon::ESPBLEiBeacon(const uint8_t *data) { memcpy(&this->beacon_data_, data, sizeof(beacon_data_)); }
optional<ESPBLEiBeacon> ESPBLEiBeacon::from_manufacturer_data(const std::string &data) {
  if (data.size() != 25)
    return {};
  if (data[0] != 0x4C || data[1] != 0x00)
    return {};

  return ESPBLEiBeacon(reinterpret_cast<const uint8_t *>(data.data()));
}

void ESPBTDevice::parse_scan_rst(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param) {
  for (uint8_t i = 0; i < ESP_BD_ADDR_LEN; i++)
    this->address_[i] = param.bda[i];
  this->address_type_ = param.ble_addr_type;
  this->rssi_ = param.rssi;
  this->parse_adv_(param);

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "Parse Result:");
  const char *address_type = "";
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
  }
  ESP_LOGVV(TAG, "  Address: %02X:%02X:%02X:%02X:%02X:%02X (%s)", this->address_[0], this->address_[1],
            this->address_[2], this->address_[3], this->address_[4], this->address_[5], address_type);

  ESP_LOGVV(TAG, "  RSSI: %d", this->rssi_);
  ESP_LOGVV(TAG, "  Name: '%s'", this->name_.c_str());
  if (this->tx_power_.has_value()) {
    ESP_LOGVV(TAG, "  TX Power: %d", *this->tx_power_);
  }
  if (this->appearance_.has_value()) {
    ESP_LOGVV(TAG, "  Appearance: %u", *this->appearance_);
  }
  if (this->ad_flag_.has_value()) {
    ESP_LOGVV(TAG, "  Ad Flag: %u", *this->ad_flag_);
  }
  for (auto uuid : this->service_uuids_) {
    ESP_LOGVV(TAG, "  Service UUID: %s", uuid.to_string().c_str());
  }
  ESP_LOGVV(TAG, "  Manufacturer data: %s", hexencode_string(this->manufacturer_data_).c_str());
  ESP_LOGVV(TAG, "  Service data: %s", hexencode_string(this->service_data_).c_str());

  if (this->service_data_uuid_.has_value()) {
    ESP_LOGVV(TAG, "  Service Data UUID: %s", this->service_data_uuid_->to_string().c_str());
  }

  ESP_LOGVV(TAG, "Adv data: %s",
            hexencode_string(std::string(reinterpret_cast<const char *>(param.ble_adv), param.adv_data_len)).c_str());
#endif
}
void ESPBTDevice::parse_adv_(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param) {
  size_t offset = 0;
  const uint8_t *payload = param.ble_adv;
  uint8_t len = param.adv_data_len;

  while (offset + 2 < len) {
    const uint8_t field_length = payload[offset++];  // First byte is length of adv record
    if (field_length == 0)
      break;

    // first byte of adv record is adv record type
    const uint8_t record_type = payload[offset++];
    const uint8_t *record = &payload[offset];
    const uint8_t record_length = field_length - 1;
    offset += record_length;

    switch (record_type) {
      case ESP_BLE_AD_TYPE_NAME_CMPL: {
        this->name_ = std::string(reinterpret_cast<const char *>(record), record_length);
        break;
      }
      case ESP_BLE_AD_TYPE_TX_PWR: {
        this->tx_power_ = *payload;
        break;
      }
      case ESP_BLE_AD_TYPE_APPEARANCE: {
        this->appearance_ = *reinterpret_cast<const uint16_t *>(record);
        break;
      }
      case ESP_BLE_AD_TYPE_FLAG: {
        this->ad_flag_ = *record;
        break;
      }
      case ESP_BLE_AD_TYPE_16SRV_CMPL:
      case ESP_BLE_AD_TYPE_16SRV_PART: {
        for (uint8_t i = 0; i < record_length / 2; i++) {
          this->service_uuids_.push_back(ESPBTUUID::from_uint16(*reinterpret_cast<const uint16_t *>(record + 2 * i)));
        }
        break;
      }
      case ESP_BLE_AD_TYPE_32SRV_CMPL:
      case ESP_BLE_AD_TYPE_32SRV_PART: {
        for (uint8_t i = 0; i < record_length / 4; i++) {
          this->service_uuids_.push_back(ESPBTUUID::from_uint32(*reinterpret_cast<const uint32_t *>(record + 4 * i)));
        }
        break;
      }
      case ESP_BLE_AD_TYPE_128SRV_CMPL:
      case ESP_BLE_AD_TYPE_128SRV_PART: {
        this->service_uuids_.push_back(ESPBTUUID::from_raw(record));
        break;
      }
      case ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE: {
        this->manufacturer_data_ = std::string(reinterpret_cast<const char *>(record), record_length);
        break;
      }
      case ESP_BLE_AD_TYPE_SERVICE_DATA: {
        if (record_length < 2) {
          ESP_LOGV(TAG, "Record length too small for ESP_BLE_AD_TYPE_SERVICE_DATA");
          break;
        }
        this->service_data_uuid_ = ESPBTUUID::from_uint16(*reinterpret_cast<const uint16_t *>(record));
        if (record_length > 2)
          this->service_data_ = std::string(reinterpret_cast<const char *>(record + 2), record_length - 2UL);
        break;
      }
      case ESP_BLE_AD_TYPE_32SERVICE_DATA: {
        if (record_length < 4) {
          ESP_LOGV(TAG, "Record length too small for ESP_BLE_AD_TYPE_32SERVICE_DATA");
          break;
        }
        this->service_data_uuid_ = ESPBTUUID::from_uint32(*reinterpret_cast<const uint32_t *>(record));
        if (record_length > 4)
          this->service_data_ = std::string(reinterpret_cast<const char *>(record + 4), record_length - 4UL);
        break;
      }
      case ESP_BLE_AD_TYPE_128SERVICE_DATA: {
        if (record_length < 16) {
          ESP_LOGV(TAG, "Record length too small for ESP_BLE_AD_TYPE_128SERVICE_DATA");
          break;
        }
        this->service_data_uuid_ = ESPBTUUID::from_raw(record);
        if (record_length > 16)
          this->service_data_ = std::string(reinterpret_cast<const char *>(record + 16), record_length - 16UL);
        break;
      }
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
uint64_t ESPBTDevice::address_uint64() const { return ble_addr_to_uint64(this->address_); }
esp_ble_addr_type_t ESPBTDevice::get_address_type() const { return this->address_type_; }
int ESPBTDevice::get_rssi() const { return this->rssi_; }
const std::string &ESPBTDevice::get_name() const { return this->name_; }
const optional<int8_t> &ESPBTDevice::get_tx_power() const { return this->tx_power_; }
const optional<uint16_t> &ESPBTDevice::get_appearance() const { return this->appearance_; }
const optional<uint8_t> &ESPBTDevice::get_ad_flag() const { return this->ad_flag_; }
const std::vector<ESPBTUUID> &ESPBTDevice::get_service_uuids() const { return this->service_uuids_; }
const std::string &ESPBTDevice::get_manufacturer_data() const { return this->manufacturer_data_; }
const std::string &ESPBTDevice::get_service_data() const { return this->service_data_; }
const optional<ESPBTUUID> &ESPBTDevice::get_service_data_uuid() const { return this->service_data_uuid_; }

void ESP32BLETracker::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Tracker:");
  ESP_LOGCONFIG(TAG, "  Scan Duration: %u s", this->scan_duration_);
  ESP_LOGCONFIG(TAG, "  Scan Interval: %u ms", this->scan_interval_);
  ESP_LOGCONFIG(TAG, "  Scan Window: %u ms", this->scan_window_);
  ESP_LOGCONFIG(TAG, "  Scan Type: %s", this->scan_active_ ? "ACTIVE" : "PASSIVE");
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
  if (!device.get_name().empty())
    ESP_LOGD(TAG, "  Name: '%s'", device.get_name().c_str());
  if (device.get_tx_power().has_value()) {
    ESP_LOGD(TAG, "  TX Power: %d", *device.get_tx_power());
  }
}

}  // namespace esp32_ble_tracker
}  // namespace esphome

#endif
