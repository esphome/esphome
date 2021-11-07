#pragma once

#include "ble_advertising.h"

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "queue.h"

#ifdef USE_ESP32_BLE_SERVER
#include "esphome/components/esp32_ble_server/ble_server.h"
#endif

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_gattc_api.h>
namespace esphome {
namespace esp32_ble {

// NOLINTNEXTLINE(modernize-use-using)
typedef struct {
  void *peer_device;
  bool connected;
  uint16_t mtu;
} conn_status_t;

class ESP32BLE : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void mark_failed() override;

  bool has_server() {
#ifdef USE_ESP32_BLE_SERVER
    return this->server_ != nullptr;
#else
    return false;
#endif
  }
  bool has_client() { return false; }

  BLEAdvertising *get_advertising() { return this->advertising_; }

#ifdef USE_ESP32_BLE_SERVER
  void set_server(esp32_ble_server::BLEServer *server) { this->server_ = server; }
#endif
 protected:
  static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  void real_gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  void real_gattc_event_handler_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  void real_gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  bool ble_setup_();

#ifdef USE_ESP32_BLE_SERVER
  esp32_ble_server::BLEServer *server_{nullptr};
#endif
  Queue<BLEEvent> ble_events_;
  BLEAdvertising *advertising_;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32BLE *global_ble;

}  // namespace esp32_ble
}  // namespace esphome

#endif
