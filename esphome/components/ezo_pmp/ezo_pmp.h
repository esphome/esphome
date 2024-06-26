#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
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

namespace esphome {
namespace ezo_pmp {

class EzoPMP : public PollingComponent, public i2c::I2CDevice {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

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

  const char *arbitrary_command_{nullptr};

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

// Action Templates
template<typename... Ts> class EzoPMPFindAction : public Action<Ts...> {
 public:
  EzoPMPFindAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->find(); }

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPDoseContinuouslyAction : public Action<Ts...> {
 public:
  EzoPMPDoseContinuouslyAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->dose_continuously(); }

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPDoseVolumeAction : public Action<Ts...> {
 public:
  EzoPMPDoseVolumeAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->dose_volume(this->volume_.value(x...)); }
  TEMPLATABLE_VALUE(double, volume)

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPDoseVolumeOverTimeAction : public Action<Ts...> {
 public:
  EzoPMPDoseVolumeOverTimeAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override {
    this->ezopmp_->dose_volume_over_time(this->volume_.value(x...), this->duration_.value(x...));
  }
  TEMPLATABLE_VALUE(double, volume)
  TEMPLATABLE_VALUE(int, duration)

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPDoseWithConstantFlowRateAction : public Action<Ts...> {
 public:
  EzoPMPDoseWithConstantFlowRateAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override {
    this->ezopmp_->dose_with_constant_flow_rate(this->volume_.value(x...), this->duration_.value(x...));
  }
  TEMPLATABLE_VALUE(double, volume)
  TEMPLATABLE_VALUE(int, duration)

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPSetCalibrationVolumeAction : public Action<Ts...> {
 public:
  EzoPMPSetCalibrationVolumeAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->set_calibration_volume(this->volume_.value(x...)); }
  TEMPLATABLE_VALUE(double, volume)

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPClearTotalVolumeDispensedAction : public Action<Ts...> {
 public:
  EzoPMPClearTotalVolumeDispensedAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->clear_total_volume_dosed(); }

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPClearCalibrationAction : public Action<Ts...> {
 public:
  EzoPMPClearCalibrationAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->clear_calibration(); }

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPPauseDosingAction : public Action<Ts...> {
 public:
  EzoPMPPauseDosingAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->pause_dosing(); }

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPStopDosingAction : public Action<Ts...> {
 public:
  EzoPMPStopDosingAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->stop_dosing(); }

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPChangeI2CAddressAction : public Action<Ts...> {
 public:
  EzoPMPChangeI2CAddressAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->change_i2c_address(this->address_.value(x...)); }
  TEMPLATABLE_VALUE(int, address)

 protected:
  EzoPMP *ezopmp_;
};

template<typename... Ts> class EzoPMPArbitraryCommandAction : public Action<Ts...> {
 public:
  EzoPMPArbitraryCommandAction(EzoPMP *ezopmp) : ezopmp_(ezopmp) {}

  void play(Ts... x) override { this->ezopmp_->exec_arbitrary_command(this->command_.value(x...)); }
  TEMPLATABLE_VALUE(std::string, command)

 protected:
  EzoPMP *ezopmp_;
};

}  // namespace ezo_pmp
}  // namespace esphome
