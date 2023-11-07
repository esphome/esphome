#ifdef USE_ESP32

#include "ble.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#ifdef USE_ARDUINO
#include <esp32-hal-bt.h>
#endif

namespace esphome {
namespace esp32_ble {

static const char *const TAG = "esp32_ble";

void ESP32BLE::setup() {
  global_ble = this;
  ESP_LOGCONFIG(TAG, "Setting up BLE...");

  if (!ble_pre_setup_()) {
    ESP_LOGE(TAG, "BLE could not be prepared for configuration");
    this->mark_failed();
    return;
  }

  this->state_ = BLE_COMPONENT_STATE_DISABLED;
  if (this->enable_on_boot_) {
    this->enable();
  }
}

void ESP32BLE::enable() {
  if (this->state_ != BLE_COMPONENT_STATE_DISABLED)
    return;

  this->state_ = BLE_COMPONENT_STATE_ENABLE;
}

void ESP32BLE::disable() {
  if (this->state_ == BLE_COMPONENT_STATE_DISABLED)
    return;

  this->state_ = BLE_COMPONENT_STATE_DISABLE;
}

bool ESP32BLE::is_active() { return this->state_ == BLE_COMPONENT_STATE_ACTIVE; }

void ESP32BLE::advertising_start() {
  this->advertising_init_();
  if (!this->is_active())
    return;
  this->advertising_->start();
}

void ESP32BLE::advertising_set_service_data(const std::vector<uint8_t> &data) {
  this->advertising_init_();
  this->advertising_->set_service_data(data);
  this->advertising_start();
}

void ESP32BLE::advertising_set_manufacturer_data(const std::vector<uint8_t> &data) {
  this->advertising_init_();
  this->advertising_->set_manufacturer_data(data);
  this->advertising_start();
}

void ESP32BLE::advertising_add_service_uuid(ESPBTUUID uuid) {
  this->advertising_init_();
  this->advertising_->add_service_uuid(uuid);
  this->advertising_start();
}

void ESP32BLE::advertising_remove_service_uuid(ESPBTUUID uuid) {
  this->advertising_init_();
  this->advertising_->remove_service_uuid(uuid);
  this->advertising_start();
}

bool ESP32BLE::ble_pre_setup_() {
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %d", err);
    return false;
  }
  return true;
}

void ESP32BLE::advertising_init_() {
  if (this->advertising_ != nullptr)
    return;
  this->advertising_ = new BLEAdvertising();  // NOLINT(cppcoreguidelines-owning-memory)

  this->advertising_->set_scan_response(true);
  this->advertising_->set_min_preferred_interval(0x06);
}

bool ESP32BLE::ble_setup_() {
  esp_err_t err;
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

  if (!this->gap_event_handlers_.empty()) {
    err = esp_ble_gap_register_callback(ESP32BLE::gap_event_handler);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", err);
      return false;
    }
  }

  if (!this->gatts_event_handlers_.empty()) {
    err = esp_ble_gatts_register_callback(ESP32BLE::gatts_event_handler);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gatts_register_callback failed: %d", err);
      return false;
    }
  }

  if (!this->gattc_event_handlers_.empty()) {
    err = esp_ble_gattc_register_callback(ESP32BLE::gattc_event_handler);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gattc_register_callback failed: %d", err);
      return false;
    }
  }

  std::string name = App.get_name();
  if (name.length() > 20) {
    if (App.is_name_add_mac_suffix_enabled()) {
      name.erase(name.begin() + 13, name.end() - 7);  // Remove characters between 13 and the mac address
    } else {
      name = name.substr(0, 20);
    }
  }

  err = esp_ble_gap_set_device_name(name.c_str());
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_set_device_name failed: %d", err);
    return false;
  }

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &(this->io_cap_), sizeof(uint8_t));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_set_security_param failed: %d", err);
    return false;
  }

  // BLE takes some time to be fully set up, 200ms should be more than enough
  delay(200);  // NOLINT

  return true;
}

bool ESP32BLE::ble_dismantle_() {
  esp_err_t err = esp_bluedroid_disable();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bluedroid_disable failed: %d", err);
    return false;
  }
  err = esp_bluedroid_deinit();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bluedroid_deinit failed: %d", err);
    return false;
  }

#ifdef USE_ARDUINO
  if (!btStop()) {
    ESP_LOGE(TAG, "btStop failed: %d", esp_bt_controller_get_status());
    return false;
  }
#else
  if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) {
    // stop bt controller
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
      err = esp_bt_controller_disable();
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_disable failed: %s", esp_err_to_name(err));
        return false;
      }
      while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
        ;
    }
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
      err = esp_bt_controller_deinit();
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_deinit failed: %s", esp_err_to_name(err));
        return false;
      }
    }
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) {
      ESP_LOGE(TAG, "esp bt controller disable failed");
      return false;
    }
  }
#endif
  return true;
}

void ESP32BLE::loop() {
  switch (this->state_) {
    case BLE_COMPONENT_STATE_OFF:
    case BLE_COMPONENT_STATE_DISABLED:
      return;
    case BLE_COMPONENT_STATE_DISABLE: {
      ESP_LOGD(TAG, "Disabling BLE...");

      for (auto *ble_event_handler : this->ble_status_event_handlers_) {
        ble_event_handler->ble_before_disabled_event_handler();
      }

      if (!ble_dismantle_()) {
        ESP_LOGE(TAG, "BLE could not be dismantled");
        this->mark_failed();
        return;
      }
      this->state_ = BLE_COMPONENT_STATE_DISABLED;
      return;
    }
    case BLE_COMPONENT_STATE_ENABLE: {
      ESP_LOGD(TAG, "Enabling BLE...");
      this->state_ = BLE_COMPONENT_STATE_OFF;

      if (!ble_setup_()) {
        ESP_LOGE(TAG, "BLE could not be set up");
        this->mark_failed();
        return;
      }

      this->state_ = BLE_COMPONENT_STATE_ACTIVE;
      return;
    }
    case BLE_COMPONENT_STATE_ACTIVE:
      break;
  }

  BLEEvent *ble_event = this->ble_events_.pop();
  while (ble_event != nullptr) {
    switch (ble_event->type_) {
      case BLEEvent::GATTS:
        this->real_gatts_event_handler_(ble_event->event_.gatts.gatts_event, ble_event->event_.gatts.gatts_if,
                                        &ble_event->event_.gatts.gatts_param);
        break;
      case BLEEvent::GATTC:
        this->real_gattc_event_handler_(ble_event->event_.gattc.gattc_event, ble_event->event_.gattc.gattc_if,
                                        &ble_event->event_.gattc.gattc_param);
        break;
      case BLEEvent::GAP:
        this->real_gap_event_handler_(ble_event->event_.gap.gap_event, &ble_event->event_.gap.gap_param);
        break;
      default:
        break;
    }
    delete ble_event;  // NOLINT(cppcoreguidelines-owning-memory)
    ble_event = this->ble_events_.pop();
  }
}

void ESP32BLE::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  BLEEvent *new_event = new BLEEvent(event, param);  // NOLINT(cppcoreguidelines-owning-memory)
  global_ble->ble_events_.push(new_event);
}  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

void ESP32BLE::real_gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  ESP_LOGV(TAG, "(BLE) gap_event_handler - %d", event);
  for (auto *gap_handler : this->gap_event_handlers_) {
    gap_handler->gap_event_handler(event, param);
  }
}

void ESP32BLE::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                   esp_ble_gatts_cb_param_t *param) {
  BLEEvent *new_event = new BLEEvent(event, gatts_if, param);  // NOLINT(cppcoreguidelines-owning-memory)
  global_ble->ble_events_.push(new_event);
}  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

void ESP32BLE::real_gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                         esp_ble_gatts_cb_param_t *param) {
  ESP_LOGV(TAG, "(BLE) gatts_event [esp_gatt_if: %d] - %d", gatts_if, event);
  for (auto *gatts_handler : this->gatts_event_handlers_) {
    gatts_handler->gatts_event_handler(event, gatts_if, param);
  }
}

void ESP32BLE::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) {
  BLEEvent *new_event = new BLEEvent(event, gattc_if, param);  // NOLINT(cppcoreguidelines-owning-memory)
  global_ble->ble_events_.push(new_event);
}  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

void ESP32BLE::real_gattc_event_handler_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  ESP_LOGV(TAG, "(BLE) gattc_event [esp_gatt_if: %d] - %d", gattc_if, event);
  for (auto *gattc_handler : this->gattc_event_handlers_) {
    gattc_handler->gattc_event_handler(event, gattc_if, param);
  }
}

float ESP32BLE::get_setup_priority() const { return setup_priority::BLUETOOTH; }

void ESP32BLE::dump_config() {
  const uint8_t *mac_address = esp_bt_dev_get_address();
  if (mac_address) {
    const char *io_capability_s;
    switch (this->io_cap_) {
      case ESP_IO_CAP_OUT:
        io_capability_s = "display_only";
        break;
      case ESP_IO_CAP_IO:
        io_capability_s = "display_yes_no";
        break;
      case ESP_IO_CAP_IN:
        io_capability_s = "keyboard_only";
        break;
      case ESP_IO_CAP_NONE:
        io_capability_s = "none";
        break;
      case ESP_IO_CAP_KBDISP:
        io_capability_s = "keyboard_display";
        break;
      default:
        io_capability_s = "invalid";
        break;
    }
    ESP_LOGCONFIG(TAG, "ESP32 BLE:");
    ESP_LOGCONFIG(TAG, "  MAC address: %02X:%02X:%02X:%02X:%02X:%02X", mac_address[0], mac_address[1], mac_address[2],
                  mac_address[3], mac_address[4], mac_address[5]);
    ESP_LOGCONFIG(TAG, "  IO Capability: %s", io_capability_s);
  } else {
    ESP_LOGCONFIG(TAG, "ESP32 BLE: bluetooth stack is not enabled");
  }
}

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

ESP32BLE *global_ble = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esp32_ble
}  // namespace esphome

#endif
