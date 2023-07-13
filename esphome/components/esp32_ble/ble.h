#pragma once

#include "ble_advertising.h"

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "queue.h"
#include "ble_event.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_gattc_api.h>

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

class ESP32BLE : public Component {
 public:
  void set_io_capability(IoCapability io_capability) { this->io_cap_ = (esp_ble_io_cap_t) io_capability; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  BLEAdvertising *get_advertising() { return this->advertising_; }

  void register_gap_event_handler(GAPEventHandler *handler) { this->gap_event_handlers_.push_back(handler); }
  void register_gattc_event_handler(GATTcEventHandler *handler) { this->gattc_event_handlers_.push_back(handler); }
  void register_gatts_event_handler(GATTsEventHandler *handler) { this->gatts_event_handlers_.push_back(handler); }

 protected:
  static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  void real_gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  void real_gattc_event_handler_(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  void real_gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  bool ble_setup_();

  std::vector<GAPEventHandler *> gap_event_handlers_;
  std::vector<GATTcEventHandler *> gattc_event_handlers_;
  std::vector<GATTsEventHandler *> gatts_event_handlers_;

  Queue<BLEEvent> ble_events_;
  BLEAdvertising *advertising_;
  esp_ble_io_cap_t io_cap_{ESP_IO_CAP_NONE};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32BLE *global_ble;

}  // namespace esp32_ble
}  // namespace esphome

#endif
