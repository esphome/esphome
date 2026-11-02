#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::ezo_pmp {

class EzoPMP final : public PollingComponent, public i2c::I2CDevice {
 public:
  void dump_config() override;

  void loop() override;
  void update() override;

  // Actions for EZO-PMP
  void find();
  void dose_continuously();
  void dose_volume(double volume);
  void dose_volume_over_time(double volume, int duration);
  void dose_with_constant_flow_rate(double volume, int duration);
  void set_calibration_volume(double volume);
  void clear_total_volume_dosed();
  void clear_calibration();
  void pause_dosing();
  void stop_dosing();
  void change_i2c_address(int address);
  void exec_arbitrary_command(const std::basic_string<char> &command);

 protected:
  uint32_t start_time_ = 0;
  uint32_t wait_time_ = 0;
  bool is_waiting_ = false;
  bool is_first_read_ = true;

  uint16_t next_command_ = 0;
  double next_command_volume_ = 0;  // might be negative
  int next_command_duration_ = 0;

  uint16_t next_command_queue_[10];
  double next_command_volume_queue_[10];
  int next_command_duration_queue_[10];
  int next_command_queue_head_ = 0;
  int next_command_queue_last_ = 0;
  int next_command_queue_length_ = 0;

  uint16_t current_command_ = 0;
  bool is_paused_flag_ = false;
  bool is_dosing_flag_ = false;

  std::string arbitrary_command_{};

  void send_next_command_();
  void read_command_result_();
  void clear_current_command_();
  void queue_command_(uint16_t command, double volume, int duration, bool should_schedule);
  void pop_next_command_();
  uint16_t peek_next_command_();

#ifdef USE_SENSOR
  SUB_SENSOR(current_volume_dosed)
  SUB_SENSOR(total_volume_dosed)
  SUB_SENSOR(absolute_total_volume_dosed)
  SUB_SENSOR(pump_voltage)
  SUB_SENSOR(max_flow_rate)
  SUB_SENSOR(last_volume_requested)
 protected:
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(is_dosing)
  SUB_BINARY_SENSOR(is_paused)
 protected:
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(dosing_mode)
  SUB_TEXT_SENSOR(calibration_status)
#endif
};

}  // namespace esphome::ezo_pmp
