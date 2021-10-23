#ifdef USE_ESP32

#include "esp32_ble_tracker.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

#include <nvs_flash.h>
#include <freertos/FreeRTOSConfig.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_gap_ble_api.h>
#include <esp_bt_defs.h>

#ifdef USE_ARDUINO
#include <esp32-hal-bt.h>
#endif

// bt_trace.h
#undef TAG

namespace esphome {
namespace esp32_ble_tracker {

static const char *const TAG = "esp32_ble_tracker";

ESP32BLETracker *global_esp32_ble_tracker = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

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

float ESP32BLETracker::get_setup_priority() const { return setup_priority::BLUETOOTH; }

void ESP32BLETracker::setup() {
  global_esp32_ble_tracker = this;
  this->scan_result_lock_ = xSemaphoreCreateMutex();
  this->scan_end_lock_ = xSemaphoreCreateMutex();

  if (!ESP32BLETracker::ble_setup()) {
    this->mark_failed();
    return;
  }

  global_esp32_ble_tracker->start_scan_(true);
}

void ESP32BLETracker::loop() {
  BLEEvent *ble_event = this->ble_events_.pop();
  while (ble_event != nullptr) {
    if (ble_event->type_)
      this->real_gattc_event_handler_(ble_event->event_.gattc.gattc_event, ble_event->event_.gattc.gattc_if,
                                      &ble_event->event_.gattc.gattc_param);
    else
      this->real_gap_event_handler_(ble_event->event_.gap.gap_event, &ble_event->event_.gap.gap_param);
    delete ble_event;  // NOLINT(cppcoreguidelines-owning-memory)
    ble_event = this->ble_events_.pop();
  }

  bool connecting = false;
  for (auto *client : this->clients_) {
    if (client->state() == ClientState::CONNECTING || client->state() == ClientState::DISCOVERED)
      connecting = true;
  }
  if (!connecting && xSemaphoreTake(this->scan_end_lock_, 0L)) {
    xSemaphoreGive(this->scan_end_lock_);
    global_esp32_ble_tracker->start_scan_(false);
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

      for (auto *client : this->clients_)
        if (client->parse_device(device)) {
          found = true;
          if (client->state() == ClientState::DISCOVERED) {
            esp_ble_gap_stop_scanning();
            if (xSemaphoreTake(this->scan_end_lock_, 10L / portTICK_PERIOD_MS)) {
              xSemaphoreGive(this->scan_end_lock_);
            }
          }
        }

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

#ifdef USE_ARDUINO
  if (!btStart()) {
    ESP_LOGE(TAG, "btStart failed: %d", esp_bt_controller_get_status());
    return false;
  }
#else
  if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
    // start bt controller
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
      esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
      err = esp_bt_controller_init(&cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %s", esp_err_to_name(err));
        return false;
      }
      while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE)
        ;
    }
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
      err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %s", esp_err_to_name(err));
        return false;
      }
    }
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
      ESP_LOGE(TAG, "esp bt controller enable failed");
      return false;
    }
  }
#endif

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
  err = esp_ble_gattc_register_callback(ESP32BLETracker::gattc_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gattc_register_callback failed: %d", err);
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

void ESP32BLETracker::start_scan_(bool first) {
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

void ESP32BLETracker::register_client(ESPBTClient *client) {
  client->app_id = ++this->app_id_;
  this->clients_.push_back(client);
}

void ESP32BLETracker::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  BLEEvent *gap_event = new BLEEvent(event, param);  // NOLINT(cppcoreguidelines-owning-memory)
  global_esp32_ble_tracker->ble_events_.push(gap_event);
}  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

void ESP32BLETracker::real_gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
      global_esp32_ble_tracker->gap_scan_result_(param->scan_rst);
      break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      global_esp32_ble_tracker->gap_scan_set_param_complete_(param->scan_param_cmpl);
      break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      global_esp32_ble_tracker->gap_scan_start_complete_(param->scan_start_cmpl);
      break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      global_esp32_ble_tracker->gap_scan_stop_complete_(param->scan_stop_cmpl);
      break;
    default:
      break;
  }
}

void ESP32BLETracker::gap_scan_set_param_complete_(const esp_ble_gap_cb_param_t::ble_scan_param_cmpl_evt_param &param) {
  this->scan_set_param_failed_ = param.status;
}

void ESP32BLETracker::gap_scan_start_complete_(const esp_ble_gap_cb_param_t::ble_scan_start_cmpl_evt_param &param) {
  this->scan_start_failed_ = param.status;
}

void ESP32BLETracker::gap_scan_stop_complete_(const esp_ble_gap_cb_param_t::ble_scan_stop_cmpl_evt_param &param) {
  xSemaphoreGive(this->scan_end_lock_);
}

void ESP32BLETracker::gap_scan_result_(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param) {
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

void ESP32BLETracker::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  BLEEvent *gattc_event = new BLEEvent(event, gattc_if, param);  // NOLINT(cppcoreguidelines-owning-memory)
  global_esp32_ble_tracker->ble_events_.push(gattc_event);
}  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

void ESP32BLETracker::real_gattc_event_handler_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                                esp_ble_gattc_cb_param_t *param) {
  for (auto *client : global_esp32_ble_tracker->clients_) {
    client->gattc_event_handler(event, gattc_if, param);
  }
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
ESPBTUUID ESPBTUUID::from_raw(const std::string &data) {
  ESPBTUUID ret;
  if (data.length() == 4) {
    ret.uuid_.len = ESP_UUID_LEN_16;
    ret.uuid_.uuid.uuid16 = 0;
    for (int i = 0; i < data.length();) {
      uint8_t msb = data.c_str()[i];
      uint8_t lsb = data.c_str()[i + 1];

      if (msb > '9')
        msb -= 7;
      if (lsb > '9')
        lsb -= 7;
      ret.uuid_.uuid.uuid16 += (((msb & 0x0F) << 4) | (lsb & 0x0F)) << (2 - i) * 4;
      i += 2;
    }
  } else if (data.length() == 8) {
    ret.uuid_.len = ESP_UUID_LEN_32;
    ret.uuid_.uuid.uuid32 = 0;
    for (int i = 0; i < data.length();) {
      uint8_t msb = data.c_str()[i];
      uint8_t lsb = data.c_str()[i + 1];

      if (msb > '9')
        msb -= 7;
      if (lsb > '9')
        lsb -= 7;
      ret.uuid_.uuid.uuid32 += (((msb & 0x0F) << 4) | (lsb & 0x0F)) << (6 - i) * 4;
      i += 2;
    }
  } else if (data.length() == 16) {  // how we can have 16 byte length string reprezenting 128 bit uuid??? needs to be
                                     // investigated (lack of time)
    ret.uuid_.len = ESP_UUID_LEN_128;
    memcpy(ret.uuid_.uuid.uuid128, (uint8_t *) data.data(), 16);
  } else if (data.length() == 36) {
    // If the length of the string is 36 bytes then we will assume it is a long hex string in
    // UUID format.
    ret.uuid_.len = ESP_UUID_LEN_128;
    int n = 0;
    for (int i = 0; i < data.length();) {
      if (data.c_str()[i] == '-')
        i++;
      uint8_t msb = data.c_str()[i];
      uint8_t lsb = data.c_str()[i + 1];

      if (msb > '9')
        msb -= 7;
      if (lsb > '9')
        lsb -= 7;
      ret.uuid_.uuid.uuid128[15 - n++] = ((msb & 0x0F) << 4) | (lsb & 0x0F);
      i += 2;
    }
  } else {
    ESP_LOGE(TAG, "ERROR: UUID value not 2, 4, 16 or 36 bytes - %s", data.c_str());
  }
  return ret;
}
ESPBTUUID ESPBTUUID::from_uuid(esp_bt_uuid_t uuid) {
  ESPBTUUID ret;
  ret.uuid_.len = uuid.len;
  ret.uuid_.uuid.uuid16 = uuid.uuid.uuid16;
  ret.uuid_.uuid.uuid32 = uuid.uuid.uuid32;
  for (size_t i = 0; i < ESP_UUID_LEN_128; i++)
    ret.uuid_.uuid.uuid128[i] = uuid.uuid.uuid128[i];
  return ret;
}
ESPBTUUID ESPBTUUID::as_128bit() const {
  if (this->uuid_.len == ESP_UUID_LEN_128) {
    return *this;
  }
  uint8_t data[] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint32_t uuid32;
  if (this->uuid_.len == ESP_UUID_LEN_32) {
    uuid32 = this->uuid_.uuid.uuid32;
  } else {
    uuid32 = this->uuid_.uuid.uuid16;
  }
  for (uint8_t i = 0; i < this->uuid_.len; i++) {
    data[12 + i] = ((uuid32 >> i * 8) & 0xFF);
  }
  return ESPBTUUID::from_raw(data);
}
bool ESPBTUUID::contains(uint8_t data1, uint8_t data2) const {
  if (this->uuid_.len == ESP_UUID_LEN_16) {
    return (this->uuid_.uuid.uuid16 >> 8) == data2 && (this->uuid_.uuid.uuid16 & 0xFF) == data1;
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
bool ESPBTUUID::operator==(const ESPBTUUID &uuid) const {
  if (this->uuid_.len == uuid.uuid_.len) {
    switch (this->uuid_.len) {
      case ESP_UUID_LEN_16:
        if (uuid.uuid_.uuid.uuid16 == this->uuid_.uuid.uuid16) {
          return true;
        }
        break;
      case ESP_UUID_LEN_32:
        if (uuid.uuid_.uuid.uuid32 == this->uuid_.uuid.uuid32) {
          return true;
        }
        break;
      case ESP_UUID_LEN_128:
        for (int i = 0; i < ESP_UUID_LEN_128; i++) {
          if (uuid.uuid_.uuid.uuid128[i] != this->uuid_.uuid.uuid128[i]) {
            return false;
          }
        }
        return true;
        break;
    }
  } else {
    return this->as_128bit() == uuid.as_128bit();
  }
  return false;
}
esp_bt_uuid_t ESPBTUUID::get_uuid() const { return this->uuid_; }
std::string ESPBTUUID::to_string() const {
  char sbuf[64];
  switch (this->uuid_.len) {
    case ESP_UUID_LEN_16:
      sprintf(sbuf, "0x%02X%02X", this->uuid_.uuid.uuid16 >> 8, this->uuid_.uuid.uuid16 & 0xff);
      break;
    case ESP_UUID_LEN_32:
      sprintf(sbuf, "0x%02X%02X%02X%02X", this->uuid_.uuid.uuid32 >> 24, (this->uuid_.uuid.uuid32 >> 16 & 0xff),
              (this->uuid_.uuid.uuid32 >> 8 & 0xff), this->uuid_.uuid.uuid32 & 0xff);
      break;
    default:
    case ESP_UUID_LEN_128:
      char *bpos = sbuf;
      for (int8_t i = 15; i >= 0; i--) {
        sprintf(bpos, "%02X", this->uuid_.uuid.uuid128[i]);
        bpos += 2;
        if (i == 6 || i == 8 || i == 10 || i == 12)
          sprintf(bpos++, "-");
      }
      sbuf[47] = '\0';
      break;
  }
  return sbuf;
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
    ESP_LOGVV(TAG, "  Manufacturer data: %s", hexencode(data.data).c_str());
    if (this->get_ibeacon().has_value()) {
      auto ibeacon = this->get_ibeacon().value();
      ESP_LOGVV(TAG, "    iBeacon data:");
      ESP_LOGVV(TAG, "      UUID: %s", ibeacon.get_uuid().to_string().c_str());
      ESP_LOGVV(TAG, "      Major: %u", ibeacon.get_major());
      ESP_LOGVV(TAG, "      Minor: %u", ibeacon.get_minor());
      ESP_LOGVV(TAG, "      TXPower: %d", ibeacon.get_signal_power());
    }
  }
  for (auto &data : this->service_datas_) {
    ESP_LOGVV(TAG, "  Service data:");
    ESP_LOGVV(TAG, "    UUID: %s", data.uuid.to_string().c_str());
    ESP_LOGVV(TAG, "    Data: %s", hexencode(data.data).c_str());
  }

  ESP_LOGVV(TAG, "Adv data: %s", hexencode(param.ble_adv, param.adv_data_len + param.scan_rsp_len).c_str());
#endif
}
void ESPBTDevice::parse_adv_(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &param) {
  size_t offset = 0;
  const uint8_t *payload = param.ble_adv;
  uint8_t len = param.adv_data_len + param.scan_rsp_len;

  while (offset + 2 < len) {
    const uint8_t field_length = payload[offset++];  // First byte is length of adv record
    if (field_length == 0)
      break;

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
      case ESP_BLE_AD_TYPE_NAME_CMPL: {
        // CSS 1.2 LOCAL NAME
        // "The Local Name data type shall be the same as, or a shortened version of, the local name assigned to the
        // device." CSS 1: Optional in this context; shall not appear more than once in a block.
        this->name_ = std::string(reinterpret_cast<const char *>(record), record_length);
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

void ESP32BLETracker::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Tracker:");
  ESP_LOGCONFIG(TAG, "  Scan Duration: %u s", this->scan_duration_);
  ESP_LOGCONFIG(TAG, "  Scan Interval: %.1f ms", this->scan_interval_ * 0.625f);
  ESP_LOGCONFIG(TAG, "  Scan Window: %.1f ms", this->scan_window_ * 0.625f);
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
  for (auto &tx_power : device.get_tx_powers()) {
    ESP_LOGD(TAG, "  TX Power: %d", tx_power);
  }
}

}  // namespace esp32_ble_tracker
}  // namespace esphome

#endif
