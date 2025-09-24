#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_ESP32
#include <esp_gattc_api.h>

namespace esphome {
namespace ble_client {

namespace espbt = esphome::esp32_ble_tracker;

class BLETextSensor : public text_sensor::TextSensor, public PollingComponent, public BLEClientNode {
 public:
  void loop() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_descr_uuid16(uint16_t uuid) { this->descr_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_descr_uuid32(uint32_t uuid) { this->descr_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_descr_uuid128(uint8_t *uuid) { this->descr_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_enable_notify(bool notify) { this->notify_ = notify; }
  std::string parse_data(uint8_t *value, uint16_t value_len);
  uint16_t handle;

 protected:
  bool notify_;
  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID char_uuid_;
  espbt::ESPBTUUID descr_uuid_;
};

}  // namespace ble_client
}  // namespace esphome
#endif
