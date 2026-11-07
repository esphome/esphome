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

#ifdef USE_SENSOR
  void set_current_volume_dosed(sensor::Sensor *current_volume_dosed) { current_volume_dosed_ = current_volume_dosed; }
  void set_total_volume_dosed(sensor::Sensor *total_volume_dosed) { total_volume_dosed_ = total_volume_dosed; }
  void set_absolute_total_volume_dosed(sensor::Sensor *absolute_total_volume_dosed) {
    absolute_total_volume_dosed_ = absolute_total_volume_dosed;
  }
  void set_pump_voltage(sensor::Sensor *pump_voltage) { pump_voltage_ = pump_voltage; }
  void set_last_volume_requested(sensor::Sensor *last_volume_requested) {
    last_volume_requested_ = last_volume_requested;
  }
  void set_max_flow_rate(sensor::Sensor *max_flow_rate) { max_flow_rate_ = max_flow_rate; }
#endif

#ifdef USE_BINARY_SENSOR
  void set_is_dosing(binary_sensor::BinarySensor *is_dosing) { is_dosing_ = is_dosing; }
  void set_is_paused(binary_sensor::BinarySensor *is_paused) { is_paused_ = is_paused; }
#endif

#ifdef USE_TEXT_SENSOR
  void set_dosing_mode(text_sensor::TextSensor *dosing_mode) { dosing_mode_ = dosing_mode; }
  void set_calibration_status(text_sensor::TextSensor *calibration_status) { calibration_status_ = calibration_status; }
#endif

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
  sensor::Sensor *current_volume_dosed_{nullptr};
  sensor::Sensor *total_volume_dosed_{nullptr};
  sensor::Sensor *absolute_total_volume_dosed_{nullptr};
  sensor::Sensor *pump_voltage_{nullptr};
  sensor::Sensor *max_flow_rate_{nullptr};
  sensor::Sensor *last_volume_requested_{nullptr};
#endif

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *is_dosing_{nullptr};
  binary_sensor::BinarySensor *is_paused_{nullptr};
#endif

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *dosing_mode_{nullptr};
  text_sensor::TextSensor *calibration_status_{nullptr};
#endif
};

}  // namespace esphome::ezo_pmp
