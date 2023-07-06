#pragma once

#ifdef USE_ESP32

#include <esp_gattc_api.h>
#include <algorithm>
#include <iterator>
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace airthings_wave_base {

class AirthingsWaveBase : public PollingComponent, public ble_client::BLEClientNode {
 public:
  AirthingsWaveBase() = default;

  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_temperature(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_pressure(sensor::Sensor *pressure) { pressure_sensor_ = pressure; }
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_sensor_ = tvoc; }

 protected:
  bool is_valid_voc_value_(uint16_t voc);

  virtual void read_sensors(uint8_t *value, uint16_t value_len) = 0;
  void request_read_values_();

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *tvoc_sensor_{nullptr};

  uint16_t handle_;
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_data_characteristic_uuid_;
};

}  // namespace airthings_wave_base
}  // namespace esphome

#endif  // USE_ESP32
