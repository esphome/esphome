#pragma once

#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "pid_controller.h"
#include "pid_simulator.h"

#include <vector>

namespace esphome {
namespace pid {

class PIDAutotuner {
 public:
  struct PIDResult {
    float kp;
    float ki;
    float kd;
  };
  struct PIDAutotuneResult {
    float output;
    optional<PIDResult> result_params;
  };

  void config(float output_min, float output_max) {
    relay_function_.output_negative = std::max(relay_function_.output_negative, output_min);
    relay_function_.output_positive = std::min(relay_function_.output_positive, output_max);
  }
  PIDAutotuneResult update(float setpoint, float process_variable);
  bool is_finished() const { return state_ != AUTOTUNE_RUNNING; }

  void dump_config();

  void set_noiseband(float noiseband) {
    relay_function_.noiseband = noiseband;
    // ZC detector uses 1/4 the noiseband of relay function (noise suppression)
    frequency_detector_.noiseband = noiseband / 4;
  }
  void set_output_positive(float output_positive) { relay_function_.output_positive = output_positive; }
  void set_output_negative(float output_negative) { relay_function_.output_negative = output_negative; }

 protected:
  struct RelayFunction {
    float update(float error);

    float current_target_error() const {
      if (state == RELAY_FUNCTION_INIT)
        return 0;
      if (state == RELAY_FUNCTION_POSITIVE)
        return -noiseband;
      return noiseband;
    }

    enum RelayFunctionState {
      RELAY_FUNCTION_INIT,
      RELAY_FUNCTION_POSITIVE,
      RELAY_FUNCTION_NEGATIVE,
    } state = RELAY_FUNCTION_INIT;
    float noiseband = 0.5;
    float output_positive = 1;
    float output_negative = -1;
    uint32_t phase_count = 0;
  } relay_function_;
  struct OscillationFrequencyDetector {
    void update(uint32_t now, float error);

    bool has_enough_data() const;

    float get_mean_oscillation_period() const;

    bool is_increase_decrease_symmetrical() const;

    enum FrequencyDetectorState {
      FREQUENCY_DETECTOR_INIT,
      FREQUENCY_DETECTOR_POSITIVE,
      FREQUENCY_DETECTOR_NEGATIVE,
    } state;
    float noiseband = 0.05;
    uint32_t last_zerocross{0};
    std::vector<uint32_t> zerocrossing_intervals;
  } frequency_detector_;
  struct OscillationAmplitudeDetector {
    void update(float error, RelayFunction::RelayFunctionState relay_state);

    bool has_enough_data() const;

    float get_mean_oscillation_amplitude() const;

    bool is_amplitude_convergent() const;

    float phase_min = NAN;
    float phase_max = NAN;
    std::vector<float> phase_mins;
    std::vector<float> phase_maxs;
    RelayFunction::RelayFunctionState last_relay_state = RelayFunction::RELAY_FUNCTION_INIT;
  } amplitude_detector_;
  PIDResult calculate_pid_(float kp_factor, float ki_factor, float kd_factor);
  void print_rule_(const char *name, float kp_factor, float ki_factor, float kd_factor);
  PIDResult get_ziegler_nichols_pid_() { return calculate_pid_(0.6f, 1.2f, 0.075f); }

  uint32_t enough_data_phase_ = 0;
  float setpoint_ = NAN;
  enum State {
    AUTOTUNE_RUNNING,
    AUTOTUNE_SUCCEEDED,
  } state_ = AUTOTUNE_RUNNING;
  float ku_;
  float pu_;
};

}  // namespace pid
}  // namespace esphome
