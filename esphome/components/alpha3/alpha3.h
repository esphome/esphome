#pragma once

#include "alpha3_command.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome::alpha3 {

namespace espbt = esphome::esp32_ble_tracker;

static const espbt::ESPBTUUID ALPHA3_GENI_SERVICE_UUID = espbt::ESPBTUUID::from_uint16(0xFE5D);
static const espbt::ESPBTUUID ALPHA3_GENI_CHARACTERISTIC_UUID = espbt::ESPBTUUID::from_raw(
    {0xA9, 0x7B, 0xB8, 0x85, 0x00, 0x1A, 0x28, 0xAA, 0x2A, 0x43, 0x6E, 0x03, 0xD1, 0xFF, 0x9C, 0x85});

enum class Alpha3SelectType : uint8_t {
  ALPHA3_SELECT_TYPE_OPERATION_MODE,
  ALPHA3_SELECT_TYPE_CONTROL_MODE,
};

enum class Alpha3NumberType : uint8_t {
  ALPHA3_NUMBER_TYPE_CONSTANT_SPEED,
  ALPHA3_NUMBER_TYPE_CONSTANT_PRESSURE,
  ALPHA3_NUMBER_TYPE_PROPORTIONAL_PRESSURE,
};

class Alpha3 final : public ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  bool is_ready() const;

  void set_flow_sensor(sensor::Sensor *sensor) { this->flow_sensor_ = sensor; }
  void set_head_sensor(sensor::Sensor *sensor) { this->head_sensor_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { this->power_sensor_ = sensor; }
  void set_current_sensor(sensor::Sensor *sensor) { this->current_sensor_ = sensor; }
  void set_speed_sensor(sensor::Sensor *sensor) { this->speed_sensor_ = sensor; }
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  void set_operating_hours_sensor(sensor::Sensor *sensor) { this->operating_hours_sensor_ = sensor; }
  void set_energy_sensor(sensor::Sensor *sensor) { this->energy_sensor_ = sensor; }
  void set_starts_sensor(sensor::Sensor *sensor) { this->starts_sensor_ = sensor; }
  void set_alarm_code_sensor(sensor::Sensor *sensor) { this->alarm_code_sensor_ = sensor; }
  void set_warning_code_sensor(sensor::Sensor *sensor) { this->warning_code_sensor_ = sensor; }
#ifdef USE_BINARY_SENSOR
  void set_ready_binary_sensor(binary_sensor::BinarySensor *entity) { this->ready_binary_sensor_ = entity; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_realized_operation_text_sensor(text_sensor::TextSensor *entity) {
    this->realized_operation_text_sensor_ = entity;
  }
  void set_active_control_source_text_sensor(text_sensor::TextSensor *entity) {
    this->active_control_source_text_sensor_ = entity;
  }
#endif
#ifdef USE_SELECT
  void set_operation_mode_select(select::Select *entity) { this->operation_mode_select_ = entity; }
  void set_control_mode_select(select::Select *entity) { this->control_mode_select_ = entity; }
  bool request_operation_mode(uint8_t mode);
  bool request_control_mode(uint8_t mode);
#endif
#ifdef USE_NUMBER
  void set_setpoint_number(Alpha3NumberType type, number::Number *entity);
  bool request_setpoint(Alpha3NumberType type, float displayed_value);
#endif

 protected:
  void try_subscribe_();
  void set_ready_(bool ready);
  void reset_connection_state_();
  void enqueue_initial_reads_();
  void start_next_work_();
  void send_next_fragment_();
  void handle_notification_(const uint8_t *data, size_t size);
  void complete_read_(const ParsedFrame &frame);
  void complete_command_transaction_(const ParsedFrame &frame);
  void execute_command_outcome_(const CommandOutcome &outcome);
  void fail_active_transaction_(const char *reason);
  void handle_response_timeout_();
#if defined(USE_SELECT) || defined(USE_NUMBER)
  bool validate_write_(Capability capability, ObjectKind object_kind);
  bool enqueue_command_(const WorkItem &work);
#endif
#ifdef USE_NUMBER
  number::Number *get_setpoint_number_(ObjectKind kind) const;
#endif

  sensor::Sensor *flow_sensor_{nullptr};
  sensor::Sensor *head_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *operating_hours_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
  sensor::Sensor *starts_sensor_{nullptr};
  sensor::Sensor *alarm_code_sensor_{nullptr};
  sensor::Sensor *warning_code_sensor_{nullptr};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *ready_binary_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *realized_operation_text_sensor_{nullptr};
  text_sensor::TextSensor *active_control_source_text_sensor_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *operation_mode_select_{nullptr};
  select::Select *control_mode_select_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *constant_speed_number_{nullptr};
  number::Number *constant_pressure_number_{nullptr};
  number::Number *proportional_pressure_number_{nullptr};
#endif
  SetpointRanges setpoint_ranges_{};
  CommandPolicy command_policy_{};
  CommandState command_state_{};
  uint16_t geni_handle_{0};
  TransportState transport_{};
  bool registration_requested_{false};
  bool ready_{false};
};

}  // namespace esphome::alpha3

#endif
