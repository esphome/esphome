#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace fyrtur_motor {

typedef enum RollingDirection { FORWARD, REVERSE } RollingDirection_t;

class FyrturMotorComponent : public PollingComponent, public uart::UARTDevice {
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

  void setup() override;
  void dump_config() override;
  void update() override;
  // void loop() override;
  float get_setup_priority() const override;

  void set_position(uint8_t position);
  void move_up(void);
  void move_down(void);
  void move_up_6(void);
  void move_down_6(void);
  void move_up_30(void);
  void move_down_30(void);
  void move_up_2(void);
  void move_down_2(void);
  void set_max_length(void);
  void set_full_length(void);
  void reset_max_length(void);
  void set_rolling_direction(RollingDirection_t direction);
  void stop(void);
  void get_status(void);

 protected:
  void send_command(const std::vector<uint8_t> &data);
  uint8_t get_checksum(const std::vector<uint8_t> &data);
  const std::vector<uint8_t> get_response(size_t amount_of_data_bytes_to_get);
  const std::vector<uint8_t> send_command_and_get_response(const std::vector<uint8_t> &data,
                                                           size_t amount_of_data_bytes_to_get, size_t attempts);
};

}  // namespace fyrtur_motor
}  // namespace esphome
