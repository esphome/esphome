#include "ble_advertising.h"

#ifdef USE_ESP32

#include <cstdio>
#include <cstring>
#include "ble_uuid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp32_ble {

static const char *const TAG = "esp32_ble";

BLEAdvertising::BLEAdvertising() {
  this->advertising_data_.set_scan_rsp = false;
  this->advertising_data_.include_name = true;
  this->advertising_data_.include_txpower = true;
  this->advertising_data_.min_interval = 0;
  this->advertising_data_.max_interval = 0;
  this->advertising_data_.appearance = 0x00;
  this->advertising_data_.manufacturer_len = 0;
  this->advertising_data_.p_manufacturer_data = nullptr;
  this->advertising_data_.service_data_len = 0;
  this->advertising_data_.p_service_data = nullptr;
  this->advertising_data_.service_uuid_len = 0;
  this->advertising_data_.p_service_uuid = nullptr;
  this->advertising_data_.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  this->advertising_params_.adv_int_min = 0x20;
  this->advertising_params_.adv_int_max = 0x40;
  this->advertising_params_.adv_type = ADV_TYPE_IND;
  this->advertising_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  this->advertising_params_.channel_map = ADV_CHNL_ALL;
  this->advertising_params_.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  this->advertising_params_.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;
}

void BLEAdvertising::add_service_uuid(ESPBTUUID uuid) { this->advertising_uuids_.push_back(uuid); }
void BLEAdvertising::remove_service_uuid(ESPBTUUID uuid) {
  this->advertising_uuids_.erase(std::remove(this->advertising_uuids_.begin(), this->advertising_uuids_.end(), uuid),
                                 this->advertising_uuids_.end());
}

void BLEAdvertising::set_service_data(const std::vector<uint8_t> &data) {
  delete[] this->advertising_data_.p_service_data;
  this->advertising_data_.p_service_data = nullptr;
  this->advertising_data_.service_data_len = data.size();
  if (!data.empty()) {
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    this->advertising_data_.p_service_data = new uint8_t[data.size()];
    memcpy(this->advertising_data_.p_service_data, data.data(), data.size());
  }
}

void BLEAdvertising::set_manufacturer_data(const std::vector<uint8_t> &data) {
  delete[] this->advertising_data_.p_manufacturer_data;
  this->advertising_data_.p_manufacturer_data = nullptr;
  this->advertising_data_.manufacturer_len = data.size();
  if (!data.empty()) {
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    this->advertising_data_.p_manufacturer_data = new uint8_t[data.size()];
    memcpy(this->advertising_data_.p_manufacturer_data, data.data(), data.size());
  }
}

void BLEAdvertising::start() {
  int num_services = this->advertising_uuids_.size();
  if (num_services == 0) {
    this->advertising_data_.service_uuid_len = 0;
  } else {
    this->advertising_data_.service_uuid_len = 16 * num_services;
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    this->advertising_data_.p_service_uuid = new uint8_t[this->advertising_data_.service_uuid_len];
    uint8_t *p = this->advertising_data_.p_service_uuid;
    for (int i = 0; i < num_services; i++) {
      ESPBTUUID uuid = this->advertising_uuids_[i];
      memcpy(p, uuid.as_128bit().get_uuid().uuid.uuid128, 16);
      p += 16;
    }
  }

  esp_err_t err;

  this->advertising_data_.set_scan_rsp = false;
  this->advertising_data_.include_name = !this->scan_response_;
  this->advertising_data_.include_txpower = !this->scan_response_;
  err = esp_ble_gap_config_adv_data(&this->advertising_data_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_config_adv_data failed (Advertising): %d", err);
    return;
  }

  if (this->scan_response_) {
    memcpy(&this->scan_response_data_, &this->advertising_data_, sizeof(esp_ble_adv_data_t));
    this->scan_response_data_.set_scan_rsp = true;
    this->scan_response_data_.include_name = true;
    this->scan_response_data_.include_txpower = true;
    this->scan_response_data_.manufacturer_len = 0;
    this->scan_response_data_.appearance = 0;
    this->scan_response_data_.flag = 0;
    err = esp_ble_gap_config_adv_data(&this->scan_response_data_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gap_config_adv_data failed (Scan response): %d", err);
      return;
    }
  }

  if (this->advertising_data_.service_uuid_len > 0) {
    delete[] this->advertising_data_.p_service_uuid;
    this->advertising_data_.p_service_uuid = nullptr;
  }

  err = esp_ble_gap_start_advertising(&this->advertising_params_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed: %d", err);
    return;
  }
}

void BLEAdvertising::stop() {
  esp_err_t err = esp_ble_gap_stop_advertising();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_stop_advertising failed: %d", err);
    return;
  }
}

}  // namespace esp32_ble
}  // namespace esphome

#endif
