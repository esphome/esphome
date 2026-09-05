#include "equitherm.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome::equitherm {

static const char *const TAG = "equitherm";

// Outdoor sensor validation range (°C). Readings outside this range are considered sensor failure.
// Covers all realistic climates: --55°C (extreme cold) to +50°C (extreme heat).
static constexpr float OUTDOOR_SENSOR_MIN_VALID = -55.0f;
static constexpr float OUTDOOR_SENSOR_MAX_VALID = 50.0f;

void EquithermClimate::setup() {
  // Register callback for indoor sensor updates — recompute on every update so the climate
  // reflects the latest reading (the validity check itself lives in compute_and_apply_()).
  this->indoor_sensor_->add_on_state_callback([this](float) { this->compute_and_apply_(); });

  // Register callback for outdoor sensor updates — same rationale.
  this->outdoor_sensor_->add_on_state_callback([this](float) { this->compute_and_apply_(); });

  // Restore previous state or set defaults
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    this->mode = climate::CLIMATE_MODE_HEAT;
    this->target_temperature = this->default_target_temperature_;
    // On first boot, trigger initial calculation
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

  if (call.has_custom_preset()) {
    this->set_custom_preset_(call.get_custom_preset());
    ESP_LOGI(TAG, "Preset changed to 'manual' — bypassing heating curve");
  } else {
    auto preset_opt = call.get_preset();
    if (preset_opt.has_value() && *preset_opt == climate::CLIMATE_PRESET_NONE) {
      this->clear_custom_preset_();
      ESP_LOGI(TAG, "Preset cleared — resuming normal equitherm calculation");
    }
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
  if (this->manual_flow_temp_ != nullptr) {
    traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
    traits.set_supported_custom_presets({"Manual"});
  }
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
                "    min_flow_temp: %.1f°C, max_flow_temp: %.1f°C",
                heating_curve_.get_min_flow_temp(), heating_curve_.get_max_flow_temp());
}

void EquithermClimate::write_setpoint_(float temp_c) {
  // Don't write invalid values to output
  if (std::isnan(temp_c)) {
    ESP_LOGW(TAG, "write_setpoint_ called with NAN, ignoring");
    return;
  }

  // Curve returns 0.0f in WWS — that's a stop signal, not a temperature.
  // Without this guard, clamp(0, min_flow_temp, max_flow_temp) would
  // raise 0 to min_flow_temp and produce unwanted heat output.
  if (temp_c == 0.0f) {
    this->write_setpoint_off_();
    return;
  }

  // Defensive clamp (heating_curve also clamps, but this ensures safety)
  float clamped_flow = clamp(temp_c, heating_curve_.get_min_flow_temp(), heating_curve_.get_max_flow_temp());

  ESP_LOGD(TAG, "write_setpoint_: %.1f°C (clamped: %.1f°C)", temp_c, clamped_flow);

#ifdef USE_NUMBER
  if (this->flow_setpoint_output_ != nullptr) {
    // Direct °C for OpenTherm - preferred
    ESP_LOGD(TAG, "Writing to flow_setpoint_output: %.1f°C", clamped_flow);
    this->flow_setpoint_output_->make_call().set_value(clamped_flow).perform();
    return;
  }
#endif

#ifdef USE_EQUITHERM_HEAT_OUTPUT
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
#endif

  ESP_LOGW(TAG, "write_setpoint_: No output configured (flow_setpoint and heat_output are both null)");
}

void EquithermClimate::write_setpoint_off_() {
  // For flow_setpoint_ (number): no action. There is no universal "off" value for a number —
  // min_value is still a heat request, not an off signal. Use on_heating_stop to do
  // whatever your hardware requires (turn off ch_enable, send MQTT, etc.).

#ifdef USE_EQUITHERM_HEAT_OUTPUT
  if (this->heat_output_ != nullptr) {
    // For normalized float output: 0.0 is universally correct — no signal, no heat.
    // Built-in so users get safe default behavior without needing on_heating_stop.
    this->heat_output_->set_level(0.0f);
  }
#endif
}

void EquithermClimate::compute_and_apply_(bool update_pid) {
  // Target temperature must be valid
  if (std::isnan(this->target_temperature)) {
    this->state_callback_.call();
    return;
  }

  // --- OUTDOOR SENSOR HANDLING ---
  // Straight read: if the outdoor reading is present, finite and in range use it; otherwise
  // mark it invalid. With no configured substitute in core, an invalid outdoor sensor skips
  // recalculation and holds the last setpoint (see guard below). No fault flag here.
  bool outdoor_valid = this->outdoor_sensor_->has_state() && !std::isnan(this->outdoor_sensor_->state) &&
                       this->outdoor_sensor_->state >= OUTDOOR_SENSOR_MIN_VALID &&
                       this->outdoor_sensor_->state <= OUTDOOR_SENSOR_MAX_VALID;

  // --- INDOOR SENSOR HANDLING ---
  // Straight read: t_indoor is the current indoor state when present and finite, else NAN.
  // No range check (the transient filter is gone). current_temperature is published only
  // when the reading is not NAN so HA does not show a garbage value.
  bool indoor_valid = this->indoor_sensor_->has_state() && !std::isnan(this->indoor_sensor_->state);
  float t_indoor = indoor_valid ? this->indoor_sensor_->state : NAN;

  if (!std::isnan(t_indoor)) {
    this->current_temperature = t_indoor;
  }

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    // Reset PID integral only on transition to OFF (not every update)
    if (this->action != climate::CLIMATE_ACTION_OFF) {
      this->pid_controller_.reset_accumulated_integral();
      this->pid_correction_ = 0.0f;
      // Turn off output only on transition - boiler's frost protection handles freeze prevention
      this->write_setpoint_off_();
      // Fire stop callback only if we were actually heating
      if (this->action == climate::CLIMATE_ACTION_HEATING) {
        this->on_heating_stop_callback_.call();
      }
    }
    this->action = climate::CLIMATE_ACTION_OFF;
    this->prev_action_ = climate::CLIMATE_ACTION_OFF;
    this->heating_curve_output_ = NAN;
    this->pid_adjusted_output_ = NAN;
    this->wws_active_ = false;
    this->flow_setpoint_ = NAN;
    this->active_setpoint_ = NAN;
    this->publish_state();
    this->state_callback_.call();
    return;  // Don't write any setpoint to boiler
  }

  // --- MANUAL PRESET OR NORMAL EQUITHERM ---
  bool manual_active = false;
#ifdef USE_NUMBER
  manual_active = this->has_custom_preset() && strcmp(this->get_custom_preset().c_str(), "Manual") == 0 &&
                  this->manual_flow_temp_ != nullptr && this->manual_flow_temp_->has_state() &&
                  !std::isnan(this->manual_flow_temp_->state);
#endif

  // Without a configured substitute, an invalid outdoor reading makes the curve incalculable.
  // In normal (non-manual, non-OFF) mode we hold the last setpoint rather than drive the
  // boiler from a garbage reading. Manual mode bypasses the curve entirely.
  if (!manual_active && !outdoor_valid) {
    ESP_LOGD(TAG, "Outdoor sensor invalid, holding last setpoint");
    this->publish_state();
    this->state_callback_.call();
    return;
  }

  float t_flow;

  if (manual_active) {
#ifdef USE_NUMBER
    // Bypass curve and PID: use manual flow temperature directly
    t_flow = this->manual_flow_temp_->state;
    ESP_LOGD(TAG, "Manual preset active: using manual flow temp %.1f°C", t_flow);
    this->wws_active_ = false;
    this->heating_curve_output_ = t_flow;
    this->pid_correction_ = 0.0f;
    this->pid_adjusted_output_ = t_flow;
#endif
  } else {
    // Normal equitherm curve (outdoor_valid is guaranteed true here)
    float t_outdoor = this->outdoor_sensor_->state;
    // WWS decision is owned by the controller, not the curve calculator.
    // This keeps HeatingCurve stateless and allows the formula to change independently.
    this->wws_active_ = (this->target_temperature - t_outdoor) <= 0.0f;

    t_flow = heating_curve_.compute_flow_temperature(this->target_temperature, t_outdoor);

    // Clamp to [min, max] only when there's positive heating demand
    if (!this->wws_active_) {
      t_flow = std::clamp(t_flow, heating_curve_.get_min_flow_temp(), heating_curve_.get_max_flow_temp());
    }

    // Store raw curve output for diagnostics (before PID)
    this->heating_curve_output_ = t_flow;

    // PID correction (ONLY when indoor sensor is valid)
    if (indoor_valid && update_pid) {
      this->pid_correction_ = this->pid_controller_.update(this->target_temperature, t_indoor);
      // Guard against nan from PID controller (can happen on first call)
      if (std::isnan(this->pid_correction_)) {
        this->pid_correction_ = 0.0f;
      }
    } else {
      // Indoor sensor invalid - disable PID correction (pure equitherm mode)
      this->pid_correction_ = 0.0f;
    }

    // Apply PID correction to flow temperature
    t_flow += this->pid_correction_;

    // Store setpoint for diagnostics (curve + PID)
    this->pid_adjusted_output_ = t_flow;
  }

  // Set action based on heating demand
  // Manual mode always calls for heat. In normal mode, WWS = off (no demand).
  // Any positive demand (delta_t > 0) = heating, even when clamped to minimum flow temp.
  bool calling_for_heat = manual_active || !this->wws_active_;
  climate::ClimateAction new_action = calling_for_heat ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;

  if (new_action != this->prev_action_) {
    if (new_action == climate::CLIMATE_ACTION_HEATING) {
      this->on_heating_start_callback_.call();
    } else if (this->prev_action_ == climate::CLIMATE_ACTION_HEATING) {
      this->on_heating_stop_callback_.call();
    }
    this->prev_action_ = new_action;
  }
  this->action = new_action;

  // WWS: don't send a setpoint to the boiler (only in normal mode — manual mode bypasses curve)
  if (!manual_active && this->wws_active_) {
    this->flow_setpoint_ = 0.0f;
    if (std::isnan(this->active_setpoint_) || this->active_setpoint_ != 0.0f) {
      this->active_setpoint_ = 0.0f;
      this->write_setpoint_off_();
    }
    this->publish_state();
    this->state_callback_.call();
    return;
  }

  // Final guard: never write nan to output
  if (std::isnan(t_flow)) {
    this->state_callback_.call();
    return;
  }

  // Round to 0.1°C precision before write deadband check.
  // Matches real thermostat behavior and eliminates sub-0.1°C noise.
  t_flow = roundf(t_flow * 10.0f) / 10.0f;

  // Store the rounded value for diagnostics
  this->flow_setpoint_ = t_flow;

  // Only write to output if setpoint actually changed (avoid spamming boiler)
  // Uses half-step threshold so any 0.1°C change triggers a write
  bool setpoint_changed =
      std::isnan(this->active_setpoint_) || fabsf(t_flow - this->active_setpoint_) > this->write_deadband_;

  ESP_LOGD(TAG, "setpoint check: t_flow=%.1f°C, active_setpoint=%.1f°C, delta=%.2f°C, changed=%s", t_flow,
           this->active_setpoint_, fabsf(t_flow - this->active_setpoint_), setpoint_changed ? "YES" : "NO");

  if (setpoint_changed) {
    this->active_setpoint_ = t_flow;
    this->write_setpoint_(t_flow);
  }
  this->publish_state();
  this->state_callback_.call();
}

}  // namespace esphome::equitherm
