#include "ble_advertising.h"

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_ADVERTISING

#include <cstdio>
#include <algorithm>
#include <cstring>
#include "ble_uuid.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::esp32_ble {

ESPHOME_LOG_TAG(TAG, "esp32_ble.advertising");
// Give up on a completion event after this long, so a lost event cannot stop advertising updates for good
static constexpr uint32_t CONFIG_COMPLETE_TIMEOUT_MS = 1000;
// A 31 byte packet fits at most 14 16-bit UUIDs (2 byte header + 2 bytes each); more can never be sent
static constexpr size_t MAX_ADVERTISED_UUIDS = 14;

BLEAdvertising::BLEAdvertising(uint32_t advertising_cycle_time) : advertising_cycle_time_(advertising_cycle_time) {
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

void BLEAdvertising::set_service_data(std::span<const uint8_t> data) {
  delete[] this->advertising_data_.p_service_data;
  this->advertising_data_.p_service_data = nullptr;
  this->advertising_data_.service_data_len = data.size();
  if (!data.empty()) {
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    this->advertising_data_.p_service_data = new uint8_t[data.size()];
    memcpy(this->advertising_data_.p_service_data, data.data(), data.size());
  }
}

void BLEAdvertising::set_service_data(const std::vector<uint8_t> &data) {
  this->set_service_data(std::span<const uint8_t>(data));
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

bool BLEAdvertising::config_pending_() const {
  return this->configs_pending_ != 0 &&
         App.get_loop_component_start_time() - this->config_sent_time_ < CONFIG_COMPLETE_TIMEOUT_MS;
}

void BLEAdvertising::reset_config_state() {
  this->configs_pending_ = 0;
  this->restart_pending_ = false;
}

void BLEAdvertising::on_config_complete() {
  if (this->configs_pending_ != 0)
    this->configs_pending_--;
}

esp_err_t BLEAdvertising::services_advertisement_() {
  if (this->config_pending_()) {
    // Applied once the stack has taken the previous config
    this->restart_pending_ = true;
    return ESP_OK;
  }
  if (this->configs_pending_ != 0) {
    // Completion events carry no id: one that is only late, not lost, can still count down the new configs
    ESP_LOGW(TAG, "Advertising config not confirmed within %u ms", (unsigned) CONFIG_COMPLETE_TIMEOUT_MS);
    this->configs_pending_ = 0;
  }
  this->config_sent_time_ = App.get_loop_component_start_time();

  // The stack copies the UUIDs during each config call, so a buffer on the stack is enough. A priority UUID goes
  // first and alone in the advertising packet, which it fills exactly with the service data; the rest go to the
  // scan response.
  const bool priority = this->priority_uuid_.is_set();
  const size_t max_services = MAX_ADVERTISED_UUIDS + (priority ? 1 : 0);
  uint8_t uuids[16 * (MAX_ADVERTISED_UUIDS + 1)];
  size_t num_services = 0;
  if (priority)
    memcpy(uuids + 16 * num_services++, this->priority_uuid_.as_128bit().get_uuid().uuid.uuid128, 16);
  for (const auto &uuid : this->advertising_uuids_) {
    if (num_services == max_services)
      break;
    if (uuid != this->priority_uuid_)  // an unset priority UUID matches nothing
      memcpy(uuids + 16 * num_services++, uuid.as_128bit().get_uuid().uuid.uuid128, 16);
  }
  this->advertising_data_.service_uuid_len = 16 * (priority ? 1 : num_services);
  this->advertising_data_.p_service_uuid = num_services > 0 ? uuids : nullptr;

  esp_err_t err;

  this->advertising_data_.set_scan_rsp = false;
  this->advertising_data_.include_name = this->include_name_in_adv_ || !this->scan_response_;
  this->advertising_data_.include_txpower = !this->scan_response_;
  // A priority UUID leaves no room for these in the advertising packet; they go to the scan response instead
  const uint16_t manufacturer_len = this->advertising_data_.manufacturer_len;
  const uint16_t appearance = this->advertising_data_.appearance;
  if (priority) {
    this->advertising_data_.manufacturer_len = 0;
    this->advertising_data_.appearance = 0;
  }
  err = esp_ble_gap_config_adv_data(&this->advertising_data_);
  this->advertising_data_.manufacturer_len = manufacturer_len;
  this->advertising_data_.appearance = appearance;
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_config_adv_data failed (Advertising): %s", esp_err_to_name(err));
    return err;
  }
  this->configs_pending_++;

  if (this->scan_response_) {
    memcpy(&this->scan_response_data_, &this->advertising_data_, sizeof(esp_ble_adv_data_t));
    this->scan_response_data_.set_scan_rsp = true;
    this->scan_response_data_.include_name = true;
    this->scan_response_data_.include_txpower = true;
    this->scan_response_data_.flag = 0;
    if (priority) {
      // The other service UUIDs, after the priority one; the service data is already in the advertising packet
      this->scan_response_data_.service_uuid_len = 16 * (num_services - 1);
      this->scan_response_data_.p_service_uuid = num_services > 1 ? uuids + 16 : nullptr;
      this->scan_response_data_.service_data_len = 0;
    } else {
      this->scan_response_data_.manufacturer_len = 0;
      this->scan_response_data_.appearance = 0;
    }
    err = esp_ble_gap_config_adv_data(&this->scan_response_data_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gap_config_adv_data failed (Scan response): %s", esp_err_to_name(err));
      return err;
    }
    this->configs_pending_++;
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
  // A restart deferred while configs were in flight
  if (this->restart_pending_ && !this->config_pending_()) {
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
