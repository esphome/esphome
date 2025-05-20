#pragma once

#include "ble_advertising.h"
#include "ble_uuid.h"

#include <functional>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "ble_event.h"
#include "queue.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble {

uint64_t ble_addr_to_uint64(const esp_bd_addr_t address);

// NOLINTNEXTLINE(modernize-use-using)
typedef struct {
  void *peer_device;
  bool connected;
  uint16_t mtu;
} conn_status_t;

enum IoCapability {
  IO_CAP_OUT = ESP_IO_CAP_OUT,
  IO_CAP_IO = ESP_IO_CAP_IO,
  IO_CAP_IN = ESP_IO_CAP_IN,
  IO_CAP_NONE = ESP_IO_CAP_NONE,
  IO_CAP_KBDISP = ESP_IO_CAP_KBDISP,
};

enum BLEComponentState {
  /** Nothing has been initialized yet. */
  BLE_COMPONENT_STATE_OFF = 0,
  /** BLE should be disabled on next loop. */
  BLE_COMPONENT_STATE_DISABLE,
  /** BLE is disabled. */
  BLE_COMPONENT_STATE_DISABLED,
  /** BLE should be enabled on next loop. */
  BLE_COMPONENT_STATE_ENABLE,
  /** BLE is active. */
  BLE_COMPONENT_STATE_ACTIVE,
};

class GAPEventHandler {
 public:
  virtual void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) = 0;
};

class GATTcEventHandler {
 public:
  virtual void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) = 0;
};

class GATTsEventHandler {
 public:
  virtual void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                   esp_ble_gatts_cb_param_t *param) = 0;
};

class BLEStatusEventHandler {
 public:
  virtual void ble_before_disabled_event_handler() = 0;
};

class ESP32BLE : public Component {
 public:
  void set_io_capability(IoCapability io_capability) { this->io_cap_ = (esp_ble_io_cap_t) io_capability; }

  void set_advertising_cycle_time(uint32_t advertising_cycle_time) {
    this->advertising_cycle_time_ = advertising_cycle_time;
  }
  uint32_t get_advertising_cycle_time() const { return this->advertising_cycle_time_; }

  void enable();
  void disable();
  bool is_active();
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_name(const std::string &name) { this->name_ = name; }

  void advertising_start();
  void advertising_set_service_data(const std::vector<uint8_t> &data);
  void advertising_set_manufacturer_data(const std::vector<uint8_t> &data);
  void advertising_set_appearance(uint16_t appearance) { this->appearance_ = appearance; }
  void advertising_add_service_uuid(ESPBTUUID uuid);
  void advertising_remove_service_uuid(ESPBTUUID uuid);
  void advertising_register_raw_advertisement_callback(std::function<void(bool)> &&callback);

  void register_gap_event_handler(GAPEventHandler *handler) { this->gap_event_handlers_.push_back(handler); }
  void register_gattc_event_handler(GATTcEventHandler *handler) { this->gattc_event_handlers_.push_back(handler); }
  void register_gatts_event_handler(GATTsEventHandler *handler) { this->gatts_event_handlers_.push_back(handler); }
  void register_ble_status_event_handler(BLEStatusEventHandler *handler) {
    this->ble_status_event_handlers_.push_back(handler);
  }
  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

 protected:
  static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  void real_gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  void real_gattc_event_handler_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  void real_gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  bool ble_setup_();
  bool ble_dismantle_();
  bool ble_pre_setup_();
  void advertising_init_();

  std::vector<GAPEventHandler *> gap_event_handlers_;
  std::vector<GATTcEventHandler *> gattc_event_handlers_;
  std::vector<GATTsEventHandler *> gatts_event_handlers_;
  std::vector<BLEStatusEventHandler *> ble_status_event_handlers_;
  BLEComponentState state_{BLE_COMPONENT_STATE_OFF};

  Queue<BLEEvent> ble_events_;
  BLEAdvertising *advertising_{};
  esp_ble_io_cap_t io_cap_{ESP_IO_CAP_NONE};
  uint32_t advertising_cycle_time_{};
  bool enable_on_boot_{};
  optional<std::string> name_;
  uint16_t appearance_{0};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32BLE *global_ble;

template<typename... Ts> class BLEEnabledCondition : public Condition<Ts...> {
 public:
  bool check(Ts... x) override { return global_ble->is_active(); }
};

template<typename... Ts> class BLEEnableAction : public Action<Ts...> {
 public:
  void play(Ts... x) override { global_ble->enable(); }
};

template<typename... Ts> class BLEDisableAction : public Action<Ts...> {
 public:
  void play(Ts... x) override { global_ble->disable(); }
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
