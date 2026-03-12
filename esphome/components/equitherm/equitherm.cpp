#include "equitherm.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome {
namespace equitherm {

static const char *const TAG = "equitherm";

// Central finite difference step size (°C) for numerical derivative of the heating curve.
// Used to compute curve sensitivity: f'(t) ≈ (f(t-h) - f(t+h)) / 2h.
// The denominator cancels in the gain scheduling ratio (ref/current use the same h).
// 0.5°C balances truncation error O(h²) vs floating-point noise on smooth power curves.
static constexpr float CENTRAL_DIFF_STEP = 0.5f;

// Margin (°C) from flow clamp edges where gain scheduling is disabled.
// Must be >= CENTRAL_DIFF_STEP so that neither sample (t ± step) hits the clamp,
// which would produce a false zero-derivative. 2× step provides safety margin.
static constexpr float GAIN_ACTIVE_REGION_MARGIN = 2.0f * CENTRAL_DIFF_STEP;

// Gain scheduling ratio bounds — limits how much PID correction is scaled.
// 0.2 = curve 5× steeper than reference → attenuate PID to 20%.
// 5.0 = curve 5× flatter than reference → amplify PID 5×.
// Standard adaptive control practice to prevent runaway gains.
static constexpr float GAIN_RATIO_MIN = 0.2f;
static constexpr float GAIN_RATIO_MAX = 5.0f;

// Minimum meaningful curve sensitivity (°C per °C outdoor).
// Below this the curve is effectively flat; gain scheduling ratio would explode.
static constexpr float MIN_SENSITIVITY = 0.01f;

// Hysteresis (°C) above min_flow_temp to decide HEATING vs IDLE action display.
// Cosmetic only — does not affect the setpoint sent to the boiler.
static constexpr float HEAT_ACTION_HYSTERESIS = 0.5f;

// Binary search parameters for finding reference outdoor temperature.
// 15 iterations over ~50°C range → precision ≈ 0.0015°C, well under 0.01°C.
static constexpr float BINARY_SEARCH_MIN_OUTDOOR = -30.0f;
static constexpr int BINARY_SEARCH_ITERATIONS = 15;

// Outdoor sensor validation range (°C). Readings outside this range are considered sensor failure.
// Covers all realistic climates: -40°C (extreme cold) to +50°C (extreme heat).
static constexpr float OUTDOOR_SENSOR_MIN_VALID = -40.0f;
static constexpr float OUTDOOR_SENSOR_MAX_VALID = 50.0f;

void EquithermClimate::setup() {
  // Register callback for indoor sensor updates
  this->indoor_sensor_->add_on_state_callback([this](float state) {
    // compute_and_apply_ will handle current_temperature assignment
    // Don't set it here unconditionally - let validation logic handle it
    this->compute_and_apply_();
  });

  // Register callback for outdoor sensor updates
  this->outdoor_sensor_->add_on_state_callback([this](float state) { this->compute_and_apply_(); });

  // Get initial sensor values
  if (this->indoor_sensor_->has_state()) {
    this->current_temperature = this->indoor_sensor_->state;
  }

  // Restore previous state or set defaults
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    this->mode = climate::CLIMATE_MODE_HEAT;
    this->target_temperature = this->default_target_temperature_;
    // On first boot, trigger initial calculation
    // Note: We don't require both sensors to be ready - fallback will handle missing sensors
    this->compute_and_apply_();
  }

  // Compute reference sensitivity for gain scheduling
  this->compute_reference_sensitivity_();
  this->prev_target_temperature_ = this->target_temperature;
}

void EquithermClimate::control(const climate::ClimateCall &call) {
  auto mode_opt = call.get_mode();
  if (mode_opt.has_value()) {
    auto mode = *mode_opt;
    // Only accept supported modes (OFF and HEAT)
    if (mode == climate::CLIMATE_MODE_OFF || mode == climate::CLIMATE_MODE_HEAT) {
      this->mode = mode;
    }
  }
  auto target_opt = call.get_target_temperature();
  if (target_opt.has_value()) {
    this->target_temperature = *target_opt;
  }

  this->compute_and_apply_();
}

climate::ClimateTraits EquithermClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT,
  });
  traits.set_visual_min_temperature(15.0f);
  traits.set_visual_max_temperature(25.0f);
  traits.set_visual_temperature_step(0.5f);
  return traits;
}

void EquithermClimate::dump_config() {
  LOG_CLIMATE("", "Equitherm", this);

  ESP_LOGCONFIG(TAG,
                "  Heating Curve (industry standard):\n"
                "    hc: %.2f, n: %.2f, shift: %.2f",
                heating_curve_.get_hc(), heating_curve_.get_n(), heating_curve_.get_shift());

  ESP_LOGCONFIG(TAG,
                "  PID Parameters:\n"
                "    kp: %.3f, ki: %.3f, kd: %.3f",
                pid_controller_.kp_, pid_controller_.ki_, pid_controller_.kd_);

  ESP_LOGCONFIG(TAG,
                "  Output Parameters:\n"
                "    min_flow_temp: %.1f°C, max_flow_temp: %.1f°C, smoothing_threshold: %.2f°C",
                heating_curve_.get_min_flow_temp(), heating_curve_.get_max_flow_temp(), this->smoothing_threshold_);

  ESP_LOGCONFIG(TAG,
                "  Gain Scheduling:\n"
                "    reference_sensitivity: %.4f",
                this->reference_sensitivity_);

  ESP_LOGCONFIG(TAG,
                "  Fallback (sensor failure):\n"
                "    fallback_outdoor_temp: %.1f°C",
                this->fallback_outdoor_temp_);
}

void EquithermClimate::compute_reference_sensitivity_() {
  float midpoint_flow = (this->heating_curve_.get_min_flow_temp() + this->heating_curve_.get_max_flow_temp()) / 2.0f;

  // Binary search to find outdoor temp that produces midpoint flow
  float lo = BINARY_SEARCH_MIN_OUTDOOR;
  float hi = this->target_temperature;

  for (int i = 0; i < BINARY_SEARCH_ITERATIONS; i++) {
    float mid = (lo + hi) / 2.0f;
    if (this->heating_curve_.compute_flow_temperature(this->target_temperature, mid) > midpoint_flow) {
      lo = mid;
    } else {
      hi = mid;
    }
  }

  float t_outdoor_ref = (lo + hi) / 2.0f;

  // Compute sensitivity at reference point using central difference
  float t_flow_ref_minus =
      this->heating_curve_.compute_flow_temperature(this->target_temperature, t_outdoor_ref - CENTRAL_DIFF_STEP);
  float t_flow_ref_plus =
      this->heating_curve_.compute_flow_temperature(this->target_temperature, t_outdoor_ref + CENTRAL_DIFF_STEP);
  this->reference_sensitivity_ = t_flow_ref_minus - t_flow_ref_plus;

  // Guard against zero/negative sensitivity
  this->reference_sensitivity_ = std::max(this->reference_sensitivity_, MIN_SENSITIVITY);

  ESP_LOGD(TAG, "Computed reference sensitivity: %.4f at outdoor ref: %.2f°C", this->reference_sensitivity_,
           t_outdoor_ref);
}

void EquithermClimate::write_setpoint_(float temp_c) {
  // Don't write invalid values to output
  if (std::isnan(temp_c)) {
    return;
  }

  // Defensive clamp (heating_curve also clamps, but this ensures safety)
  float clamped_flow = clamp(temp_c, heating_curve_.get_min_flow_temp(), heating_curve_.get_max_flow_temp());

#ifdef USE_NUMBER
  if (this->ch_setpoint_ != nullptr) {
    // Direct °C for OpenTherm - preferred
    this->ch_setpoint_->make_call().set_value(clamped_flow).perform();
    return;
  }
#endif

  if (this->heat_output_ != nullptr) {
    // Normalize only at boundary - never in calculation
    float flow_range = heating_curve_.get_max_flow_temp() - heating_curve_.get_min_flow_temp();
    // Guard against division by zero
    if (flow_range < 0.01f) {
      this->heat_output_->set_level(0.0f);
      return;
    }
    float normalized_level = clamp((clamped_flow - heating_curve_.get_min_flow_temp()) / flow_range, 0.0f, 1.0f);
    this->heat_output_->set_level(normalized_level);
  }
}

void EquithermClimate::write_setpoint_off_() {
#ifdef USE_NUMBER
  if (this->ch_setpoint_ != nullptr) {
    // For OpenTherm: write 0 to signal off (boiler's frost protection handles freeze prevention)
    this->ch_setpoint_->make_call().set_value(0.0f).perform();
    return;
  }
#endif

  if (this->heat_output_ != nullptr) {
    // For float output: set level to 0 (fully off)
    this->heat_output_->set_level(0.0f);
  }
}

void EquithermClimate::compute_and_apply_(bool update_pid) {
  // Target temperature must be valid
  if (std::isnan(this->target_temperature)) {
    return;
  }

  // --- OUTDOOR SENSOR HANDLING ---
  float t_outdoor;
  bool outdoor_valid = this->outdoor_sensor_->has_state() && !std::isnan(this->outdoor_sensor_->state) &&
                       this->outdoor_sensor_->state >= OUTDOOR_SENSOR_MIN_VALID &&
                       this->outdoor_sensor_->state <= OUTDOOR_SENSOR_MAX_VALID;

  if (outdoor_valid) {
    t_outdoor = this->outdoor_sensor_->state;
    this->last_valid_outdoor_temp_ = t_outdoor;
    this->last_valid_outdoor_time_ = millis();
    if (this->outdoor_fallback_active_) {
      ESP_LOGI(TAG, "Outdoor sensor recovered (%.1f°C), resuming normal operation", t_outdoor);
      this->outdoor_fallback_active_ = false;
    }
  } else {
    t_outdoor = this->fallback_outdoor_temp_;
    if (!this->outdoor_fallback_active_) {
      ESP_LOGW(TAG, "Outdoor sensor invalid, using fallback: %.1f°C", t_outdoor);
      this->outdoor_fallback_active_ = true;
    }
  }

  // --- INDOOR SENSOR HANDLING ---
  float t_indoor;
  bool indoor_valid = this->indoor_sensor_->has_state() && !std::isnan(this->indoor_sensor_->state);

  if (indoor_valid) {
    t_indoor = this->indoor_sensor_->state;
    this->current_temperature = t_indoor;  // Update display
    this->last_valid_indoor_temp_ = t_indoor;
    this->last_valid_indoor_time_ = millis();
    if (this->indoor_fallback_active_) {
      ESP_LOGI(TAG, "Indoor sensor recovered (%.1f°C), resuming PID control", t_indoor);
      this->indoor_fallback_active_ = false;
    }
  } else {
    // Indoor sensor failed - continue with pure equitherm (PID disabled)
    if (!this->indoor_fallback_active_) {
      ESP_LOGW(TAG, "Indoor sensor failed, switching to pure equitherm mode (PID disabled)");
    }
    this->indoor_fallback_active_ = true;
    // Keep last valid indoor temp for display (better than showing "unknown")
    // current_temperature unchanged - shows stale but valid value
  }

  // Recompute reference sensitivity if target temperature changed
  bool target_changed = !std::isnan(this->prev_target_temperature_) &&
                        fabsf(this->target_temperature - this->prev_target_temperature_) > 0.01f;
  if (target_changed) {
    ESP_LOGD(TAG, "Target temperature changed from %.1f°C to %.1f°C, recomputing reference sensitivity",
             this->prev_target_temperature_, this->target_temperature);
    this->compute_reference_sensitivity_();
  }
  this->prev_target_temperature_ = this->target_temperature;

  float t_flow;

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    // Reset PID integral only on transition to OFF (not every update)
    if (this->action != climate::CLIMATE_ACTION_OFF) {
      this->pid_controller_.reset_accumulated_integral();
      this->raw_pid_correction_ = 0.0f;
      // Turn off output only on transition - boiler's frost protection handles freeze prevention
      this->write_setpoint_off_();
    }
    this->action = climate::CLIMATE_ACTION_OFF;
    this->base_curve_output_ = NAN;
    this->final_flow_setpoint_ = NAN;
    this->publish_state();
    this->state_callback_.call();
    return;  // Don't write any setpoint to boiler
  }

  // Calculate base supply temperature from curve
  t_flow = heating_curve_.compute_flow_temperature(this->target_temperature, t_outdoor);

  // Store base curve output for diagnostics (before corrections)
  this->base_curve_output_ = t_flow;

  // Apply smoothing threshold on curve output
  bool change_below_threshold =
      !std::isnan(this->prev_smoothed_flow_) && fabsf(this->prev_smoothed_flow_ - t_flow) < this->smoothing_threshold_;
  if (change_below_threshold) {
    t_flow = this->prev_smoothed_flow_;
  } else {
    this->prev_smoothed_flow_ = t_flow;
  }

  // PID correction with gain scheduling (ONLY when indoor sensor is valid)
  if (indoor_valid && update_pid) {
    this->raw_pid_correction_ = this->pid_controller_.update(this->target_temperature, t_indoor);
    // Guard against nan from PID controller (can happen on first call)
    if (std::isnan(this->raw_pid_correction_)) {
      this->raw_pid_correction_ = 0.0f;
    }
  } else if (!indoor_valid) {
    // Indoor sensor failed - disable PID correction (pure equitherm mode)
    this->raw_pid_correction_ = 0.0f;
  }

  // Gain scheduling: scale PID correction by curve sensitivity ratio
  // Defaults to 1.0 (initialized in header) - used at clamp edges where sensitivity is meaningless
  bool reference_valid = !std::isnan(this->reference_sensitivity_) && this->reference_sensitivity_ > MIN_SENSITIVITY;
  bool in_active_region =
      this->base_curve_output_ > this->heating_curve_.get_min_flow_temp() + GAIN_ACTIVE_REGION_MARGIN &&
      this->base_curve_output_ < this->heating_curve_.get_max_flow_temp() - GAIN_ACTIVE_REGION_MARGIN;

  if (reference_valid && in_active_region) {
    float t_flow_minus =
        this->heating_curve_.compute_flow_temperature(this->target_temperature, t_outdoor - CENTRAL_DIFF_STEP);
    float t_flow_plus =
        this->heating_curve_.compute_flow_temperature(this->target_temperature, t_outdoor + CENTRAL_DIFF_STEP);
    float local_sensitivity = std::max(t_flow_minus - t_flow_plus, MIN_SENSITIVITY);
    this->gain_scheduling_ratio_ =
        std::clamp(this->reference_sensitivity_ / local_sensitivity, GAIN_RATIO_MIN, GAIN_RATIO_MAX);
  }

  float scaled_correction = this->raw_pid_correction_ * this->gain_scheduling_ratio_;
  if (this->gain_scheduling_ratio_ != 1.0f) {
    ESP_LOGD(TAG, "Gain scheduling: pid=%.2f°C, ratio=%.2f, scaled=%.2f°C", this->raw_pid_correction_,
             this->gain_scheduling_ratio_, scaled_correction);
  }
  t_flow += scaled_correction;

  // Set action based on whether we're actually calling for heat
  bool calling_for_heat = t_flow > heating_curve_.get_min_flow_temp() + HEAT_ACTION_HYSTERESIS;
  this->action = calling_for_heat ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;

  // Final guard: never write nan to output
  if (std::isnan(t_flow)) {
    return;
  }

  this->final_flow_setpoint_ = t_flow;
  this->write_setpoint_(t_flow);
  this->publish_state();
  this->state_callback_.call();
}

}  // namespace equitherm
}  // namespace esphome
