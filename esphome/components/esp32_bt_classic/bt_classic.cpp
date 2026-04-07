#ifdef USE_ESP32

#include "bt_classic.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/task.h>
#include <nvs_flash.h>

// For time getting:
#include "esphome/components/homeassistant/time/homeassistant_time.h"

#ifdef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#error Not compatible with ESP_HOSTED
#endif
namespace esphome {
namespace esp32_bt_classic {

#ifdef USE_BUTTON
void ResetBtStackButton::press_action() { this->parent_->reset_bt_stack(); }
#endif

float ESP32BtClassic::get_setup_priority() const {
  // Setup just after BLE, (but before AFTER_BLUETOOTH) to ensure both can co-exist!
  return setup_priority::BLUETOOTH + 5.0f;
}

void ESP32BtClassic::setup() {
  global_bt_classic = this;
  ESP_LOGCONFIG(TAG, "Setting up BT Classic...");

  if (!bt_pre_setup_()) {
    ESP_LOGE(TAG, "BT Classic could not be prepared for configuration");
    this->mark_failed();
    return;
  }

  if (this->enable_on_boot_) {
    this->enable();
  }

  ESP_LOGD(TAG, "BT Classic setup complete");
}

void ESP32BtClassic::enable() {
  ESP_LOGD(TAG, "Enabling");
  if (!bt_setup_()) {
    ESP_LOGE(TAG, "BT Classic could not be set up");
    this->mark_failed();
#ifdef USE_TEXT_SENSOR
    if (last_error_sensor_) {
      last_error_sensor_->publish_state("setup");
    }
#endif
    return;
  }
}
void ESP32BtClassic::disable() {
  ESP_LOGD(TAG, "Disabling");
  if (!bt_dismantle_()) {
    ESP_LOGE(TAG, "Could not be dismantled");
    this->mark_failed();
#ifdef USE_TEXT_SENSOR
    if (last_error_sensor_) {
      last_error_sensor_->publish_state("dismantle");
    }
#endif
    return;
  }
}

bool ESP32BtClassic::bt_pre_setup_() {
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %d", err);
    return false;
  }
  return true;
}

bool ESP32BtClassic::bt_setup_() {
  esp_err_t err;
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
      err = esp_bt_controller_enable(BT_CONTROLLER_MODE);
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

  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    ESP_LOGD(TAG, "Initializing BlueDroid");
    if ((err = esp_bluedroid_init()) != ESP_OK) {
      ESP_LOGE(TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(err));
      return false;
    }
  } else {
    ESP_LOGD(TAG, "BlueDroid Already Initialized!");
  }

  if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    ESP_LOGD(TAG, "Enabling BlueDroid");
    if ((err = esp_bluedroid_enable()) != ESP_OK) {
      ESP_LOGE(TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(err));
      return false;
    }
  } else {
    ESP_LOGD(TAG, "BlueDroid Already Enabled!");
  }

  bool success = gap_startup();

  // BT takes some time to be fully set up, 200ms should be more than enough
  for (int i = 10; i < 20; i++) {
    App.feed_wdt();
    delay(10);  // NOLINT
  }

  return success;
}

bool ESP32BtClassic::bt_dismantle_() {
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
  return true;
}

bool ESP32BtClassic::gap_startup() {
  ESP_LOGD(TAG, "Startup GAP");
  // set discoverable and connectable mode, wait to be connected
  esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

  // register GAP callback function
  esp_err_t err = esp_bt_gap_register_callback(ESP32BtClassic::gap_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bt_gap_register_callback failed: %s", esp_err_to_name(err));
    return false;
  }

  return true;
}

void ESP32BtClassic::reset_bt_stack() {
  ESP_LOGW(TAG, "Resetting BT stack!");

#if 1
  ESP_LOGD(TAG, "BlueDroid status: %d", esp_bluedroid_get_status());
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
    ESP_LOGD(TAG, "Disable BlueDroid");
    auto resp = esp_bluedroid_disable();
    ESP_LOGI(TAG, "BlueDroid Disable: %s", esp_err_to_name(resp));
  }
  if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    ESP_LOGD(TAG, "De-init BlueDroid");
    auto resp = esp_bluedroid_deinit();
    ESP_LOGI(TAG, "BlueDroid De-init: %s", esp_err_to_name(resp));
  }
  delay(250);
#endif

  ESP_LOGD(TAG, "BT Controller status: %d", esp_bt_controller_get_status());
  esp_bt_controller_disable();
  ESP_LOGD(TAG, "De-init BT Controller");
  esp_bt_controller_deinit();

  scanPending_ = false;
  active_scan_list_.clear();

  delay(500);
  ESP_LOGW(TAG, "Re-Setup BT stack;");

  bt_setup_();
}

void ESP32BtClassic::loop() {
  // Handle GAP event queue
  BtGapEvent *bt_event = bt_events_.pop();
  while (bt_event != nullptr) {
    real_gap_event_handler_(bt_event->event, &(bt_event->param));
    delete bt_event;  // NOLINT(cppcoreguidelines-owning-memory)
    bt_event = bt_events_.pop();
  }

  // Process scanning queue
  if (!active_scan_list_.empty() && (millis() + scan_delay_) > last_scan_ms && !scanPending_) {
    if (active_scan_list_.front().scans_remaining > 0) {
      startScan(active_scan_list_.front().address);
      active_scan_list_.front().scans_remaining--;
    } else {
      active_scan_list_.erase(active_scan_list_.begin());
    }
  }
}

void ESP32BtClassic::startScan(const uint64_t u64_addr) {
  esp_bd_addr_t bd_addr;
  uint64_to_bd_addr(u64_addr, bd_addr);
  ESP_LOGD(TAG, "Start scanning for %02X:%02X:%02X:%02X:%02X:%02X", EXPAND_MAC_F(bd_addr));
  esp_err_t result = esp_bt_gap_read_remote_name(bd_addr);

  if (result == ESP_OK) {
    scanPending_ = true;
    last_scan_ms = millis();
    for (auto *listener : scan_start_listners_) {
      listener->on_scan_start();
    }
  } else {
    ESP_LOGE(TAG, "Could not start scan! Error: %s\n  BlueDroid status: %d\n  Controller status: %d",
             esp_err_to_name(result), esp_bluedroid_get_status(), esp_bt_controller_get_status());
  }
}

void ESP32BtClassic::addScan(const bt_scan_item &scan) {
  // Ensure active scan list only contains unique MAC addresses
  auto element = std::find_if(active_scan_list_.begin(), active_scan_list_.end(),
                              [&scan](const bt_scan_item &i) { return i.address == scan.address; });
  if (element != active_scan_list_.end()) {
    // Device already in active scan list; increase scans_remaining
    element->scans_remaining += scan.scans_remaining;
    ESP_LOGV(TAG, "Found %s already in active scan list. Increased scans remaining to %d",
             u64_addr_to_str(scan.address).c_str(), element->scans_remaining);
  } else {
    active_scan_list_.push_back(scan);
    ESP_LOGV(TAG, "Added %s to active scan list with %d scans remaining", u64_addr_to_str(scan.address).c_str(),
             scan.scans_remaining);
  }
}

void ESP32BtClassic::addScan(const std::vector<bt_scan_item> &scan_list) {
  for (const auto &scan : scan_list) {
    addScan(scan);
  }
}

void ESP32BtClassic::gap_event_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  BtGapEvent *new_event = new BtGapEvent(event, param);  // NOLINT(cppcoreguidelines-owning-memory)
  global_bt_classic->bt_events_.push(new_event);
}  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

void ESP32BtClassic::real_gap_event_handler_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  ESP_LOGV(TAG, "(BT) gap_event_handler - %d", event);

  switch (event) {
    case ESP_BT_GAP_READ_REMOTE_NAME_EVT: {
      const auto &scan_item = handle_scan_result(param->read_rmt_name);

      for (auto *listener : scan_result_listners_) {
        listener->on_scan_result(param->read_rmt_name, scan_item);
      }

      break;
    }
    default: {
      ESP_LOGD(TAG, "event: %d", event);
      break;
    }
  }
}

optional<bt_scan_item> ESP32BtClassic::handle_scan_result(const rmt_name_result &result) {
  scanPending_ = false;
  uint32_t scanDuration = millis() - last_scan_ms;
  const uint64_t u64_addr = bd_addr_to_uint64(result.bda);

  optional<bt_scan_item> active_scan_item{};
  auto it = active_scan_list_.begin();
  while (it != active_scan_list_.end()) {
    if (it->address == u64_addr) {
      // copy scan_item to return value before modifications!
      active_scan_item = *it;
      active_scan_item->scan_duration = scanDuration;

      // If device was found, remove it from the scan list
      if (ESP_BT_STATUS_SUCCESS == result.stat) {
        ESP_LOGI(TAG, "Found device '%02X:%02X:%02X:%02X:%02X:%02X' (%s) in %lu ms with %d scans remaining",
                 EXPAND_MAC_F(result.bda), result.rmt_name, scanDuration, it->scans_remaining);
        active_scan_list_.erase(it);
      } else {
        it->next_scan_time = millis() + scan_delay_;

        ESP_LOGD(TAG, "Device '%02X:%02X:%02X:%02X:%02X:%02X' scan result: %s (%d) in %lu ms", EXPAND_MAC_F(result.bda),
                 esp_bt_status_to_str(result.stat), result.stat, scanDuration);
        ESP_LOGD(TAG, "BlueDroid status: %d\n  Controller status: %d", esp_bluedroid_get_status(),
                 esp_bt_controller_get_status());

        if (it->scans_remaining == 0) {
          ESP_LOGW(TAG, "Device '%02X:%02X:%02X:%02X:%02X:%02X' not found on final scan. Removing from scan list.",
                   EXPAND_MAC_F(result.bda));
          active_scan_list_.erase(it);
        } else {
          ESP_LOGD(TAG, "Device '%02X:%02X:%02X:%02X:%02X:%02X' not found. %d scans remaining",
                   EXPAND_MAC_F(result.bda), it->scans_remaining);
          // Put device at end of the scan queue
          if (active_scan_list_.size() > 1) {
            moveItemToBack(active_scan_list_, it - active_scan_list_.begin());
          }
        }
      }
      break;
    }
    it++;
  }

#ifdef USE_TEXT_SENSOR
  if (last_error_sensor_ && result.stat == ESP_BT_STATUS_FAIL && scanDuration < 100) {
    auto current_time = esphome::homeassistant::global_homeassistant_time->now();
    last_error_sensor_->publish_state(current_time.strftime("%Y-%m-%d %H:%M:%S"));
  }
#endif

  if (active_scan_list_.empty()) {
    ESP_LOGD(TAG, "Scan complete. No more devices left to scan.");
  }
  return active_scan_item;
}

void ESP32BtClassic::dump_config() {
  const uint8_t *mac_address = esp_bt_dev_get_address();
  if (mac_address) {
    ESP_LOGCONFIG(TAG, "ESP32 BT Classic:");
    ESP_LOGCONFIG(TAG, "  MAC address: %02X:%02X:%02X:%02X:%02X:%02X", EXPAND_MAC_F(mac_address));
    ESP_LOGCONFIG(TAG, "  %d registered Scan Start Listners", scan_start_listners_.size());
    ESP_LOGCONFIG(TAG, "  %d registered Scan Result Listners", scan_result_listners_.size());
  } else {
    ESP_LOGCONFIG(TAG, "ESP32 BT: bluetooth stack is not enabled");
  }
}

ESP32BtClassic *global_bt_classic = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esp32_bt_classic
}  // namespace esphome

#endif
