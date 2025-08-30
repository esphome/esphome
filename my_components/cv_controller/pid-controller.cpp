

#include "pid-controller.h"

namespace esphome {
namespace cv_controller {

// public methods
void PidController::setup(ControlParameters params, bool is_high_accuracy) {
  high_accuracy_ = is_high_accuracy;
  start_counter_ = 0;

  kP_ = (high_accuracy_) ? params.kp : params.kp_low;
  kI_ = (high_accuracy_) ? params.ki : 0.0;
  pi_max_ = params.pi_max;
  control_band_ = params.control_band;
  time_interval_ = params.time_interval * 1000;  // interval in msec
  start_output_ = params.start_output;

  start_time_ = clock();  // initialise the loop counter
  calculate();            // calculate the first value
}

void PidController::loop() {
  if ((clock() - start_time_) > time_interval_) {
    if (debug_) {
      ESP_LOGI(TAG, "Elapsed: %d Time: %d", clock() - start_time_, start_time_);
    }
    calculate();
    start_time_ = clock();
    if (debug_) {
      ESP_LOGI(TAG, "New clock time: %d  %d", clock(), start_time_);
    }
  }
}

void PidController::setSetpoint(float setpoint_value) {
  setpoint_ = setpoint_value;
  // calculate();                // need to do this immediately because otherwise the old control value will be sent to
  // the CH system. (negative spike)
}

void PidController::setCurrent(float current_value) { measuredTemp_ = current_value; }

float PidController::getControlOutput() {
  return pt_ / 100.0;  // convert from 0-100% in a fraction 0 - 1
}

float PidController::getPPvalue() { return pp_; }

float PidController::getPIvalue() { return pi_; }

float PidController::getPTvalue() { return pt_; }

void PidController::print() {
  if (debug_) {
    ESP_LOGI(TAG, "Temp: %.1f Setpoint: %.1f PT: %.2f PP: %.2f PI: %.2f TempDiff: %f", measuredTemp_, setpoint_, pt_,
             pp_, pi_, (setpoint_ - measuredTemp_));
  }
}

// protected methods

void PidController::resetOutput(float control_output) {
  // only called once during a restart of the ESP, used to get a proper start of the pid controller

  if (debug_) {
    ESP_LOGI(TAG, "RESETTING PI CONTROLLER");
  }
  float error = setpoint_ - measuredTemp_;
  if (abs(error) <= control_band_) {
    // we are in control mode calculate the sum_error to get the required starting control_output
    // control_output = kP * error + kI * sum_error
    if (high_accuracy_) {
      sum_error_ = (control_output - kP_ * error) / kI_;
      calculateSumError(error, false);  // only check pi min and max values
      pp_ = kP_ * error;
      pi_ = kI_ * sum_error_;
      pt_ = pp_ + pi_;
    }
  } else {
    // default to zero for the controller
    sum_error_ = 0.0;
    pp_ = 0.0;
    pi_ = 0.0;
    pt_ = pp_ + pi_;
  }
  // clamp output
  if (pt_ > max_) {
    pt_ = max_;
  }
  if (pt_ < min_) {
    pt_ = min_;
  }
}

void PidController::calculate() {
  if (high_accuracy_) {
    calculateHigh();
  } else {
    calculateLow();
  }
  if (debug_) {
    ESP_LOGI(TAG, "Calculate pid");
  }
  // print();    // only during debug
}

void PidController::calculateLow() {
  // simple on / off control at 80% of total power
  // low accuracy error most valves full degrees some 0.5 degrees
  // error >= 2.0 valve on (heating)
  // error == 1 (or 0.5) on when temperature is increasing, off when temperature is decreasing  (antipendeling)
  // error <= 0.0 valve off (no heating)
  float error = setpoint_ - measuredTemp_;
  pp_ = kP_ * error;
  pi_ = 0.0;
  pt_ = pp_ + pi_;

  // clamp output
  if (pt_ > max_) {
    pt_ = max_;
  }
  if (pt_ < min_) {
    pt_ = min_;
  }
}

void PidController::calculateHigh() {
  /*
  Simple PI controller pt = kP * error + kI * sum(error)

  */

  if (start_counter_ > 1) {  // use the counter to avoid peaks during startup of the controller
    float error = setpoint_ - measuredTemp_;
    if (abs(error) <= control_band_) {
      // we are inside controlband
      calculateSumError(error, true);
      pi_ = kI_ * sum_error_;
      pp_ = kP_ * error;
      pt_ = pp_ + pi_;
    } else {
      // we are outside the controlband, set pp and pt
      calculateSumError(error, false);
      pp_ = kP_ * error;
      pt_ = pp_ + pi_;  // outside the control band we will not change pi
    }
  } else {
    pp_ = 0.0;
    pi_ = pi_min_;
    pt_ = 0.0;
    sum_error_ = 0.0;
    start_counter_++;
  }
  // clamp output
  if (pt_ > max_) {
    pt_ = max_;
  }
  if (pt_ < min_) {
    pt_ = min_;
  }
}

void PidController::calculateSumError(float error, bool controlling) {
  if (controlling) {
    // outside the controllwindow we are not integrating the error
    sum_error_ += error;
  }
  pi_ = kI_ * sum_error_;
  if (pi_ < pi_min_) {
    // we are outside the integration control window, set min sum_error
    sum_error_ = pi_min_ / kI_;
  }
  if (pi_ > pi_max_) {
    // we are outside the integration control window, set min sum_error
    sum_error_ = pi_max_ / kI_;
  }
}

}  // namespace cv_controller
}  // namespace esphome
