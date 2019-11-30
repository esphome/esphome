#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace pid {

struct PidClimateTuningParams {
 public:
  PidClimateTuningParams();
  PidClimateTuningParams(float kp, float ki, float kd, float i_max, float i_enable);

  float kp{NAN};
  float ki{NAN};
  float kd{NAN};
  float i_max{NAN};
  float i_enable{NAN};
};

class PidClimate : public climate::Climate, public PollingComponent {
 public:
  PidClimate();
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_target_temperature(float target_temperature) { this->target_temperature = target_temperature; }
  void set_sensor(sensor::Sensor *sensor) { sensor_ = sensor; };
  void set_tuning_params(const PidClimateTuningParams &config) { tuning_params_ = config; }
  void set_output(output::FloatOutput *output) { float_output_ = output; }

  // Expose values from last PID calculation for use in lambdas in other components.
  double output{0};
  double output_p{0};
  double output_i{0};
  double output_d{0};

 protected:
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
  
  void switch_to_mode_(climate::ClimateMode mode);
  void switch_to_action_(climate::ClimateAction action);

  // The sensor used for getting the current temperature
  sensor::Sensor *sensor_{nullptr};
  // The output that will be updated with the output of the PID loop
  output::FloatOutput *float_output_;

  // PID loop tuning parameters
  PidClimateTuningParams tuning_params_{};

  double previous_temperature_{NAN};
  double i_err_{0};
};

}  // namespace pid
}  // namespace esphome
