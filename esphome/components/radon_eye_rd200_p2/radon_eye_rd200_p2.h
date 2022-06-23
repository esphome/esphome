#pragma once

#ifdef USE_ESP32

#include <algorithm>
#include <iterator>
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <esp_gattc_api.h>

namespace esphome {
namespace radon_eye_rd200_p2 {

static const char *const SERVICE_UUID = "00001523-1212-EFDE-1523-785FEABCD123";
static const char *const WRITE_CHARACTERISTIC_UUID = "00001524-1212-EFDE-1523-785FEABCD123";
static const char *const READ_CHARACTERISTIC_UUID = "00001525-1212-EFDE-1523-785FEABCD123";

class RadonEyeRD200P2 : public PollingComponent, public ble_client::BLEClientNode {
 public:
  RadonEyeRD200P2();

  void dump_config() override;
  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_radon(sensor::Sensor *radon) { radon_sensor_ = radon; }
  void set_temperature(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }

 protected:
  void read_sensors_radon_(uint8_t *value, uint16_t value_len);
  void read_sensors_temp_hum_(uint8_t *value, uint16_t value_len);

  void write_query_radon_message_();
  void write_query_temp_hum_message_();
  void request_read_values_();

  bool has_required_chars_();

  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  uint16_t read_handle_;
  uint16_t write_handle_;
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_write_characteristic_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_read_characteristic_uuid_;

  float temphum_u16_to_temperature_(uint16_t value);
  uint16_t temphum_u16_to_humidity_(uint16_t value);

  enum QueryType { QUERY_RADON, QUERY_TEMP_HUM };
  QueryType current_query_;
};

}  // namespace radon_eye_rd200_p2
}  // namespace esphome

#endif  // USE_ESP32
