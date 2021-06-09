#include "ble.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

#include <nvs_flash.h>
#include <freertos/FreeRTOSConfig.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <freertos/task.h>
#include <esp_gap_ble_api.h>

namespace esphome {
namespace esp32_ble {

static const char *TAG = "esp32_ble";

void ESP32BLE::setup() {
  global_ble = this;
  ESP_LOGCONFIG(TAG, "Setting up BLE...");

  xTaskCreatePinnedToCore(ESP32BLE::ble_core_task_,
                          "ble_task",  // name
                          10000,       // stack size
                          nullptr,     // input params
                          1,           // priority
                          nullptr,     // handle, not needed
                          0            // core
  );
}

void ESP32BLE::mark_failed() {
  Component::mark_failed();
  if (this->server_ != nullptr) {
    this->server_->mark_failed();
  }
}

bool ESP32BLE::can_proceed() { return this->ready_; }

void ESP32BLE::ble_core_task_(void *params) {
  if (!ble_setup_()) {
    ESP_LOGE(TAG, "BLE could not be set up");
    global_ble->mark_failed();
    return;
  }

  global_ble->ready_ = true;
  ESP_LOGD(TAG, "BLE Setup complete");

  while (true) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

bool ESP32BLE::ble_setup_() {
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %d", err);
    return false;
  }

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
  err = esp_ble_gap_register_callback(ESP32BLE::gap_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", err);
    return false;
  }

  if (global_ble->has_server()) {
    err = esp_ble_gatts_register_callback(ESP32BLE::gatts_event_handler);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gatts_register_callback failed: %d", err);
      return false;
    }
  }

  if (global_ble->has_client()) {
    err = esp_ble_gattc_register_callback(ESP32BLE::gattc_event_handler);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gattc_register_callback failed: %d", err);
      return false;
    }
  }

  err = esp_ble_gap_set_device_name(App.get_name().c_str());
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_set_device_name failed: %d", err);
    return false;
  }

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

// void ESP32BLE::loop() {
//   BLEEvent *ble_event = this->ble_events_.pop();
//   while (ble_event != nullptr) {
//     switch (ble_event->type_) {
//       case ble_event->GATTS:
//         this->real_gatts_event_handler_(ble_event->event_.gatts.gatts_event, ble_event->event_.gatts.gatts_if,
//                                         &ble_event->event_.gatts.gatts_param);
//         break;
//       case ble_event->GAP:
//         this->real_gap_event_handler_(ble_event->event_.gap.gap_event, &ble_event->event_.gap.gap_param);
//         break;
//       default:
//         break;
//     }
//     delete ble_event;
//     ble_event = this->ble_events_.pop();
//   }
// }

void ESP32BLE::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  global_ble->real_gap_event_handler_(event, param);
}

void ESP32BLE::real_gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  ESP_LOGV(TAG, "(BLE) gap_event_handler - %d", event);
  switch (event) {
    default:
      break;
  }
}

void ESP32BLE::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                   esp_ble_gatts_cb_param_t *param) {
  global_ble->real_gatts_event_handler_(event, gatts_if, param);
}

void ESP32BLE::real_gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                         esp_ble_gatts_cb_param_t *param) {
  ESP_LOGV(TAG, "(BLE) gatts_event [esp_gatt_if: %d] - %d", gatts_if, event);
  this->server_->gatts_event_handler(event, gatts_if, param);
}

void ESP32BLE::real_gattc_event_handler_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  // this->client_->gattc_event_handler(event, gattc_if, param);
}

float ESP32BLE::get_setup_priority() const { return setup_priority::BLUETOOTH; }

void ESP32BLE::dump_config() { ESP_LOGCONFIG(TAG, "ESP32 BLE:"); }

ESP32BLE *global_ble = nullptr;

}  // namespace esp32_ble
}  // namespace esphome

#endif
