#include "esp32_ble_beacon.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#ifdef USE_ARDUINO
#include <esp32-hal-bt.h>
#endif

namespace esphome {
namespace esp32_ble_beacon {

static const char *const TAG = "esp32_ble_beacon";

static const esp_ble_ibeacon_head_t IBEACON_COMMON_HEAD = {
    .flags = {0x02, 0x01, 0x06}, .length = 0x1A, .type = 0xFF, .company_id = {0x4C, 0x00}, .beacon_type = {0x02, 0x15}};

void ESP32BLEBeacon::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 BLE Beacon:");
  char uuid[37];
  char *bpos = uuid;
  for (int8_t ii = 0; ii < 16; ++ii) {
    bpos += sprintf(bpos, "%02X", this->uuid_[ii]);
    if (ii == 3 || ii == 5 || ii == 7 || ii == 9) {
      bpos += sprintf(bpos, "-");
    }
  }
  uuid[36] = '\0';
  ESP_LOGCONFIG(TAG,
                "  UUID: %s, Major: %u, Minor: %u, Min Interval: %ums, Max Interval: %ums, Measured Power: %d"
                ", TX Power: %ddBm",
                uuid, this->major_, this->minor_, this->min_interval_, this->max_interval_, this->measured_power_,
                (this->tx_power_ * 3) - 12);
}

float ESP32BLEBeacon::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void ESP32BLEBeacon::setup() {
  this->ble_adv_params_ = {
      .adv_int_min = static_cast<uint16_t>(this->min_interval_ / 0.625f),
      .adv_int_max = static_cast<uint16_t>(this->max_interval_ / 0.625f),
      .adv_type = ADV_TYPE_NONCONN_IND,
      .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .channel_map = ADV_CHNL_ALL,
      .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
  };

  global_ble->advertising_register_raw_advertisement_callback([this](bool advertise) {
    this->advertising_ = advertise;
    if (advertise) {
      this->on_advertise_();
    }
  });
}

void ESP32BLEBeacon::on_advertise_() {
  esp_ble_ibeacon_t ibeacon_adv_data;
  memcpy(&ibeacon_adv_data.ibeacon_head, &IBEACON_COMMON_HEAD, sizeof(esp_ble_ibeacon_head_t));
  memcpy(&ibeacon_adv_data.ibeacon_vendor.proximity_uuid, this->uuid_.data(),
         sizeof(ibeacon_adv_data.ibeacon_vendor.proximity_uuid));
  ibeacon_adv_data.ibeacon_vendor.minor = byteswap(this->minor_);
  ibeacon_adv_data.ibeacon_vendor.major = byteswap(this->major_);
  ibeacon_adv_data.ibeacon_vendor.measured_power = static_cast<uint8_t>(this->measured_power_);

  ESP_LOGD(TAG, "Setting BLE TX power");
  esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, this->tx_power_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_tx_power_set failed: %s", esp_err_to_name(err));
  }
  err = esp_ble_gap_config_adv_data_raw((uint8_t *) &ibeacon_adv_data, sizeof(ibeacon_adv_data));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_config_adv_data_raw failed: %s", esp_err_to_name(err));
    return;
  }
}

void ESP32BLEBeacon::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (!this->advertising_)
    return;

  esp_err_t err;
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      err = esp_ble_gap_start_advertising(&this->ble_adv_params_);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
      }
      break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      err = param->adv_start_cmpl.status;
      if (err != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "BLE adv start failed: %s", esp_err_to_name(err));
      }
      break;
    }
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      err = param->adv_stop_cmpl.status;
      if (err != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "BLE adv stop failed: %s", esp_err_to_name(err));
      } else {
        ESP_LOGD(TAG, "BLE stopped advertising successfully");
      }
      // this->advertising_ = false;
      break;
    }
    default:
      break;
  }
}

}  // namespace esp32_ble_beacon
}  // namespace esphome

#endif
