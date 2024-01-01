#include "ble_rssi_sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_client {

static const char *const TAG = "ble_rssi_sensor";

void BLEClientRSSISensor::loop() {}

void BLEClientRSSISensor::dump_config() {
  LOG_SENSOR("", "BLE Client RSSI Sensor", this);
  ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent()->address_str().c_str());
  LOG_UPDATE_INTERVAL(this);
}

void BLEClientRSSISensor::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_CLOSE_EVT: {
      this->status_set_warning();
      this->publish_state(NAN);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;
      if (this->should_update_) {
        this->should_update_ = false;
        this->get_rssi_();
      }
      break;
    }
    default:
      break;
  }
}

void BLEClientRSSISensor::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    // server response on RSSI request:
    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
      if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
        int8_t rssi = param->read_rssi_cmpl.rssi;
        ESP_LOGI(TAG, "ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT RSSI: %d", rssi);
        this->status_clear_warning();
        this->publish_state(rssi);
      }
      break;
    default:
      break;
  }
}

void BLEClientRSSISensor::update() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Cannot poll, not connected", this->get_name().c_str());
    this->should_update_ = true;
    return;
  }
  this->get_rssi_();
}
void BLEClientRSSISensor::get_rssi_() {
  ESP_LOGV(TAG, "requesting rssi from %s", this->parent()->address_str().c_str());
  auto status = esp_ble_gap_read_rssi(this->parent()->get_remote_bda());
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gap_read_rssi error, address=%s, status=%d", this->parent()->address_str().c_str(), status);
    this->status_set_warning();
    this->publish_state(NAN);
  }
}

}  // namespace ble_client
}  // namespace esphome
#endif
