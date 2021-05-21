#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "ble_service.h"
#include "ble_characteristic.h"
#include "ble_uuid.h"

#include "queue.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble {

class BLEServer : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void teardown();

  void set_manufacturer(const std::string manufacturer) { this->manufacturer_ = manufacturer; }
  void set_model(const std::string model) { this->model_ = model; }

  BLEService *create_service(const uint8_t *uuid);
  BLEService *create_service(const char *uuid);
  BLEService *create_service(ESPBTUUID uuid, uint16_t num_handles = 15, uint8_t inst_id = 0);

  esp_gatt_if_t get_gatts_if() { return this->gatts_if_; }

  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

 protected:
  bool create_device_characteristics_();

  std::string manufacturer_;
  optional<std::string> model_;
  esp_gatt_if_t gatts_if_{0};

  std::vector<BLEService *> services_;
  BLEService *device_information_service;

  enum : uint8_t {
    UNINITIALIZED = 0x00,
    AWAITING_REGISTRATION,
    REGISTERED,
    AWAITING_SERVICE_CREATION,
    AWAITING_SERVICE_PRE_START,
    AWAITING_SERVICE_START,
    RUNNING,
  } state_;
};

extern BLEServer *global_ble_server;

}  // namespace esp32_ble
}  // namespace esphome

#endif
