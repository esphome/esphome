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
// #ifdef USE_SELECT
// #include "esphome/components/select/select.h"
// #endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/cover/cover.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace fyrtur_motor {

typedef enum RollingDirection { FORWARD, REVERSE } RollingDirection_t;

class FyrturMotorComponent : public PollingComponent, public uart::UARTDevice, public cover::Cover {
 public:
  FyrturMotorComponent() = default;

  // #ifdef CUSTOM_FW
  // #ifdef USE_SENSOR
  //   SUB_SENSOR(version)
  //   SUB_SENSOR(minimum_voltage)
  //   SUB_SENSOR(status)
  //   SUB_SENSOR(current)
  //   SUB_SENSOR(speed)
  //   SUB_SENSOR(calibration_status)
  //   SUB_SENSOR(max_length)
  //   SUB_SENSOR(full_length)
  // #endif

  // #ifdef USE_TEXT_SENSOR
  //   SUB_TEXT_SENSOR(status)
  // #endif

  // #ifdef USE_BINARY_SENSOR
  //   SUB_BINARY_SENSOR(charging_mos_enabled)
  //   SUB_BINARY_SENSOR(discharging_mos_enabled)
  // #endif
  // #endif

#ifdef USE_SENSOR
  SUB_SENSOR(battery_level)
  SUB_SENSOR(voltage)
  SUB_SENSOR(speed)
  SUB_SENSOR(position)
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(moving)
  SUB_BINARY_SENSOR(fully_open)
  SUB_BINARY_SENSOR(fully_closed)
  SUB_BINARY_SENSOR(partialy_open)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(status)
#endif

#ifdef USE_BUTTON
  SUB_BUTTON(stop)
  SUB_BUTTON(move_up)
  SUB_BUTTON(move_down)
  SUB_BUTTON(toggle_roll_direction)
  SUB_BUTTON(set_max_length)
  SUB_BUTTON(reset_max_length)
#endif

#ifdef USE_NUMBER
  SUB_NUMBER(upper_setpoint)
  SUB_NUMBER(lower_setpoint)
#endif

#ifdef USE_SWITCH
  SUB_SWITCH(open_close)
#endif

  void setup() override;
  // void dump_config() override;
  void update() override;
  void loop() override;
  float get_setup_priority() const override;

#ifdef USE_SWITCH
  void open_close(bool state);
#endif

#ifdef USE_NUMBER
  void update_setpoints();
#endif

  void set_position(uint8_t position);
  void move_up();
  void move_down();
  void move_up_6();
  void move_down_6();
  void move_up_30();
  void move_down_30();
  void move_up_2();
  void move_down_2();
  void set_max_length();
  void set_full_length();
  void reset_max_length();
  void toggle_roll_direction();
  void stop();
  void get_status();

 protected:
  void send_command(const std::vector<uint8_t> &data);
  uint8_t get_checksum(const std::vector<uint8_t> &data);
  std::vector<uint8_t> get_response(size_t amount_of_data_bytes_to_get);
  std::vector<uint8_t> send_command_and_get_response(const std::vector<uint8_t> &data,
                                                     size_t amount_of_data_bytes_to_get, size_t attempts);
};

}  // namespace fyrtur_motor
}  // namespace esphome
