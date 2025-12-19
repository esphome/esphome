#include "ble.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#else
extern "C" {
#include <esp_hosted.h>
#include <esp_hosted_misc.h>
#include <esp_hosted_bluedroid.h>
}
#endif
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

namespace esphome::esp32_ble {

static const char *const TAG = "esp32_ble";

// GAP event groups for deduplication across gap_event_handler and dispatch_gap_event_
#define GAP_SCAN_COMPLETE_EVENTS \
  case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: \
  case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: \
  case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT

#define GAP_ADV_COMPLETE_EVENTS \
  case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: \
  case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT: \
  case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: \
  case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: \
  case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT

#define GAP_SECURITY_EVENTS \
  case ESP_GAP_BLE_AUTH_CMPL_EVT: \
  case ESP_GAP_BLE_SEC_REQ_EVT: \
  case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: \
  case ESP_GAP_BLE_PASSKEY_REQ_EVT: \
  case ESP_GAP_BLE_NC_REQ_EVT

void ESP32BLE::setup() {
  global_ble = this;
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

#ifdef USE_ESP32_BLE_ADVERTISING
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

void ESP32BLE::advertising_set_service_data_and_name(std::span<const uint8_t> data, bool include_name) {
  // This method atomically updates both service data and device name inclusion in BLE advertising.
  // When include_name is true, the device name is included in the advertising packet making it
  // visible to passive BLE scanners. When false, the name is only visible in scan response
  // (requires active scanning). This atomic operation ensures we only restart advertising once
  // when changing both properties, avoiding the brief gap that would occur with separate calls.

  this->advertising_init_();

  if (include_name) {
    // When including name, clear service data first to avoid packet overflow
    this->advertising_->set_service_data(std::span<const uint8_t>{});
    this->advertising_->set_include_name(true);
  } else {
    // When including service data, clear name first to avoid packet overflow
    this->advertising_->set_include_name(false);
    this->advertising_->set_service_data(data);
  }

  this->advertising_start();
}

void ESP32BLE::advertising_register_raw_advertisement_callback(std::function<void(bool)> &&callback) {
  this->advertising_init_();
  this->advertising_->register_raw_advertisement_callback(std::move(callback));
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
#endif

bool ESP32BLE::ble_pre_setup_() {
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %d", err);
    return false;
  }
  return true;
}

#ifdef USE_ESP32_BLE_ADVERTISING
void ESP32BLE::advertising_init_() {
  if (this->advertising_ != nullptr)
    return;
  this->advertising_ = new BLEAdvertising(this->advertising_cycle_time_);  // NOLINT(cppcoreguidelines-owning-memory)

  this->advertising_->set_scan_response(true);
  this->advertising_->set_min_preferred_interval(0x06);
  this->advertising_->set_appearance(this->appearance_);
}
#endif

bool ESP32BLE::ble_setup_() {
  esp_err_t err;
#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
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
#else
  esp_hosted_connect_to_slave();  // NOLINT

  if (esp_hosted_bt_controller_init() != ESP_OK) {
    ESP_LOGW(TAG, "esp_hosted_bt_controller_init failed");
    return false;
  }

  if (esp_hosted_bt_controller_enable() != ESP_OK) {
    ESP_LOGW(TAG, "esp_hosted_bt_controller_enable failed");
    return false;
  }

  hosted_hci_bluedroid_open();

  esp_bluedroid_hci_driver_operations_t operations = {
      .send = hosted_hci_bluedroid_send,
      .check_send_available = hosted_hci_bluedroid_check_send_available,
      .register_host_callback = hosted_hci_bluedroid_register_host_callback,
  };
  esp_bluedroid_attach_hci_driver(&operations);
#endif

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

#ifdef ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT
  err = esp_ble_gap_register_callback(ESP32BLE::gap_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", err);
    return false;
  }
#endif

#if defined(USE_ESP32_BLE_SERVER) && defined(ESPHOME_ESP32_BLE_GATTS_EVENT_HANDLER_COUNT)
  err = esp_ble_gatts_register_callback(ESP32BLE::gatts_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_register_callback failed: %d", err);
    return false;
  }
#endif

#if defined(USE_ESP32_BLE_CLIENT) && defined(ESPHOME_ESP32_BLE_GATTC_EVENT_HANDLER_COUNT)
  err = esp_ble_gattc_register_callback(ESP32BLE::gattc_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gattc_register_callback failed: %d", err);
    return false;
  }
#endif

  const char *device_name;
  std::string name_with_suffix;

  if (this->name_ != nullptr) {
    if (App.is_name_add_mac_suffix_enabled()) {
      // MAC address length: 12 hex chars + null terminator
      constexpr size_t mac_address_len = 13;
      // MAC address suffix length (last 6 characters of 12-char MAC address string)
      constexpr size_t mac_address_suffix_len = 6;
      char mac_addr[mac_address_len];
      get_mac_address_into_buffer(mac_addr);
      const char *mac_suffix_ptr = mac_addr + mac_address_suffix_len;
      name_with_suffix =
          make_name_with_suffix(this->name_, strlen(this->name_), '-', mac_suffix_ptr, mac_address_suffix_len);
      device_name = name_with_suffix.c_str();
    } else {
      device_name = this->name_;
    }
  } else {
    name_with_suffix = App.get_name();
    if (name_with_suffix.length() > 20) {
      if (App.is_name_add_mac_suffix_enabled()) {
        // Keep first 13 chars and last 7 chars (MAC suffix), remove middle
        name_with_suffix.erase(13, name_with_suffix.length() - 20);
      } else {
        name_with_suffix.resize(20);
      }
    }
    device_name = name_with_suffix.c_str();
  }

  err = esp_ble_gap_set_device_name(device_name);
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
    // ESP_ERR_INVALID_STATE means Bluedroid is already disabled, which is fine
    if (err != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "esp_bluedroid_disable failed: %d", err);
      return false;
    }
    ESP_LOGD(TAG, "Already disabled");
  }
  err = esp_bluedroid_deinit();
  if (err != ESP_OK) {
    // ESP_ERR_INVALID_STATE means Bluedroid is already deinitialized, which is fine
    if (err != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "esp_bluedroid_deinit failed: %d", err);
      return false;
    }
    ESP_LOGD(TAG, "Already deinitialized");
  }

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
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
#else
  if (esp_hosted_bt_controller_disable() != ESP_OK) {
    ESP_LOGW(TAG, "esp_hosted_bt_controller_disable failed");
    return false;
  }

  if (esp_hosted_bt_controller_deinit(false) != ESP_OK) {
    ESP_LOGW(TAG, "esp_hosted_bt_controller_deinit failed");
    return false;
  }

  hosted_hci_bluedroid_close();
#endif
  return true;
}

void ESP32BLE::loop() {
  switch (this->state_) {
    case BLE_COMPONENT_STATE_OFF:
    case BLE_COMPONENT_STATE_DISABLED:
      return;
    case BLE_COMPONENT_STATE_DISABLE: {
      ESP_LOGD(TAG, "Disabling");

#ifdef ESPHOME_ESP32_BLE_BLE_STATUS_EVENT_HANDLER_COUNT
      for (auto *ble_event_handler : this->ble_status_event_handlers_) {
        ble_event_handler->ble_before_disabled_event_handler();
      }
#endif

      if (!ble_dismantle_()) {
        ESP_LOGE(TAG, "Could not be dismantled");
        this->mark_failed();
        return;
      }
      this->state_ = BLE_COMPONENT_STATE_DISABLED;
      return;
    }
    case BLE_COMPONENT_STATE_ENABLE: {
      ESP_LOGD(TAG, "Enabling");
      this->state_ = BLE_COMPONENT_STATE_OFF;

      if (!ble_setup_()) {
        ESP_LOGE(TAG, "Could not be set up");
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
#if defined(USE_ESP32_BLE_SERVER) && defined(ESPHOME_ESP32_BLE_GATTS_EVENT_HANDLER_COUNT)
      case BLEEvent::GATTS: {
        esp_gatts_cb_event_t event = ble_event->event_.gatts.gatts_event;
        esp_gatt_if_t gatts_if = ble_event->event_.gatts.gatts_if;
        esp_ble_gatts_cb_param_t *param = &ble_event->event_.gatts.gatts_param;
        ESP_LOGV(TAG, "gatts_event [esp_gatt_if: %d] - %d", gatts_if, event);
        for (auto *gatts_handler : this->gatts_event_handlers_) {
          gatts_handler->gatts_event_handler(event, gatts_if, param);
        }
        break;
      }
#endif
#if defined(USE_ESP32_BLE_CLIENT) && defined(ESPHOME_ESP32_BLE_GATTC_EVENT_HANDLER_COUNT)
      case BLEEvent::GATTC: {
        esp_gattc_cb_event_t event = ble_event->event_.gattc.gattc_event;
        esp_gatt_if_t gattc_if = ble_event->event_.gattc.gattc_if;
        esp_ble_gattc_cb_param_t *param = &ble_event->event_.gattc.gattc_param;
        ESP_LOGV(TAG, "gattc_event [esp_gatt_if: %d] - %d", gattc_if, event);
        for (auto *gattc_handler : this->gattc_event_handlers_) {
          gattc_handler->gattc_event_handler(event, gattc_if, param);
        }
        break;
      }
#endif
      case BLEEvent::GAP: {
        esp_gap_ble_cb_event_t gap_event = ble_event->event_.gap.gap_event;
        switch (gap_event) {
          case ESP_GAP_BLE_SCAN_RESULT_EVT:
#ifdef ESPHOME_ESP32_BLE_GAP_SCAN_EVENT_HANDLER_COUNT
            // Use the new scan event handler - no memcpy!
            for (auto *scan_handler : this->gap_scan_event_handlers_) {
              scan_handler->gap_scan_event_handler(ble_event->scan_result());
            }
#endif
            break;

          // Scan complete events
          GAP_SCAN_COMPLETE_EVENTS:
          // Advertising complete events
          GAP_ADV_COMPLETE_EVENTS:
          // RSSI complete event
          case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
          // Security events
          GAP_SECURITY_EVENTS:
            ESP_LOGV(TAG, "gap_event_handler - %d", gap_event);
#ifdef ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT
            {
              esp_ble_gap_cb_param_t *param;
              // clang-format off
              switch (gap_event) {
                // All three scan complete events have the same structure with just status
                // The scan_complete struct matches ESP-IDF's layout exactly, so this reinterpret_cast is safe
                // This is verified at compile-time by static_assert checks in ble_event.h
                // The struct already contains our copy of the status (copied in BLEEvent constructor)
                GAP_SCAN_COMPLETE_EVENTS:
                  param = reinterpret_cast<esp_ble_gap_cb_param_t *>(&ble_event->event_.gap.scan_complete);
                  break;

                // All advertising complete events have the same structure with just status
                GAP_ADV_COMPLETE_EVENTS:
                  param = reinterpret_cast<esp_ble_gap_cb_param_t *>(&ble_event->event_.gap.adv_complete);
                  break;

                case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
                  param = reinterpret_cast<esp_ble_gap_cb_param_t *>(&ble_event->event_.gap.read_rssi_complete);
                  break;

                GAP_SECURITY_EVENTS:
                  param = reinterpret_cast<esp_ble_gap_cb_param_t *>(&ble_event->event_.gap.security);
                  break;

                default:
                  break;
              }
              // clang-format on
              // Dispatch to all registered handlers
              for (auto *gap_handler : this->gap_event_handlers_) {
                gap_handler->gap_event_handler(gap_event, param);
              }
            }
#endif
            break;

          default:
            // Unknown/unhandled event
            ESP_LOGW(TAG, "Unhandled GAP event type in loop: %d", gap_event);
            break;
        }
        break;
      }
      default:
        break;
    }
    // Return the event to the pool
    this->ble_event_pool_.release(ble_event);
    ble_event = this->ble_events_.pop();
  }
#ifdef USE_ESP32_BLE_ADVERTISING
  if (this->advertising_ != nullptr) {
    this->advertising_->loop();
  }
#endif

  // Log dropped events periodically
  uint16_t dropped = this->ble_events_.get_and_reset_dropped_count();
  if (dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u BLE events due to buffer overflow", dropped);
  }
}

// Helper function to load new event data based on type
void load_ble_event(BLEEvent *event, esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
  event->load_gap_event(e, p);
}

#ifdef USE_ESP32_BLE_CLIENT
void load_ble_event(BLEEvent *event, esp_gattc_cb_event_t e, esp_gatt_if_t i, esp_ble_gattc_cb_param_t *p) {
  event->load_gattc_event(e, i, p);
}
#endif

#ifdef USE_ESP32_BLE_SERVER
void load_ble_event(BLEEvent *event, esp_gatts_cb_event_t e, esp_gatt_if_t i, esp_ble_gatts_cb_param_t *p) {
  event->load_gatts_event(e, i, p);
}
#endif

template<typename... Args> void enqueue_ble_event(Args... args) {
  // Allocate an event from the pool
  BLEEvent *event = global_ble->ble_event_pool_.allocate();
  if (event == nullptr) {
    // No events available - queue is full or we're out of memory
    global_ble->ble_events_.increment_dropped_count();
    return;
  }

  // Load new event data (replaces previous event)
  load_ble_event(event, args...);

  // Push the event to the queue
  global_ble->ble_events_.push(event);
  // Push always succeeds because we're the only producer and the pool ensures we never exceed queue size
}

// Explicit template instantiations for the friend function
template void enqueue_ble_event(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t *);
#ifdef USE_ESP32_BLE_SERVER
template void enqueue_ble_event(esp_gatts_cb_event_t, esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
#endif
#ifdef USE_ESP32_BLE_CLIENT
template void enqueue_ble_event(esp_gattc_cb_event_t, esp_gatt_if_t, esp_ble_gattc_cb_param_t *);
#endif

void ESP32BLE::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    // Queue GAP events that components need to handle
    // Scanning events - used by esp32_ble_tracker
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    GAP_SCAN_COMPLETE_EVENTS:
    // Advertising events - used by esp32_ble_beacon and esp32_ble server
    GAP_ADV_COMPLETE_EVENTS:
    // Connection events - used by ble_client
    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
      enqueue_ble_event(event, param);
      return;

    // Security events - used by ble_client and bluetooth_proxy
    // These are rare but interactive (pairing/bonding), so notify immediately
    GAP_SECURITY_EVENTS:
      enqueue_ble_event(event, param);
      // Wake up main loop to process security event immediately
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
      App.wake_loop_threadsafe();
#endif
      return;

    // Ignore these GAP events as they are not relevant for our use case
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
    case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT:       // BLE 5.0 PHY update complete
    case ESP_GAP_BLE_CHANNEL_SELECT_ALGORITHM_EVT:  // BLE 5.0 channel selection algorithm
      return;

    default:
      break;
  }
  ESP_LOGW(TAG, "Ignoring unexpected GAP event type: %d", event);
}

#ifdef USE_ESP32_BLE_SERVER
void ESP32BLE::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                   esp_ble_gatts_cb_param_t *param) {
  enqueue_ble_event(event, gatts_if, param);
  // Wake up main loop to process GATT event immediately
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
  App.wake_loop_threadsafe();
#endif
}
#endif

#ifdef USE_ESP32_BLE_CLIENT
void ESP32BLE::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) {
  enqueue_ble_event(event, gattc_if, param);
  // Wake up main loop to process GATT event immediately
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
  App.wake_loop_threadsafe();
#endif
}
#endif

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
    char mac_s[18];
    format_mac_addr_upper(mac_address, mac_s);
    ESP_LOGCONFIG(TAG,
                  "BLE:\n"
                  "  MAC address: %s\n"
                  "  IO Capability: %s",
                  mac_s, io_capability_s);
  } else {
    ESP_LOGCONFIG(TAG, "Bluetooth stack is not enabled");
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

}  // namespace esphome::esp32_ble

#endif
