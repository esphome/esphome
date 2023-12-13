#include "esp32_ble_beacon.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <freertos/task.h>
#include <esp_gap_ble_api.h>
#include <cstring>
#include "esphome/core/hal.h"

#ifdef USE_ARDUINO
#include <esp32-hal-bt.h>
#endif

namespace esphome {
namespace esp32_ble_beacon {

static const char *const TAG = "esp32_ble_beacon";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#define ENDIAN_CHANGE_U16(x) ((((x) &0xFF00) >> 8) + (((x) &0xFF) << 8))

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
                this->tx_power_);
}

void ESP32BLEBeacon::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 BLE beacon...");
  global_esp32_ble_beacon = this;

  xTaskCreatePinnedToCore(ESP32BLEBeacon::ble_core_task,
                          "ble_task",  // name
                          10000,       // stack size (in words)
                          nullptr,     // input params
                          1,           // priority
                          nullptr,     // Handle, not needed
                          0            // core
  );
}

float ESP32BLEBeacon::get_setup_priority() const { return setup_priority::BLUETOOTH; }
void ESP32BLEBeacon::ble_core_task(void *params) {
  ble_setup();

  while (true) {
    delay(1000);  // NOLINT
  }
}

void ESP32BLEBeacon::ble_setup() {
  ble_adv_params.adv_int_min = static_cast<uint16_t>(global_esp32_ble_beacon->min_interval_ / 0.625f);
  ble_adv_params.adv_int_max = static_cast<uint16_t>(global_esp32_ble_beacon->max_interval_ / 0.625f);

  // Initialize non-volatile storage for the bluetooth controller
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %d", err);
    return;
  }

#ifdef USE_ARDUINO
  if (!btStart()) {
    ESP_LOGE(TAG, "btStart failed: %d", esp_bt_controller_get_status());
    return;
  }
#else
  if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
    // start bt controller
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
      esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
      err = esp_bt_controller_init(&cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %s", esp_err_to_name(err));
        return;
      }
      while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE)
        ;
    }
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
      err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %s", esp_err_to_name(err));
        return;
      }
    }
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
      ESP_LOGE(TAG, "esp bt controller enable failed");
      return;
    }
  }
#endif

  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  err = esp_bluedroid_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", err);
    return;
  }
  err = esp_bluedroid_enable();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", err);
    return;
  }
  err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV,
                             static_cast<esp_power_level_t>((global_esp32_ble_beacon->tx_power_ + 12) / 3));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_tx_power_set failed: %s", esp_err_to_name(err));
    return;
  }
  err = esp_ble_gap_register_callback(ESP32BLEBeacon::gap_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", err);
    return;
  }

  esp_ble_ibeacon_t ibeacon_adv_data;
  memcpy(&ibeacon_adv_data.ibeacon_head, &IBEACON_COMMON_HEAD, sizeof(esp_ble_ibeacon_head_t));
  memcpy(&ibeacon_adv_data.ibeacon_vendor.proximity_uuid, global_esp32_ble_beacon->uuid_.data(),
         sizeof(ibeacon_adv_data.ibeacon_vendor.proximity_uuid));
  ibeacon_adv_data.ibeacon_vendor.minor = ENDIAN_CHANGE_U16(global_esp32_ble_beacon->minor_);
  ibeacon_adv_data.ibeacon_vendor.major = ENDIAN_CHANGE_U16(global_esp32_ble_beacon->major_);
  ibeacon_adv_data.ibeacon_vendor.measured_power = static_cast<uint8_t>(global_esp32_ble_beacon->measured_power_);

  esp_ble_gap_config_adv_data_raw((uint8_t *) &ibeacon_adv_data, sizeof(ibeacon_adv_data));
}

void ESP32BLEBeacon::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  esp_err_t err;
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      err = esp_ble_gap_start_advertising(&ble_adv_params);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed: %d", err);
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
      break;
    }
    default:
      break;
  }
}

ESP32BLEBeacon *global_esp32_ble_beacon = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esp32_ble_beacon
}  // namespace esphome

#endif
