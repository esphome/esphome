#include "equitherm.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome {
namespace equitherm {

static const char *const TAG = "equitherm";

// Outdoor sensor validation range (°C). Readings outside this range are considered sensor failure.
// Covers all realistic climates: --55°C (extreme cold) to +50°C (extreme heat).
static constexpr float OUTDOOR_SENSOR_MIN_VALID = -55.0f;
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
    float new_target = *target_opt;
    // Reset PID integral on setpoint change to prevent overshoot from old error context
    if (new_target != this->target_temperature) {
      ESP_LOGD(TAG, "Target temperature changed: %.1f°C → %.1f°C, resetting PID integral", this->target_temperature,
               new_target);
      this->pid_controller_.reset_accumulated_integral();
      this->pid_correction_ = 0.0f;
    }
    this->target_temperature = new_target;
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
                "    min_flow_temp: %.1f°C, max_flow_temp: %.1f°C\n"
                "    rate_limit_per_minute: %.2f°C/min",
                heating_curve_.get_min_flow_temp(), heating_curve_.get_max_flow_temp(), this->rate_limit_per_minute_);

  ESP_LOGCONFIG(TAG,
                "  Fallback (sensor failure):\n"
                "    fallback_outdoor_temp: %.1f°C\n"
                "    sensor_stale_timeout: %.1f min",
                this->fallback_outdoor_temp_, this->sensor_stale_timeout_ms_ / 60000.0f);
}

void EquithermClimate::write_setpoint_(float temp_c) {
  // Don't write invalid values to output
  if (std::isnan(temp_c)) {
    ESP_LOGW(TAG, "write_setpoint_ called with NAN, ignoring");
    return;
  }

  // Defensive clamp (heating_curve also clamps, but this ensures safety)
  float clamped_flow = clamp(temp_c, heating_curve_.get_min_flow_temp(), heating_curve_.get_max_flow_temp());

  ESP_LOGD(TAG, "write_setpoint_: %.1f°C (clamped: %.1f°C)", temp_c, clamped_flow);

#ifdef USE_NUMBER
  if (this->ch_setpoint_ != nullptr) {
    // Direct °C for OpenTherm - preferred
    ESP_LOGD(TAG, "Writing to ch_setpoint: %.1f°C", clamped_flow);
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
    ESP_LOGD(TAG, "Writing to heat_output: %.2f (normalized)", normalized_level);
    this->heat_output_->set_level(normalized_level);
    return;
  }

  ESP_LOGW(TAG, "write_setpoint_: No output configured (ch_setpoint and heat_output are both null)");
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
    // Reset rate limiting state and notify sensors before early return
    this->rate_limiting_active_ = false;
    this->state_callback_.call();
    return;
  }

  // --- OUTDOOR SENSOR HANDLING ---
  float t_outdoor;
  uint32_t now = millis();
  bool outdoor_reading_valid = this->outdoor_sensor_->has_state() && !std::isnan(this->outdoor_sensor_->state) &&
                               this->outdoor_sensor_->state >= OUTDOOR_SENSOR_MIN_VALID &&
                               this->outdoor_sensor_->state <= OUTDOOR_SENSOR_MAX_VALID;

  // Check for stale data (no update within timeout window)
  // Note: Unsigned subtraction handles millis() rollover correctly
  bool outdoor_stale = (now - this->last_valid_outdoor_time_) > this->sensor_stale_timeout_ms_;

  bool outdoor_valid = outdoor_reading_valid && !outdoor_stale;

  if (outdoor_valid) {
    t_outdoor = this->outdoor_sensor_->state;
    this->last_valid_outdoor_temp_ = t_outdoor;
    this->last_valid_outdoor_time_ = now;
    if (this->outdoor_fallback_active_) {
      ESP_LOGI(TAG, "Outdoor sensor recovered (%.1f°C), resuming normal operation", t_outdoor);
      this->outdoor_fallback_active_ = false;
      // Reset rate limiter to avoid clamping from fallback-based calculation
      this->prev_rate_limited_flow_ = NAN;
    }
  } else {
    t_outdoor = this->fallback_outdoor_temp_;
    if (!this->outdoor_fallback_active_) {
      if (outdoor_stale) {
        ESP_LOGW(TAG, "Outdoor sensor stale (no update for %.1f min), using fallback: %.1f°C",
                 (now - this->last_valid_outdoor_time_) / 60000.0f, t_outdoor);
      } else {
        ESP_LOGW(TAG, "Outdoor sensor invalid, using fallback: %.1f°C", t_outdoor);
      }
      this->outdoor_fallback_active_ = true;
      // Reset rate limiter to avoid clamping from sensor-based calculation
      this->prev_rate_limited_flow_ = NAN;
    }
  }

  // --- INDOOR SENSOR HANDLING ---
  float t_indoor;
  bool indoor_reading_valid = this->indoor_sensor_->has_state() && !std::isnan(this->indoor_sensor_->state);

  // Check for stale data (no update within timeout window)
  // Note: Unsigned subtraction handles millis() rollover correctly
  bool indoor_stale = (now - this->last_valid_indoor_time_) > this->sensor_stale_timeout_ms_;

  bool indoor_valid = indoor_reading_valid && !indoor_stale;

  if (indoor_valid) {
    t_indoor = this->indoor_sensor_->state;
    this->current_temperature = t_indoor;  // Update display
    this->last_valid_indoor_temp_ = t_indoor;
    this->last_valid_indoor_time_ = now;
    if (this->indoor_fallback_active_) {
      ESP_LOGI(TAG, "Indoor sensor recovered (%.1f°C), resuming PID control", t_indoor);
      this->indoor_fallback_active_ = false;
    }
  } else {
    // Indoor sensor failed - continue with pure equitherm (PID disabled)
    if (!this->indoor_fallback_active_) {
      if (indoor_stale) {
        ESP_LOGW(TAG, "Indoor sensor stale (no update for %.1f min), switching to pure equitherm mode",
                 (now - this->last_valid_indoor_time_) / 60000.0f);
      } else {
        ESP_LOGW(TAG, "Indoor sensor failed, switching to pure equitherm mode (PID disabled)");
      }
    }
    this->indoor_fallback_active_ = true;
    // Keep last valid indoor temp for display (better than showing "unknown")
    // current_temperature unchanged - shows stale but valid value
  }

  float t_flow;

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    // Reset PID integral only on transition to OFF (not every update)
    if (this->action != climate::CLIMATE_ACTION_OFF) {
      this->pid_controller_.reset_accumulated_integral();
      this->pid_correction_ = 0.0f;
      // Turn off output only on transition - boiler's frost protection handles freeze prevention
      this->write_setpoint_off_();
    }
    this->action = climate::CLIMATE_ACTION_OFF;
    this->curve_output_raw_ = NAN;
    this->base_curve_output_ = NAN;
    this->rate_limiting_active_ = false;
    this->final_flow_setpoint_ = NAN;
    this->last_written_setpoint_ = NAN;
    // Reset rate limiter state to avoid issues on next HEAT transition
    this->prev_rate_limited_flow_ = NAN;
    this->last_rate_limit_time_ = millis();
    this->publish_state();
    this->state_callback_.call();
    return;  // Don't write any setpoint to boiler
  }

  // Calculate base supply temperature from curve
  t_flow = heating_curve_.compute_flow_temperature(this->target_temperature, t_outdoor);

  // Store raw curve output for diagnostics (before PID and rate limiting)
  this->curve_output_raw_ = t_flow;

  // PID correction (ONLY when indoor sensor is valid)
  // Calculate PID BEFORE rate limiting so the combined output is rate-limited together
  if (indoor_valid && update_pid) {
    this->pid_correction_ = this->pid_controller_.update(this->target_temperature, t_indoor);
    // Guard against nan from PID controller (can happen on first call)
    if (std::isnan(this->pid_correction_)) {
      this->pid_correction_ = 0.0f;
    }
  } else if (!indoor_valid) {
    // Indoor sensor failed - disable PID correction (pure equitherm mode)
    this->pid_correction_ = 0.0f;
  }

  // Apply PID correction to flow temperature BEFORE rate limiting
  t_flow += this->pid_correction_;

  // Store base setpoint for diagnostics (curve + PID, before rate limiting)
  this->base_curve_output_ = t_flow;

  // Apply rate limiting (°C per minute) to the combined output (curve + PID)
  // This ensures PID corrections are also subject to rate limiting
  uint32_t elapsed_ms = now - this->last_rate_limit_time_;

  // First valid calculation - initialize without limiting
  if (std::isnan(this->prev_rate_limited_flow_)) {
    ESP_LOGI(TAG, "Initializing rate limiter: starting at %.1f°C", t_flow);
    this->prev_rate_limited_flow_ = t_flow;
    this->rate_limiting_active_ = false;
  }
  // Subsequent runs: apply rate limiting
  // Guard against sub-millisecond elapsed time to prevent floating-point noise
  else if (elapsed_ms > 0 && this->rate_limit_per_minute_ > 0.0f) {
    float elapsed_minutes = elapsed_ms / 60000.0f;
    float max_change = this->rate_limit_per_minute_ * elapsed_minutes;
    float delta = t_flow - this->prev_rate_limited_flow_;

    if (fabsf(delta) > max_change) {
      delta = (delta > 0) ? max_change : -max_change;
      t_flow = this->prev_rate_limited_flow_ + delta;
      this->rate_limiting_active_ = true;
      ESP_LOGD(TAG, "Rate limiting: raw=%.1f°C, limited=%.1f°C, remaining=%.1f°C, limit=%.3f°C/min",
               this->base_curve_output_, t_flow, fabsf(this->base_curve_output_ - t_flow),
               this->rate_limit_per_minute_);
    } else {
      this->rate_limiting_active_ = false;
      ESP_LOGVV(TAG, "No rate limiting: delta=%.3f°C <= max_change=%.3f°C", fabsf(delta), max_change);
    }
  }
  // Same millisecond or rate limiting disabled
  else {
    this->rate_limiting_active_ = false;
  }

  this->prev_rate_limited_flow_ = t_flow;
  this->last_rate_limit_time_ = now;

  // Set action based on whether we're actually calling for heat
  bool calling_for_heat = t_flow > heating_curve_.get_min_flow_temp() + this->action_hysteresis_;
  this->action = calling_for_heat ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;

  // Final guard: never write nan to output
  if (std::isnan(t_flow)) {
    // Reset rate limiting state and notify sensors before early return
    this->rate_limiting_active_ = false;
    this->state_callback_.call();
    return;
  }

  // Round to 0.1°C precision before write deadband check.
  // Matches real thermostat behavior and eliminates sub-0.1°C noise.
  t_flow = roundf(t_flow * 10.0f) / 10.0f;

  // Store the rounded value for diagnostics
  this->final_flow_setpoint_ = t_flow;

  // Only write to output if setpoint actually changed (avoid spamming boiler)
  // Uses half-step threshold so any 0.1°C change triggers a write
  bool setpoint_changed =
      std::isnan(this->last_written_setpoint_) || fabsf(t_flow - this->last_written_setpoint_) > this->write_deadband_;

  ESP_LOGD(TAG, "setpoint check: t_flow=%.1f°C, last_written=%.1f°C, delta=%.2f°C, changed=%s", t_flow,
           this->last_written_setpoint_, fabsf(t_flow - this->last_written_setpoint_), setpoint_changed ? "YES" : "NO");

  if (setpoint_changed) {
    this->last_written_setpoint_ = t_flow;
    this->write_setpoint_(t_flow);
  }
  this->publish_state();
  this->state_callback_.call();
}

}  // namespace equitherm
}  // namespace esphome
