#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "ble_server.h"
#include "queue.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_gattc_api.h>

namespace esphome {
namespace esp32_ble {

class ESP32BLE : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  bool has_server() { return this->server_ != nullptr; }
  bool has_client() { return false; }

  void set_server(BLEServer *server) { this->server_ = server; }

 protected:
  static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  void real_gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  void real_gattc_event_handler_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  void real_gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  static bool ble_setup_();

  BLEServer *server_{nullptr};
  Queue<BLEEvent> ble_events_;
};

extern ESP32BLE *global_ble;

}  // namespace esp32_ble
}  // namespace esphome

#endif
