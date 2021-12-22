#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/float_output.h"
#include "pid_controller.h"
#include "pid_autotuner.h"

namespace esphome {
namespace pid {

class PIDClimate : public climate::Climate, public Component {
 public:
  PIDClimate() = default;
  void setup() override;
  void dump_config() override;

  void set_sensor(sensor::Sensor *sensor) { sensor_ = sensor; }
  void set_cool_output(output::FloatOutput *cool_output) { cool_output_ = cool_output; }
  void set_heat_output(output::FloatOutput *heat_output) { heat_output_ = heat_output; }
  void set_kp(float kp) { controller_.kp = kp; }
  void set_ki(float ki) { controller_.ki = ki; }
  void set_kd(float kd) { controller_.kd = kd; }
  void set_min_integral(float min_integral) { controller_.min_integral = min_integral; }
  void set_max_integral(float max_integral) { controller_.max_integral = max_integral; }

  float get_output_value() const { return output_value_; }
  float get_error_value() const { return controller_.error; }
  float get_kp() { return controller_.kp; }
  float get_ki() { return controller_.ki; }
  float get_kd() { return controller_.kd; }
  float get_proportional_term() const { return controller_.proportional_term; }
  float get_integral_term() const { return controller_.integral_term; }
  float get_derivative_term() const { return controller_.derivative_term; }
  void add_on_pid_computed_callback(std::function<void()> &&callback) {
    pid_computed_callback_.add(std::move(callback));
  }
  void set_default_target_temperature(float default_target_temperature) {
    default_target_temperature_ = default_target_temperature;
  }
  void start_autotune(std::unique_ptr<PIDAutotuner> &&autotune);
  void reset_integral_term();

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  void update_pid_();

  bool supports_cool_() const { return this->cool_output_ != nullptr; }
  bool supports_heat_() const { return this->heat_output_ != nullptr; }

  void write_output_(float value);

  /// The sensor used for getting the current temperature
  sensor::Sensor *sensor_;
  output::FloatOutput *cool_output_ = nullptr;
  output::FloatOutput *heat_output_ = nullptr;
  PIDController controller_;
  /// Output value as reported by the PID controller, for PIDClimateSensor
  float output_value_;
  CallbackManager<void()> pid_computed_callback_;
  float default_target_temperature_;
  std::unique_ptr<PIDAutotuner> autotuner_;
  bool do_publish_ = false;
};

template<typename... Ts> class PIDAutotuneAction : public Action<Ts...> {
 public:
  PIDAutotuneAction(PIDClimate *parent) : parent_(parent) {}

  void set_noiseband(float noiseband) { noiseband_ = noiseband; }
  void set_positive_output(float positive_output) { positive_output_ = positive_output; }
  void set_negative_output(float negative_output) { negative_output_ = negative_output; }

  void play(Ts... x) {
    auto tuner = make_unique<PIDAutotuner>();
    tuner->set_noiseband(this->noiseband_);
    tuner->set_output_negative(this->negative_output_);
    tuner->set_output_positive(this->positive_output_);
    this->parent_->start_autotune(std::move(tuner));
  }

 protected:
  float noiseband_;
  float positive_output_;
  float negative_output_;
  PIDClimate *parent_;
};

template<typename... Ts> class PIDResetIntegralTermAction : public Action<Ts...> {
 public:
  PIDResetIntegralTermAction(PIDClimate *parent) : parent_(parent) {}

  void play(Ts... x) { this->parent_->reset_integral_term(); }

 protected:
  PIDClimate *parent_;
};

template<typename... Ts> class PIDSetControlParametersAction : public Action<Ts...> {
 public:
  PIDSetControlParametersAction(PIDClimate *parent) : parent_(parent) {}

  void play(Ts... x) {
    auto kp = this->kp_.value(x...);
    auto ki = this->ki_.value(x...);
    auto kd = this->kd_.value(x...);

    this->parent_->set_kp(kp);
    this->parent_->set_ki(ki);
    this->parent_->set_kd(kd);
  }

 protected:
  TEMPLATABLE_VALUE(float, kp)
  TEMPLATABLE_VALUE(float, ki)
  TEMPLATABLE_VALUE(float, kd)

  PIDClimate *parent_;
};

}  // namespace pid
}  // namespace esphome
