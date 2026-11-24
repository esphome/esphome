#include "ble_advertising.h"

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_ADVERTISING

#include <algorithm>
#include <cstring>
#include "esphome/components/ble_device_base/adv_payload.h"
#include "ble.h"
#include "ble_uuid.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifndef CONFIG_BT_CONTROLLER_DISABLED
#include <esp_bt.h>
#endif

namespace esphome::esp32_ble {

using ble_device_base::AdvPayload;
using ble_device_base::build_adv_packets;

ESPHOME_LOG_TAG(TAG, "esp32_ble.advertising");

// The TX power Bluedroid advertised, from its BTM_BLE_ADV_TX_POWER table
static int8_t adv_tx_power_dbm() {
#ifdef CONFIG_IDF_TARGET_ESP32
  static constexpr int8_t TX_POWER_DBM[] = {-12, -9, -6, -3, 0, 3, 6, 9};
#else
  static constexpr int8_t TX_POWER_DBM[] = {-24, -21, -18, -15, -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 18, 20};
#endif
#ifdef CONFIG_BT_CONTROLLER_DISABLED
  const size_t level = 0;  // a hosted controller; Bluedroid sends level 0 too
#else
  const size_t level = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
#endif
  return TX_POWER_DBM[std::min(level, std::size(TX_POWER_DBM) - 1)];
}

BLEAdvertising::BLEAdvertising(uint32_t advertising_cycle_time) : advertising_cycle_time_(advertising_cycle_time) {
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

void BLEAdvertising::set_service_data(std::span<const uint8_t> data) {
  // Too long to fit is the same as none
  this->service_data_len_ = data.size() <= MAX_DATA_LEN ? data.size() : 0;
  memcpy(this->service_data_.data(), data.data(), this->service_data_len_);
}

void BLEAdvertising::set_service_data(const std::vector<uint8_t> &data) {
  this->set_service_data(std::span<const uint8_t>(data));
}

void BLEAdvertising::set_manufacturer_data(const std::vector<uint8_t> &data) {
  // Only what fits goes on air
  this->manufacturer_data_len_ = std::min(data.size(), MAX_DATA_LEN);
  memcpy(this->manufacturer_data_.data(), data.data(), this->manufacturer_data_len_);
}

esp_err_t BLEAdvertising::services_advertisement_() {
  char name_buffer[ESP32BLE::DEVICE_NAME_BUFFER_SIZE];
  const char *name = global_ble->device_name(name_buffer);
  AdvPayload adv;
  AdvPayload rsp;
  build_adv_packets({.uuids = this->advertising_uuids_,
                     .priority_uuid = this->priority_uuid_,
                     .manufacturer_data = {this->manufacturer_data_.data(), this->manufacturer_data_len_},
                     .service_data = {this->service_data_.data(), this->service_data_len_},
                     .name = name,
                     .name_len = strlen(name),
                     .appearance = this->appearance_,
                     .tx_power_dbm = adv_tx_power_dbm(),
                     .name_in_adv = this->include_name_in_adv_},
                    adv, rsp);

  // Bluedroid copies the bytes during each call
  esp_err_t err = esp_ble_gap_config_adv_data_raw(const_cast<uint8_t *>(adv.data()), adv.size());
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_config_adv_data_raw failed: %s", esp_err_to_name(err));
    return err;
  }
  err = esp_ble_gap_config_scan_rsp_data_raw(const_cast<uint8_t *>(rsp.data()), rsp.size());
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_config_scan_rsp_data_raw failed: %s", esp_err_to_name(err));
    return err;
  }
  err = esp_ble_gap_start_advertising(&this->advertising_params_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
    return err;
  }
  return ESP_OK;
}

void BLEAdvertising::start() {
  if (this->current_adv_index_ == -1) {
    this->services_advertisement_();
  } else {
    this->raw_advertisements_callbacks_[this->current_adv_index_](true);
  }
}

void BLEAdvertising::stop() {
  this->start_pending_ = false;
  esp_err_t err = esp_ble_gap_stop_advertising();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_stop_advertising failed: %d", err);
    return;
  }
  if (this->current_adv_index_ != -1) {
    this->raw_advertisements_callbacks_[this->current_adv_index_](false);
  }
}

void BLEAdvertising::loop() {
  // Send a queued start after the event drain
  if (this->start_pending_) {
    this->start_pending_ = false;
    this->start();
  }
  if (this->raw_advertisements_callbacks_.empty()) {
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_advertisement_time_ > this->advertising_cycle_time_) {
    this->stop();
    this->current_adv_index_ += 1;
    if (static_cast<size_t>(this->current_adv_index_) >= this->raw_advertisements_callbacks_.size()) {
      this->current_adv_index_ = -1;
    }
    this->start();
    this->last_advertisement_time_ = now;
  }
}

void BLEAdvertising::register_raw_advertisement_callback(std::function<void(bool)> &&callback) {
  this->raw_advertisements_callbacks_.push_back(std::move(callback));
}

}  // namespace esphome::esp32_ble

#endif  // USE_ESP32_BLE_ADVERTISING
#endif  // USE_ESP32
