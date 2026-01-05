#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome {
namespace alpha3 {

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

// Additional response type for mode status (if available)
static const uint8_t GENI_RESPONSE_TYPE_MODE[GENI_RESPONSE_TYPE_LENGTH] = {81, 0, 1, 0, 1, 0, 0, 0};  // act_mode1
static const int16_t GENI_RESPONSE_MODE_OFFSET = 0;

// Command IDs (Class 3 - COMMANDS)
static const uint8_t GENI_CMD_STOP = 5;
static const uint8_t GENI_CMD_START = 6;
static const uint8_t GENI_CMD_REMOTE = 7;
static const uint8_t GENI_CMD_LOCAL = 8;
static const uint8_t GENI_CMD_CONST_FREQ = 22;
static const uint8_t GENI_CMD_PROP_PRESS = 23;
static const uint8_t GENI_CMD_CONST_PRESS = 24;
static const uint8_t GENI_CMD_MIN = 25;
static const uint8_t GENI_CMD_MAX = 26;
static const uint8_t GENI_CMD_REF_UP = 33;
static const uint8_t GENI_CMD_REF_DOWN = 34;
static const uint8_t GENI_CMD_AUTOADAPT = 52;

class Alpha3 : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void dump_config() override;
  void set_flow_sensor(sensor::Sensor *sensor) { this->flow_sensor_ = sensor; }
  void set_head_sensor(sensor::Sensor *sensor) { this->head_sensor_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { this->power_sensor_ = sensor; }
  void set_current_sensor(sensor::Sensor *sensor) { this->current_sensor_ = sensor; }
  void set_speed_sensor(sensor::Sensor *sensor) { this->speed_sensor_ = sensor; }
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  void set_mode_text_sensor(text_sensor::TextSensor *sensor) { this->mode_text_sensor_ = sensor; }

  // Control methods
  void send_command(uint8_t command_id);
  void set_remote_mode();
  void set_local_mode();
  void set_mode_autoadapt();
  void set_mode_const_pressure(uint8_t level);  // 1=MIN, 2=MID, 3=MAX
  void set_mode_prop_pressure(uint8_t level);
  void set_mode_const_freq();
  void stop_pump();
  void start_pump();
  void adjust_setpoint(int8_t delta);  // +1 or -1

 protected:
  sensor::Sensor *flow_sensor_{nullptr};
  sensor::Sensor *head_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  text_sensor::TextSensor *mode_text_sensor_{nullptr};
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
  uint16_t calculate_crc_(const uint8_t *data, size_t len);
  void send_command_(uint8_t command_id);
  void send_commands_(const uint8_t *command_ids, size_t num_commands);
  const char *get_mode_name_(uint8_t mode);
};
}  // namespace alpha3
}  // namespace esphome

#endif
