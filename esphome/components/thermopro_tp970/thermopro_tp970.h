#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "thermopro_tp970_protocol.h"

#ifdef USE_ESP32
namespace esphome::thermopro_tp970 {

class ThermoProTP970 : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void dump_config() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void set_timeout_ms(uint32_t timeout_ms) { this->timeout_ms_ = timeout_ms; }

#ifdef USE_SENSOR
  SUB_SENSOR(internal_temperature)
  SUB_SENSOR(ambient_temperature)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(notification_count)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(temperature_stream)
#endif

 protected:
  void invalidate_();
  void publish_reading_(const TemperatureReading &reading);
  uint16_t handle_{0};
  uint32_t timeout_ms_{15000};
  uint32_t last_notification_{0};
  uint32_t notification_count_{0};
  bool streaming_{false};
};

}  // namespace esphome::thermopro_tp970
#endif
