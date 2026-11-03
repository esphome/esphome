#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome::alpha3 {

namespace espbt = esphome::esp32_ble_tracker;

static const espbt::ESPBTUUID ALPHA3_GENI_SERVICE_UUID = espbt::ESPBTUUID::from_uint16(0xfe5d);
static const espbt::ESPBTUUID ALPHA3_GENI_CHARACTERISTIC_UUID = espbt::ESPBTUUID::from_raw(
    {0xa9, 0x7b, 0xb8, 0x85, 0x00, 0x1a, 0x28, 0xaa, 0x2a, 0x43, 0x6e, 0x03, 0xd1, 0xff, 0x9c, 0x85});
static const int16_t GENI_RESPONSE_HEADER_LENGTH = 13;
static const size_t GENI_RESPONSE_TYPE_LENGTH = 8;

static const uint8_t GENI_RESPONSE_TYPE_FLOW_HEAD[GENI_RESPONSE_TYPE_LENGTH] = {31, 0, 1, 48, 1, 0, 0, 24};
static const int16_t GENI_RESPONSE_FLOW_OFFSET = 0;
static const int16_t GENI_RESPONSE_HEAD_OFFSET = 4;

static const uint8_t GENI_RESPONSE_TYPE_POWER[GENI_RESPONSE_TYPE_LENGTH] = {44, 0, 1, 0, 1, 0, 0, 37};
static const int16_t GENI_RESPONSE_VOLTAGE_AC_OFFSET = 0;
static const int16_t GENI_RESPONSE_VOLTAGE_DC_OFFSET = 4;
static const int16_t GENI_RESPONSE_CURRENT_OFFSET = 8;
static const int16_t GENI_RESPONSE_POWER_OFFSET = 12;
static const int16_t GENI_RESPONSE_MOTOR_POWER_OFFSET = 16;  // not sure
static const int16_t GENI_RESPONSE_MOTOR_SPEED_OFFSET = 20;

class Alpha3 final : public esphome::ble_client::BLEClientNode, public PollingComponent {
  SUB_SENSOR(flow)
  SUB_SENSOR(head)
  SUB_SENSOR(power)
  SUB_SENSOR(current)
  SUB_SENSOR(speed)
  SUB_SENSOR(voltage)

 public:
  void setup() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void dump_config() override;

 protected:
  uint16_t geni_handle_;
  int16_t response_length_;
  int16_t response_offset_;
  uint8_t response_type_[GENI_RESPONSE_TYPE_LENGTH];
  uint8_t buffer_[4];
  void extract_publish_sensor_value_(const uint8_t *response, int16_t length, int16_t response_offset,
                                     int16_t value_offset, sensor::Sensor *sensor, float factor);
  void handle_geni_response_(const uint8_t *response, uint16_t length);
  void send_request_(uint8_t *request, size_t len);
  bool is_current_response_type_(const uint8_t *response_type);
};
}  // namespace esphome::alpha3

#endif
