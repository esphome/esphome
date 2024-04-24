#pragma once

// All information related to reading battery levels came from the sensors.airthings_wave
// project by Sverre Hamre (https://github.com/sverrham/sensor.airthings_wave)

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

namespace espbt = esphome::esp32_ble_tracker;

static const uint8_t ACCESS_CONTROL_POINT_COMMAND = 0x6d;
static const auto CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID = espbt::ESPBTUUID::from_uint16(0x2902);

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
  void set_battery_voltage(sensor::Sensor *voltage) {
    battery_voltage_ = voltage;
    this->read_battery_next_update_ = true;
  }
  void set_battery_update_interval(uint32_t interval) { battery_update_interval_ = interval; }

 protected:
  bool is_valid_voc_value_(uint16_t voc);

  bool request_read_values_();
  virtual void read_sensors(uint8_t *raw_value, uint16_t value_len) = 0;

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *tvoc_sensor_{nullptr};
  sensor::Sensor *battery_voltage_{nullptr};

  uint16_t handle_;
  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID sensors_data_characteristic_uuid_;

  uint16_t acp_handle_{0};
  uint16_t cccd_handle_{0};
  espbt::ESPBTUUID access_control_point_characteristic_uuid_;

  uint8_t responses_pending_{0};
  void response_pending_();
  void response_received_();
  void set_response_timeout_();

  // default to *not* reading battery voltage from the device; the
  // set_* function for the battery sensor will set this to 'true'
  bool read_battery_next_update_{false};
  bool request_battery_();
  void read_battery_(uint8_t *raw_value, uint16_t value_len);
  uint32_t battery_update_interval_{};

  struct AccessControlPointResponse {
    uint32_t unused1;
    uint8_t unused2;
    uint8_t illuminance;
    uint8_t unused3[10];
    uint16_t unused4[4];
    uint16_t battery;
    uint16_t unused5;
  };
};

}  // namespace airthings_wave_base
}  // namespace esphome

#endif  // USE_ESP32
