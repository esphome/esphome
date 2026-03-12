#include "equitherm_climate.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome {
namespace equitherm_climate {

static const char *const TAG = "equitherm_climate";
static constexpr uint32_t UPDATE_DEBOUNCE_MS = 100;  // Minimum time between updates

void EquithermClimate::setup() {
  // Register callback for indoor sensor updates
  this->indoor_sensor_->add_on_state_callback([this](float state) {
    this->current_temperature = state;
    this->schedule_update_();
  });

  // Register callback for outdoor sensor updates
  this->outdoor_sensor_->add_on_state_callback([this](float state) { this->schedule_update_(); });

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
    // On first boot, trigger initial calculation if sensors are ready
    if (this->outdoor_sensor_->has_state() && this->indoor_sensor_->has_state()) {
      this->update_equitherm_();
    }
  }
}

void EquithermClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
  }
  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
  }

  this->update_equitherm_();
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
  LOG_CLIMATE("", "Equitherm Climate", this);

  bool pid_active = this->pid_controller_.is_active();

  ESP_LOGCONFIG(TAG,
                "  Control Parameters:\n"
                "    slope: %.2f, exponent: %.2f, shift: %.2f",
                controller_.get_slope(), controller_.get_exponent(), controller_.get_shift());

  if (pid_active) {
    ESP_LOGCONFIG(TAG,
                  "  Room Correction: PID mode\n"
                  "    kp: %.3f, ki: %.3f, kd: %.3f",
                  pid_controller_.kp_, pid_controller_.ki_, pid_controller_.kd_);
    // Note: target_diff_factor is ignored when PID is active
    if (this->target_diff_factor_ != 0.0f) {
      ESP_LOGW(TAG, "  Note: target_diff_factor (%.2f) ignored - PID takes precedence", this->target_diff_factor_);
    }
  } else {
    ESP_LOGCONFIG(TAG,
                  "  Room Correction: Linear mode\n"
                  "    target_diff_factor: %.2f, room_error_clamp: %.1f°C",
                  this->target_diff_factor_, this->room_error_clamp_);
  }

  ESP_LOGCONFIG(TAG,
                "  Output Parameters:\n"
                "    t_min_flow: %.1f°C, t_max_flow: %.1f°C, smoothing_threshold: %.2f°C",
                controller_.get_t_min_flow(), controller_.get_t_max_flow(), this->smoothing_threshold_);
}

void EquithermClimate::write_setpoint_(float temp_c) {
  // Don't write invalid values to output
  if (std::isnan(temp_c)) {
    return;
  }

  // Defensive clamp (controller also clamps, but this ensures safety)
  float safe = clamp(temp_c, controller_.get_t_min_flow(), controller_.get_t_max_flow());

#ifdef USE_NUMBER
  if (this->ch_setpoint_ != nullptr) {
    // Direct °C for OpenTherm - preferred
    this->ch_setpoint_->make_call().set_value(safe).perform();
    return;
  }
#endif

  if (this->heat_output_ != nullptr) {
    // Normalize only at boundary - never in calculation
    float range = controller_.get_t_max_flow() - controller_.get_t_min_flow();
    // Guard against division by zero
    if (range < 0.01f) {
      this->heat_output_->set_level(0.0f);
      return;
    }
    float level = clamp((safe - controller_.get_t_min_flow()) / range, 0.0f, 1.0f);
    this->heat_output_->set_level(level);
  }
}

void EquithermClimate::schedule_update_() {
  // Debounce sensor updates - defer instead of drop
  uint32_t now = millis();
  uint32_t time_since_last = now - this->last_update_ms_;

  if (time_since_last >= UPDATE_DEBOUNCE_MS) {
    // Enough time has passed, process immediately
    this->last_update_ms_ = now;
    this->deferred_update_pending_ = false;
    this->update_equitherm_();
  } else if (!this->deferred_update_pending_) {
    // Within debounce window - defer the update instead of dropping it
    this->deferred_update_pending_ = true;
    uint32_t delay = UPDATE_DEBOUNCE_MS - time_since_last;
    this->set_timeout(delay, [this]() {
      this->deferred_update_pending_ = false;
      this->last_update_ms_ = millis();
      this->update_equitherm_();
    });
  }
  // If deferred update is already pending, do nothing - it will pick up latest values
}

void EquithermClimate::update_equitherm_() {
  // Validate sensors have data
  if (!this->outdoor_sensor_->has_state() || !this->indoor_sensor_->has_state()) {
    return;
  }

  if (std::isnan(this->target_temperature)) {
    return;
  }

  float t_outdoor = this->outdoor_sensor_->state;
  float t_indoor = this->indoor_sensor_->state;

  // Validate sensor values are not nan
  if (std::isnan(t_outdoor) || std::isnan(t_indoor)) {
    return;
  }

  float t_flow;

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    // Reset PID integral only on transition to OFF (not every update)
    if (this->action != climate::CLIMATE_ACTION_OFF) {
      this->pid_controller_.reset_accumulated_integral();
      this->pid_correction_ = 0.0f;
    }
    // In OFF mode, write minimum flow temp (not 0!)
    t_flow = controller_.get_t_min_flow();
    this->action = climate::CLIMATE_ACTION_OFF;
    this->flow_curve_ = t_flow;
  } else {
    // Calculate base supply temperature from curve
    t_flow = controller_.compute(this->target_temperature, t_outdoor);

    // Store base curve output for diagnostics (before corrections)
    this->flow_curve_ = t_flow;

    // Room correction (target_diff_factor) - only when PID is disabled
    // PID provides its own room error correction, so these are mutually exclusive
    bool pid_active = this->pid_controller_.is_active();

    // Apply smoothing threshold BEFORE corrections - only in linear mode
    // When PID is active, let it handle the response without interference
    if (!pid_active && !std::isnan(this->prev_et_result_) &&
        fabsf(this->prev_et_result_ - t_flow) < this->smoothing_threshold_) {
      t_flow = this->prev_et_result_;
    } else {
      this->prev_et_result_ = t_flow;
    }

    if (!pid_active && this->target_diff_factor_ != 0.0f && !std::isnan(t_indoor)) {
      float room_error = clamp(this->target_temperature - t_indoor, -this->room_error_clamp_, this->room_error_clamp_);
      float correction = room_error * this->target_diff_factor_;
      t_flow += correction;
    }

    // PID correction
    if (pid_active && !std::isnan(t_indoor)) {
      this->pid_correction_ = this->pid_controller_.update(this->target_temperature, t_indoor);
      // Guard against nan from PID controller (can happen on first call)
      if (std::isnan(this->pid_correction_)) {
        this->pid_correction_ = 0.0f;
      }
      t_flow += this->pid_correction_;
    } else {
      this->pid_correction_ = 0.0f;
    }

    // Set action based on whether we're actually calling for heat
    this->action =
        (t_flow > controller_.get_t_min_flow() + 0.5f) ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;
  }

  // Final guard: never write nan to output
  if (std::isnan(t_flow)) {
    return;
  }

  this->current_setpoint_ = t_flow;
  this->write_setpoint_(t_flow);
  this->publish_state();
  this->state_callback_.call();
}

}  // namespace equitherm_climate
}  // namespace esphome
