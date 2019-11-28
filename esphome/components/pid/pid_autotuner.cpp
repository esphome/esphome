#include "pid_autotuner.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pid {

static const char *TAG = "pid.autotune";

PIDAutotuner::PIDAutotuneResult PIDAutotuner::update(float setpoint, float process_variable) {
  PIDAutotuner::PIDAutotuneResult res;
  if (this->state_ == AUTOTUNE_SUCCEEDED) {
    res.result_params = this->get_ziegler_nichols_pid_();
    return res;
  }

  float error = setpoint - process_variable;
  const uint32_t now = millis();

  float output = this->relay_function_.update(error);
  this->frequency_detector_.update(now, error);
  this->amplitude_detector_.update(error, this->relay_function_.state);
  res.output = output;

  if (!this->frequency_detector_.has_enough_data() || !this->amplitude_detector_.has_enough_data())
    // not enough data for calculation yet
    return res;

  if (!this->frequency_detector_.is_increase_decrease_symmetrical() ||
      !this->amplitude_detector_.is_amplitude_convergent()) {
    // The frequency/amplitude is not fully accurate yet, try to wait
    // until the fault clears, or terminate after a while anyway
    uint32_t phase = this->relay_function_.phase_count;
    if (this->enough_data_phase_ == 0) {
      this->enough_data_phase_ = phase;
    } else if (phase - this->enough_data_phase_ <= 6) {
      // keep trying for at least 6 more phases
      return res;
    } else {
      // proceed to calculating PID parameters
      // warning will be shown in "Checks" section
    }
  }

  float osc_ampl = this->amplitude_detector_.get_mean_oscillation_amplitude();
  float d = (this->relay_function_.output_positive - this->relay_function_.output_negative) / 2.0f;
  this->ku_ = 4.0f * d / float(M_PI * osc_ampl);
  this->pu_ = this->frequency_detector_.get_mean_oscillation_period();

  this->state_ = AUTOTUNE_SUCCEEDED;
  res.result_params = this->get_ziegler_nichols_pid_();
  ESP_LOGI(TAG, "PID Autotune finished!");
  this->dump_config();

  return res;
}
void PIDAutotuner::dump_config() {
  ESP_LOGI(TAG, "PID Autotune:");
  if (this->state_ == AUTOTUNE_SUCCEEDED) {
    ESP_LOGI(TAG, "  State: Succeeded!");
    bool has_issue = false;
    if (!this->amplitude_detector_.is_amplitude_convergent()) {
      ESP_LOGW(TAG, "  Could not reliable determine oscillation amplitude, PID parameters may be inaccurate!");
      ESP_LOGW(TAG, "    Please make sure you eliminate all outside influences on the measured temperature.");
      has_issue = true;
    }
    if (!this->frequency_detector_.is_increase_decrease_symmetrical()) {
      ESP_LOGW(TAG, "  Oscillation Frequency is not symmetrical. PID parameters may be inaccurate!");
      ESP_LOGW(
          TAG,
          "    This is usually because the heat and cool processes do not change the temperature at the same rate.");
      ESP_LOGW(TAG, "    Please try reducing the heat_output value (or increase cool_output in case of a cooler)");
      has_issue = true;
    }
    if (!has_issue) {
      ESP_LOGI(TAG, "  All checks passed!");
    }

    auto fac = get_ziegler_nichols_pid_();
    ESP_LOGI(TAG, "  Calculated PID parameters (\"Ziegler-Nichols PID\" rule):");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  control_parameters:");
    ESP_LOGI(TAG, "    kp: %.5f", fac.kp);
    ESP_LOGI(TAG, "    ki: %.5f", fac.ki);
    ESP_LOGI(TAG, "    kd: %.5f", fac.kd);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  Please copy these values into your YAML configuration! They will reset on the next reboot.");

    ESP_LOGD(TAG, "  Alternative Rules:");
    // http://www.mstarlabs.com/control/znrule.html
    print_rule_("Ziegler-Nichols PI", 0.45f, 0.54f, 0.0f);
    print_rule_("Pessen Integral PID", 0.7f, 1.75f, 0.105f);
    print_rule_("Some Overshoot PID", 0.333f, 0.667f, 0.111f);
    print_rule_("No Overshoot PID", 0.2f, 0.4f, 0.0625f);
  }

  if (this->state_ == AUTOTUNE_RUNNING) {
    ESP_LOGI(TAG, "  Autotune is still running!");
    ESP_LOGD(TAG, "  Stats so far:");
    ESP_LOGD(TAG, "    Phases: %u", relay_function_.phase_count);
    ESP_LOGD(TAG, "    Detected %u zero-crossings", frequency_detector_.zerocrossing_intervals.size());  // NOLINT
    ESP_LOGD(TAG, "    Current Phase Min: %.2f, Max: %.2f", amplitude_detector_.phase_min,
             amplitude_detector_.phase_max);
  }
}
PIDAutotuner::PIDResult PIDAutotuner::calculate_pid_(float kp_factor, float ki_factor, float kd_factor) {
  float kp = kp_factor * ku_;
  float ki = ki_factor * ku_ / pu_;
  float kd = kd_factor * ku_ * pu_;
  return {
      .kp = kp,
      .ki = ki,
      .kd = kd,
  };
}
void PIDAutotuner::print_rule_(const char *name, float kp_factor, float ki_factor, float kd_factor) {
  auto fac = calculate_pid_(kp_factor, ki_factor, kd_factor);
  ESP_LOGD(TAG, "    Rule '%s':", name);
  ESP_LOGD(TAG, "      kp: %.5f, ki: %.5f, kd: %.5f", fac.kp, fac.ki, fac.kd);
}

// ================== RelayFunction ==================
float PIDAutotuner::RelayFunction::update(float error) {
  if (this->state == RELAY_FUNCTION_INIT) {
    bool pos = error > this->noiseband;
    state = pos ? RELAY_FUNCTION_POSITIVE : RELAY_FUNCTION_NEGATIVE;
  }
  bool change = false;
  if (this->state == RELAY_FUNCTION_POSITIVE && error < -this->noiseband) {
    // Positive hysteresis reached, change direction
    this->state = RELAY_FUNCTION_NEGATIVE;
    change = true;
  } else if (this->state == RELAY_FUNCTION_NEGATIVE && error > this->noiseband) {
    // Negative hysteresis reached, change direction
    this->state = RELAY_FUNCTION_POSITIVE;
    change = true;
  }

  if (change)
    this->phase_count++;

  return state == RELAY_FUNCTION_POSITIVE ? output_positive : output_negative;
}

// ================== OscillationFrequencyDetector ==================
void PIDAutotuner::OscillationFrequencyDetector::update(uint32_t now, float error) {
  if (this->state == FREQUENCY_DETECTOR_INIT) {
    bool pos = error > this->noiseband;
    state = pos ? FREQUENCY_DETECTOR_POSITIVE : FREQUENCY_DETECTOR_NEGATIVE;
  }

  bool had_crossing = false;
  if (this->state == FREQUENCY_DETECTOR_POSITIVE && error < -this->noiseband) {
    this->state = FREQUENCY_DETECTOR_NEGATIVE;
    had_crossing = true;
  } else if (this->state == FREQUENCY_DETECTOR_NEGATIVE && error > this->noiseband) {
    this->state = FREQUENCY_DETECTOR_POSITIVE;
    had_crossing = true;
  }

  if (had_crossing) {
    // Had crossing above hysteresis threshold, record
    if (this->last_zerocross != 0) {
      uint32_t dt = now - this->last_zerocross;
      this->zerocrossing_intervals.push_back(dt);
    }
    this->last_zerocross = now;
  }
}
bool PIDAutotuner::OscillationFrequencyDetector::has_enough_data() const {
  // Do we have enough data in this detector to generate PID values?
  return this->zerocrossing_intervals.size() >= 2;
}
float PIDAutotuner::OscillationFrequencyDetector::get_mean_oscillation_period() const {
  // Get the mean oscillation period in seconds
  // Only call if has_enough_data() has returned true.
  float sum = 0.0f;
  for (uint32_t v : this->zerocrossing_intervals)
    sum += v;
  // zerocrossings are each half-period, multiply by 2
  float mean_value = sum / this->zerocrossing_intervals.size();
  // divide by 1000 to get seconds, multiply by two because zc happens two times per period
  float mean_period = mean_value / 1000 * 2;
  return mean_period;
}
bool PIDAutotuner::OscillationFrequencyDetector::is_increase_decrease_symmetrical() const {
  // Check if increase/decrease of process value was symmetrical
  // If the process value increases much faster than it decreases, the generated PID values will
  // not be very good and the function output values need to be adjusted
  // Happens for example with a well-insulated heating element.
  // We calculate this based on the zerocrossing interval.
  if (zerocrossing_intervals.empty())
    return false;
  uint32_t max_interval = zerocrossing_intervals[0];
  uint32_t min_interval = zerocrossing_intervals[0];
  for (uint32_t interval : zerocrossing_intervals) {
    max_interval = std::max(max_interval, interval);
    min_interval = std::min(min_interval, interval);
  }
  float ratio = min_interval / float(max_interval);
  return ratio >= 0.66;
}

// ================== OscillationAmplitudeDetector ==================
void PIDAutotuner::OscillationAmplitudeDetector::update(float error,
                                                        PIDAutotuner::RelayFunction::RelayFunctionState relay_state) {
  if (relay_state != last_relay_state) {
    if (last_relay_state == RelayFunction::RELAY_FUNCTION_POSITIVE) {
      // Transitioned from positive error to negative error.
      // The positive error peak must have been in previous segment (180° shifted)
      // record phase_max
      this->phase_maxs.push_back(phase_max);
    } else if (last_relay_state == RelayFunction::RELAY_FUNCTION_NEGATIVE) {
      // Transitioned from negative error to positive error.
      // The negative error peak must have been in previous segment (180° shifted)
      // record phase_min
      this->phase_mins.push_back(phase_min);
    }
    // reset phase values for next phase
    this->phase_min = error;
    this->phase_max = error;
  }
  this->last_relay_state = relay_state;

  this->phase_min = std::min(this->phase_min, error);
  this->phase_max = std::max(this->phase_max, error);

  // Check arrays sizes, we keep at most 7 items (6 datapoints is enough, and data at beginning might not
  // have been stabilized)
  if (this->phase_maxs.size() > 7)
    this->phase_maxs.erase(this->phase_maxs.begin());
  if (this->phase_mins.size() > 7)
    this->phase_mins.erase(this->phase_mins.begin());
}
bool PIDAutotuner::OscillationAmplitudeDetector::has_enough_data() const {
  // Return if we have enough data to generate PID parameters
  // The first phase is not very useful if the setpoint is not set to the starting process value
  // So discard first phase. Otherwise we need at least two phases.
  return std::min(phase_mins.size(), phase_maxs.size()) >= 3;
}
float PIDAutotuner::OscillationAmplitudeDetector::get_mean_oscillation_amplitude() const {
  float total_amplitudes = 0;
  size_t total_amplitudes_n = 0;
  for (int i = 1; i < std::min(phase_mins.size(), phase_maxs.size()) - 1; i++) {
    total_amplitudes += std::abs(phase_maxs[i] - phase_mins[i + 1]);
    total_amplitudes_n++;
  }
  float mean_amplitude = total_amplitudes / total_amplitudes_n;
  // Amplitude is measured from center, divide by 2
  return mean_amplitude / 2.0f;
}
bool PIDAutotuner::OscillationAmplitudeDetector::is_amplitude_convergent() const {
  // Check if oscillation amplitude is convergent
  // We implement this by checking global extrema against average amplitude
  if (this->phase_mins.empty() || this->phase_maxs.empty())
    return false;

  float global_max = phase_maxs[0], global_min = phase_mins[0];
  for (auto v : this->phase_mins)
    global_min = std::min(global_min, v);
  for (auto v : this->phase_maxs)
    global_max = std::min(global_max, v);
  float global_amplitude = (global_max - global_min) / 2.0f;
  float mean_amplitude = this->get_mean_oscillation_amplitude();
  return (mean_amplitude - global_amplitude) / (global_amplitude) < 0.05f;
}

}  // namespace pid
}  // namespace esphome
