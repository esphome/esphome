// standard .h section
#pragma once
#include "esphome/core/log.h"
#include <chrono>

namespace esphome {
namespace cv_controller {

struct ControlParameters {
  float kp = 1.0;
  float kp_low = 1.0;
  float ki = 1.0;
  float pi_max = 80.0;
  float pi_target = 70.0;
  float control_band = 2.0;
  int time_interval = 60;  // in seconds
  float start_output = 30.0;
};

class PidController {
 public:
  void setSetpoint(float setpoint);
  void setCurrent(float current);  // true when temperature accuracy < 0.1 degrees

  float getControlOutput();  // calculate the new output value (%)
  float getPPvalue();        // Current pp control value (%)
  float getPIvalue();        // current PI control value (%)
  float getPTvalue();        // current PT control value (%)

  void setup(ControlParameters params, bool high_accuracy);
  void loop();
  void print();

 protected:
  const char *TAG = "PIDController";
  bool debug_ = false;
  int start_counter_ = 0;
  long unsigned int start_time_;

  // parameters for the PID controller
  float max_ = 100.0;  // max value of the output (%)
  float min_ = 0.0;    // min value of the output (%)

  // the following parameters are set during the PY validation phase
  float pi_max_ = 80;              // above this value we will stop changing the error
  float kP_ = 0;                   // Proportional factor
  float kI_ = 0;                   // Intergral factor
  float control_band_ = 2.0;       // active control band
  int time_interval_ = 60 * 1000;  // PID controller update interval in seconds
  float start_output_ = 0.0;       // used to start the pid controller at a specif value (used for testing)
  // end of py validation

  // parameters used during the calculation of the new PID values
  float pi_min_ = 0;        // below this value we will stop changing the error
  float measuredTemp_ = 0;  // current measurement
  float setpoint_ = 0;      // setpoint
  float sum_error_ = 0;

  float pt_ = 0;  // control output, clamped between 0 and 100
  float pp_ = 0;  // proportional value
  float pi_ = 0;  // integral value

  bool high_accuracy_;  // temp sensor is 0.1 degree accuracy

  void resetOutput(float start);  // will reset the pi value to the give start value
  void calculate();               // calculate the required output value
  void calculateLow();            // calculate control for a low accuracy valve (on/off controller)
  void calculateHigh();           // calculate control for high accuracy muur controller (PI controller)
  void calculateSumError(float error, bool controlling);  // calculate sum_error with checks
};

}  // namespace cv_controller
}  // namespace esphome
