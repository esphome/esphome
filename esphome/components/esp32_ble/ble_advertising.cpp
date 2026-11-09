#include "ble_advertising.h"

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_ADVERTISING

#include <algorithm>
#include <cstring>
#include "adv_payload.h"
#include "ble.h"
#include "ble_uuid.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifndef CONFIG_BT_CONTROLLER_DISABLED
#include <esp_bt.h>
#endif

namespace esphome::esp32_ble {

ESPHOME_LOG_TAG(TAG, "esp32_ble.advertising");
// One 31 byte packet holds at most 14 16-bit UUIDs
static constexpr size_t MAX_ADVERTISED_UUIDS = 14;

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
  // A priority UUID goes first and alone in the advertising packet
  const bool priority = this->priority_uuid_.is_set();
  AdvPayload::UUID128 uuids[MAX_ADVERTISED_UUIDS + 1];
  size_t num_services = 0;
  if (priority)
    memcpy(uuids[num_services++].data(), this->priority_uuid_.as_128bit().uuid128(), 16);
  for (const auto &uuid : this->advertising_uuids_) {
    if (num_services == std::size(uuids))
      break;
    if (uuid != this->priority_uuid_)  // an unset priority UUID matches nothing
      memcpy(uuids[num_services++].data(), uuid.as_128bit().uuid128(), 16);
  }
  const std::span<const AdvPayload::UUID128> all_uuids(uuids, num_services);
  const std::span<const uint8_t> manufacturer_data(this->manufacturer_data_.data(), this->manufacturer_data_len_);
  const std::span<const uint8_t> service_data(this->service_data_.data(), this->service_data_len_);
  char name_buffer[ESP32BLE::DEVICE_NAME_BUFFER_SIZE];
  const char *name = global_ble->device_name(name_buffer);
  const size_t name_len = strlen(name);

  // Each packet keeps Bluedroid's element order. With a priority UUID, appearance, manufacturer data and the other
  // UUIDs move to the scan response, and the service data stays only in the advertising packet.
  AdvPayload adv;
  AdvPayload rsp;
  AdvPayload &extras = priority ? rsp : adv;
  adv.add_flags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  extras.add_appearance(this->appearance_);
  if (this->include_name_in_adv_)
    adv.add_name(name, name_len);
  rsp.add_name(name, name_len);
  extras.add_manufacturer_data(manufacturer_data);
  rsp.add_tx_power(adv_tx_power_dbm());
  adv.add_uuids(priority ? all_uuids.first(1) : all_uuids);
  rsp.add_uuids(priority ? all_uuids.subspan(1) : all_uuids);
  adv.add_service_data(service_data);
  if (!priority)
    rsp.add_service_data(service_data);

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
  this->restart_pending_ = false;
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
  // The only sender of the services advertising, after the event drain
  if (this->restart_pending_) {
    this->restart_pending_ = false;
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
