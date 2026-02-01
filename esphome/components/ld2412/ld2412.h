#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/ld24xx/ld24xx.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include <array>

namespace esphome::ld2412 {

using namespace ld24xx;

static constexpr uint8_t MAX_LINE_LENGTH = 54;  // Max characters for serial buffer
static constexpr uint8_t TOTAL_GATES = 14;      // Total number of gates supported by the LD2412

class LD2412Component : public Component, public uart::UARTDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(dynamic_background_correction_status)
  SUB_BINARY_SENSOR(moving_target)
  SUB_BINARY_SENSOR(still_target)
  SUB_BINARY_SENSOR(target)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR_WITH_DEDUP(light, uint8_t)
  SUB_SENSOR_WITH_DEDUP(detection_distance, int)
  SUB_SENSOR_WITH_DEDUP(moving_target_distance, int)
  SUB_SENSOR_WITH_DEDUP(moving_target_energy, uint8_t)
  SUB_SENSOR_WITH_DEDUP(still_target_distance, int)
  SUB_SENSOR_WITH_DEDUP(still_target_energy, uint8_t)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(mac)
  SUB_TEXT_SENSOR(version)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(light_threshold)
  SUB_NUMBER(max_distance_gate)
  SUB_NUMBER(min_distance_gate)
  SUB_NUMBER(timeout)
#endif
#ifdef USE_SELECT
  SUB_SELECT(baud_rate)
  SUB_SELECT(distance_resolution)
  SUB_SELECT(light_function)
  SUB_SELECT(out_pin_level)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(bluetooth)
  SUB_SWITCH(engineering_mode)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(factory_reset)
  SUB_BUTTON(query)
  SUB_BUTTON(restart)
  SUB_BUTTON(start_dynamic_background_correction)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_light_out_control();
  void set_basic_config();
#ifdef USE_NUMBER
  void set_gate_move_threshold_number(uint8_t gate, number::Number *n);
  void set_gate_still_threshold_number(uint8_t gate, number::Number *n);
  void set_gate_threshold();
  void get_gate_threshold();
#endif
#ifdef USE_SENSOR
  void set_gate_move_sensor(uint8_t gate, sensor::Sensor *s);
  void set_gate_still_sensor(uint8_t gate, sensor::Sensor *s);
#endif
  void set_engineering_mode(bool enable);
  void read_all_info();
  void restart_and_read_all_info();
  void set_bluetooth(bool enable);
  void set_distance_resolution(const char *state);
  void set_baud_rate(const char *state);
  void factory_reset();
  void start_dynamic_background_correction();

 protected:
  void send_command_(uint8_t command_str, const uint8_t *command_value, uint8_t command_value_len);
  void set_config_mode_(bool enable);
  void handle_periodic_data_();
  bool handle_ack_data_();
  void readline_(int readch);
  void query_parameters_();
  void get_version_();
  void get_mac_();
  void get_distance_resolution_();
  void query_light_control_();
  void restart_();
  void query_dynamic_background_correction_();

  uint8_t light_function_ = 0;
  uint8_t light_threshold_ = 0;
  uint8_t out_pin_level_ = 0;
  uint8_t buffer_pos_ = 0;  // where to resume processing/populating buffer
  uint8_t buffer_data_[MAX_LINE_LENGTH];
  uint8_t mac_address_[6] = {0, 0, 0, 0, 0, 0};
  uint8_t version_[6] = {0, 0, 0, 0, 0, 0};
  bool bluetooth_on_{false};
  bool dynamic_background_correction_active_{false};
#ifdef USE_NUMBER
  std::array<number::Number *, TOTAL_GATES> gate_move_threshold_numbers_{};
  std::array<number::Number *, TOTAL_GATES> gate_still_threshold_numbers_{};
#endif
#ifdef USE_SENSOR
  std::array<SensorWithDedup<uint8_t> *, TOTAL_GATES> gate_move_sensors_{};
  std::array<SensorWithDedup<uint8_t> *, TOTAL_GATES> gate_still_sensors_{};
#endif
};

}  // namespace esphome::ld2412
