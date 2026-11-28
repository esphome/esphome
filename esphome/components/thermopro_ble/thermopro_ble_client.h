#pragma once

#include "esphome/core/defines.h"

#ifdef USE_THERMOPRO_BLE_GATT

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "thermopro_ble_protocol.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::thermopro_ble {

class ThermoProBLEClient : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void dump_config() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void set_timeout_ms(uint32_t timeout_ms) { this->timeout_ms_ = timeout_ms; }

#ifdef USE_SENSOR
  SUB_SENSOR(temperature)
  SUB_SENSOR(external_temperature)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(notification_count)
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(temperature_stream)
#endif

 protected:
  void invalidate_();
  void publish_reading_(const ParseResult &reading);
  uint16_t handle_{0};
  uint32_t timeout_ms_{15000};
  NotificationState notification_state_;
};

}  // namespace esphome::thermopro_ble
#endif
