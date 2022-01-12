#include "thermostat_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace thermostat {

static const char *const TAG = "thermostat.climate";

void ThermostatClimate::setup() {
  if (this->use_startup_delay_) {
    // start timers so that no actions are called for a moment
    this->start_timer_(thermostat::TIMER_COOLING_OFF);
    this->start_timer_(thermostat::TIMER_FANNING_OFF);
    this->start_timer_(thermostat::TIMER_HEATING_OFF);
    if (this->supports_fan_only_action_uses_fan_mode_timer_)
      this->start_timer_(thermostat::TIMER_FAN_MODE);
  }
  // add a callback so that whenever the sensor state changes we can take action
  this->sensor_->add_on_state_callback([this](float state) {
    this->current_temperature = state;
    // required action may have changed, recompute, refresh, we'll publish_state() later
    this->switch_to_action_(this->compute_action_(), false);
    this->switch_to_supplemental_action_(this->compute_supplemental_action_());
    // current temperature and possibly action changed, so publish the new state
    this->publish_state();
  });
  this->current_temperature = this->sensor_->state;
  // restore all climate data, if possible
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    // restore from defaults, change_away handles temps for us
    this->mode = this->default_mode_;
    this->change_away_(false);
  }
  // refresh the climate action based on the restored settings, we'll publish_state() later
  this->switch_to_action_(this->compute_action_(), false);
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
  this->setup_complete_ = true;
  this->publish_state();
}

float ThermostatClimate::cool_deadband() { return this->cooling_deadband_; }
float ThermostatClimate::cool_overrun() { return this->cooling_overrun_; }
float ThermostatClimate::heat_deadband() { return this->heating_deadband_; }
float ThermostatClimate::heat_overrun() { return this->heating_overrun_; }

void ThermostatClimate::refresh() {
  this->switch_to_mode_(this->mode, false);
  this->switch_to_action_(this->compute_action_(), false);
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
  this->switch_to_fan_mode_(this->fan_mode.value(), false);
  this->switch_to_swing_mode_(this->swing_mode, false);
  this->check_temperature_change_trigger_();
  this->publish_state();
}

bool ThermostatClimate::climate_action_change_delayed() {
  bool state_mismatch = this->action != this->compute_action_(true);

  switch (this->compute_action_(true)) {
    case climate::CLIMATE_ACTION_OFF:
    case climate::CLIMATE_ACTION_IDLE:
      return state_mismatch && (!this->idle_action_ready_());
    case climate::CLIMATE_ACTION_COOLING:
      return state_mismatch && (!this->cooling_action_ready_());
    case climate::CLIMATE_ACTION_HEATING:
      return state_mismatch && (!this->heating_action_ready_());
    case climate::CLIMATE_ACTION_FAN:
      return state_mismatch && (!this->fanning_action_ready_());
    case climate::CLIMATE_ACTION_DRYING:
      return state_mismatch && (!this->drying_action_ready_());
    default:
      break;
  }
  return false;
}

bool ThermostatClimate::fan_mode_change_delayed() {
  bool state_mismatch = this->fan_mode.value_or(climate::CLIMATE_FAN_ON) != this->prev_fan_mode_;
  return state_mismatch && (!this->fan_mode_ready_());
}

climate::ClimateAction ThermostatClimate::delayed_climate_action() { return this->compute_action_(true); }

climate::ClimateFanMode ThermostatClimate::locked_fan_mode() { return this->prev_fan_mode_; }

bool ThermostatClimate::hysteresis_valid() {
  if ((this->supports_cool_ || (this->supports_fan_only_ && this->supports_fan_only_cooling_)) &&
      (std::isnan(this->cooling_deadband_) || std::isnan(this->cooling_overrun_)))
    return false;

  if (this->supports_heat_ && (std::isnan(this->heating_deadband_) || std::isnan(this->heating_overrun_)))
    return false;

  return true;
}

void ThermostatClimate::validate_target_temperature() {
  if (std::isnan(this->target_temperature)) {
    this->target_temperature =
        ((this->get_traits().get_visual_max_temperature() - this->get_traits().get_visual_min_temperature()) / 2) +
        this->get_traits().get_visual_min_temperature();
  } else {
    // target_temperature must be between the visual minimum and the visual maximum
    if (this->target_temperature < this->get_traits().get_visual_min_temperature())
      this->target_temperature = this->get_traits().get_visual_min_temperature();
    if (this->target_temperature > this->get_traits().get_visual_max_temperature())
      this->target_temperature = this->get_traits().get_visual_max_temperature();
  }
}

void ThermostatClimate::validate_target_temperatures() {
  if (this->supports_two_points_) {
    this->validate_target_temperature_low();
    this->validate_target_temperature_high();
  } else {
    this->validate_target_temperature();
  }
}

void ThermostatClimate::validate_target_temperature_low() {
  if (std::isnan(this->target_temperature_low)) {
    this->target_temperature_low = this->get_traits().get_visual_min_temperature();
  } else {
    // target_temperature_low must not be lower than the visual minimum
    if (this->target_temperature_low < this->get_traits().get_visual_min_temperature())
      this->target_temperature_low = this->get_traits().get_visual_min_temperature();
    // target_temperature_low must not be greater than the visual maximum minus set_point_minimum_differential_
    if (this->target_temperature_low >
        this->get_traits().get_visual_max_temperature() - this->set_point_minimum_differential_)
      this->target_temperature_low =
          this->get_traits().get_visual_max_temperature() - this->set_point_minimum_differential_;
    // if target_temperature_low is set greater than target_temperature_high, move up target_temperature_high
    if (this->target_temperature_low > this->target_temperature_high - this->set_point_minimum_differential_)
      this->target_temperature_high = this->target_temperature_low + this->set_point_minimum_differential_;
  }
}

void ThermostatClimate::validate_target_temperature_high() {
  if (std::isnan(this->target_temperature_high)) {
    this->target_temperature_high = this->get_traits().get_visual_max_temperature();
  } else {
    // target_temperature_high must not be lower than the visual maximum
    if (this->target_temperature_high > this->get_traits().get_visual_max_temperature())
      this->target_temperature_high = this->get_traits().get_visual_max_temperature();
    // target_temperature_high must not be lower than the visual minimum plus set_point_minimum_differential_
    if (this->target_temperature_high <
        this->get_traits().get_visual_min_temperature() + this->set_point_minimum_differential_)
      this->target_temperature_high =
          this->get_traits().get_visual_min_temperature() + this->set_point_minimum_differential_;
    // if target_temperature_high is set less than target_temperature_low, move down target_temperature_low
    if (this->target_temperature_high < this->target_temperature_low + this->set_point_minimum_differential_)
      this->target_temperature_low = this->target_temperature_high - this->set_point_minimum_differential_;
  }
}

void ThermostatClimate::control(const climate::ClimateCall &call) {
  if (call.get_preset().has_value()) {
    // setup_complete_ blocks modifying/resetting the temps immediately after boot
    if (this->setup_complete_) {
      this->change_away_(*call.get_preset() == climate::CLIMATE_PRESET_AWAY);
    } else {
      this->preset = *call.get_preset();
    }
  }
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_fan_mode().has_value())
    this->fan_mode = *call.get_fan_mode();
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();
  if (this->supports_two_points_) {
    if (call.get_target_temperature_low().has_value()) {
      this->target_temperature_low = *call.get_target_temperature_low();
      validate_target_temperature_low();
    }
    if (call.get_target_temperature_high().has_value()) {
      this->target_temperature_high = *call.get_target_temperature_high();
      validate_target_temperature_high();
    }
  } else {
    if (call.get_target_temperature().has_value()) {
      this->target_temperature = *call.get_target_temperature();
      validate_target_temperature();
    }
  }
  // make any changes happen
  refresh();
}

climate::ClimateTraits ThermostatClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  if (supports_auto_)
    traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);
  if (supports_heat_cool_)
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT_COOL);
  if (supports_cool_)
    traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  if (supports_dry_)
    traits.add_supported_mode(climate::CLIMATE_MODE_DRY);
  if (supports_fan_only_)
    traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);
  if (supports_heat_)
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);

  if (supports_fan_mode_on_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_ON);
  if (supports_fan_mode_off_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_OFF);
  if (supports_fan_mode_auto_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
  if (supports_fan_mode_low_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_LOW);
  if (supports_fan_mode_medium_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_MEDIUM);
  if (supports_fan_mode_high_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_HIGH);
  if (supports_fan_mode_middle_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_MIDDLE);
  if (supports_fan_mode_focus_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_FOCUS);
  if (supports_fan_mode_diffuse_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_DIFFUSE);

  if (supports_swing_mode_both_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
  if (supports_swing_mode_horizontal_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
  if (supports_swing_mode_off_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
  if (supports_swing_mode_vertical_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);

  if (supports_away_)
    traits.set_supported_presets({climate::CLIMATE_PRESET_HOME, climate::CLIMATE_PRESET_AWAY});

  traits.set_supports_two_point_target_temperature(this->supports_two_points_);
  traits.set_supports_action(true);
  return traits;
}

climate::ClimateAction ThermostatClimate::compute_action_(const bool ignore_timers) {
  auto target_action = climate::CLIMATE_ACTION_IDLE;
  // if any hysteresis values or current_temperature is not valid, we go to OFF;
  if (std::isnan(this->current_temperature) || !this->hysteresis_valid()) {
    return climate::CLIMATE_ACTION_OFF;
  }
  // do not change the action if an "ON" timer is running
  if ((!ignore_timers) &&
      (timer_active_(thermostat::TIMER_IDLE_ON) || timer_active_(thermostat::TIMER_COOLING_ON) ||
       timer_active_(thermostat::TIMER_FANNING_ON) || timer_active_(thermostat::TIMER_HEATING_ON))) {
    return this->action;
  }

  // ensure set point(s) is/are valid before computing the action
  this->validate_target_temperatures();
  // everything has been validated so we can now safely compute the action
  switch (this->mode) {
    // if the climate mode is OFF then the climate action must be OFF
    case climate::CLIMATE_MODE_OFF:
      target_action = climate::CLIMATE_ACTION_OFF;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      if (this->fanning_required_())
        target_action = climate::CLIMATE_ACTION_FAN;
      break;
    case climate::CLIMATE_MODE_DRY:
      target_action = climate::CLIMATE_ACTION_DRYING;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      if (this->cooling_required_() && this->heating_required_()) {
        // this is bad and should never happen, so just stop.
        // target_action = climate::CLIMATE_ACTION_IDLE;
      } else if (this->cooling_required_()) {
        target_action = climate::CLIMATE_ACTION_COOLING;
      } else if (this->heating_required_()) {
        target_action = climate::CLIMATE_ACTION_HEATING;
      }
      break;
    case climate::CLIMATE_MODE_COOL:
      if (this->cooling_required_()) {
        target_action = climate::CLIMATE_ACTION_COOLING;
      }
      break;
    case climate::CLIMATE_MODE_HEAT:
      if (this->heating_required_()) {
        target_action = climate::CLIMATE_ACTION_HEATING;
      }
      break;
    default:
      break;
  }
  // do not abruptly switch actions. cycle through IDLE, first. we'll catch this at the next update.
  if ((((this->action == climate::CLIMATE_ACTION_COOLING) || (this->action == climate::CLIMATE_ACTION_DRYING)) &&
       (target_action == climate::CLIMATE_ACTION_HEATING)) ||
      ((this->action == climate::CLIMATE_ACTION_HEATING) &&
       ((target_action == climate::CLIMATE_ACTION_COOLING) || (target_action == climate::CLIMATE_ACTION_DRYING)))) {
    return climate::CLIMATE_ACTION_IDLE;
  }

  return target_action;
}

climate::ClimateAction ThermostatClimate::compute_supplemental_action_() {
  auto target_action = climate::CLIMATE_ACTION_IDLE;
  // if any hysteresis values or current_temperature is not valid, we go to OFF;
  if (std::isnan(this->current_temperature) || !this->hysteresis_valid()) {
    return climate::CLIMATE_ACTION_OFF;
  }

  // ensure set point(s) is/are valid before computing the action
  this->validate_target_temperatures();
  // everything has been validated so we can now safely compute the action
  switch (this->mode) {
    // if the climate mode is OFF then the climate action must be OFF
    case climate::CLIMATE_MODE_OFF:
      target_action = climate::CLIMATE_ACTION_OFF;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      if (this->supplemental_cooling_required_() && this->supplemental_heating_required_()) {
        // this is bad and should never happen, so just stop.
        // target_action = climate::CLIMATE_ACTION_IDLE;
      } else if (this->supplemental_cooling_required_()) {
        target_action = climate::CLIMATE_ACTION_COOLING;
      } else if (this->supplemental_heating_required_()) {
        target_action = climate::CLIMATE_ACTION_HEATING;
      }
      break;
    case climate::CLIMATE_MODE_COOL:
      if (this->supplemental_cooling_required_()) {
        target_action = climate::CLIMATE_ACTION_COOLING;
      }
      break;
    case climate::CLIMATE_MODE_HEAT:
      if (this->supplemental_heating_required_()) {
        target_action = climate::CLIMATE_ACTION_HEATING;
      }
      break;
    default:
      break;
  }

  return target_action;
}

void ThermostatClimate::switch_to_action_(climate::ClimateAction action, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((action == this->action) && this->setup_complete_)
    // already in target mode
    return;

  if (((action == climate::CLIMATE_ACTION_OFF && this->action == climate::CLIMATE_ACTION_IDLE) ||
       (action == climate::CLIMATE_ACTION_IDLE && this->action == climate::CLIMATE_ACTION_OFF)) &&
      this->setup_complete_) {
    // switching from OFF to IDLE or vice-versa -- this is only a visual difference.
    // OFF means user manually disabled, IDLE means the temperature is in target range.
    this->action = action;
    if (publish_state)
      this->publish_state();
    return;
  }

  bool action_ready = false;
  Trigger<> *trig = this->idle_action_trigger_, *trig_fan = nullptr;
  switch (action) {
    case climate::CLIMATE_ACTION_OFF:
    case climate::CLIMATE_ACTION_IDLE:
      if (this->idle_action_ready_()) {
        this->start_timer_(thermostat::TIMER_IDLE_ON);
        if (this->action == climate::CLIMATE_ACTION_COOLING)
          this->start_timer_(thermostat::TIMER_COOLING_OFF);
        if (this->action == climate::CLIMATE_ACTION_FAN) {
          if (this->supports_fan_only_action_uses_fan_mode_timer_)
            this->start_timer_(thermostat::TIMER_FAN_MODE);
          else
            this->start_timer_(thermostat::TIMER_FANNING_OFF);
        }
        if (this->action == climate::CLIMATE_ACTION_HEATING)
          this->start_timer_(thermostat::TIMER_HEATING_OFF);
        // trig = this->idle_action_trigger_;
        ESP_LOGVV(TAG, "Switching to IDLE/OFF action");
        this->cooling_max_runtime_exceeded_ = false;
        this->heating_max_runtime_exceeded_ = false;
        action_ready = true;
      }
      break;
    case climate::CLIMATE_ACTION_COOLING:
      if (this->cooling_action_ready_()) {
        this->start_timer_(thermostat::TIMER_COOLING_ON);
        this->start_timer_(thermostat::TIMER_COOLING_MAX_RUN_TIME);
        if (this->supports_fan_with_cooling_) {
          this->start_timer_(thermostat::TIMER_FANNING_ON);
          trig_fan = this->fan_only_action_trigger_;
        }
        trig = this->cool_action_trigger_;
        ESP_LOGVV(TAG, "Switching to COOLING action");
        action_ready = true;
      }
      break;
    case climate::CLIMATE_ACTION_HEATING:
      if (this->heating_action_ready_()) {
        this->start_timer_(thermostat::TIMER_HEATING_ON);
        this->start_timer_(thermostat::TIMER_HEATING_MAX_RUN_TIME);
        if (this->supports_fan_with_heating_) {
          this->start_timer_(thermostat::TIMER_FANNING_ON);
          trig_fan = this->fan_only_action_trigger_;
        }
        trig = this->heat_action_trigger_;
        ESP_LOGVV(TAG, "Switching to HEATING action");
        action_ready = true;
      }
      break;
    case climate::CLIMATE_ACTION_FAN:
      if (this->fanning_action_ready_()) {
        if (this->supports_fan_only_action_uses_fan_mode_timer_)
          this->start_timer_(thermostat::TIMER_FAN_MODE);
        else
          this->start_timer_(thermostat::TIMER_FANNING_ON);
        trig = this->fan_only_action_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_ONLY action");
        action_ready = true;
      }
      break;
    case climate::CLIMATE_ACTION_DRYING:
      if (this->drying_action_ready_()) {
        this->start_timer_(thermostat::TIMER_COOLING_ON);
        this->start_timer_(thermostat::TIMER_FANNING_ON);
        trig = this->dry_action_trigger_;
        ESP_LOGVV(TAG, "Switching to DRYING action");
        action_ready = true;
      }
      break;
    default:
      // we cannot report an invalid mode back to HA (even if it asked for one)
      //  and must assume some valid value
      action = climate::CLIMATE_ACTION_OFF;
      // trig = this->idle_action_trigger_;
  }

  if (action_ready) {
    if (this->prev_action_trigger_ != nullptr) {
      this->prev_action_trigger_->stop_action();
      this->prev_action_trigger_ = nullptr;
    }
    this->action = action;
    this->prev_action_trigger_ = trig;
    assert(trig != nullptr);
    trig->trigger();
    // if enabled, call the fan_only action with cooling/heating actions
    if (trig_fan != nullptr) {
      ESP_LOGVV(TAG, "Calling FAN_ONLY action with HEATING/COOLING action");
      trig_fan->trigger();
    }
    if (publish_state)
      this->publish_state();
  }
}

void ThermostatClimate::switch_to_supplemental_action_(climate::ClimateAction action) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((action == this->supplemental_action_) && this->setup_complete_)
    // already in target mode
    return;

  switch (action) {
    case climate::CLIMATE_ACTION_OFF:
    case climate::CLIMATE_ACTION_IDLE:
      this->cancel_timer_(thermostat::TIMER_COOLING_MAX_RUN_TIME);
      this->cancel_timer_(thermostat::TIMER_HEATING_MAX_RUN_TIME);
      break;
    case climate::CLIMATE_ACTION_COOLING:
      this->cancel_timer_(thermostat::TIMER_COOLING_MAX_RUN_TIME);
      break;
    case climate::CLIMATE_ACTION_HEATING:
      this->cancel_timer_(thermostat::TIMER_HEATING_MAX_RUN_TIME);
      break;
    default:
      return;
  }
  ESP_LOGVV(TAG, "Updating supplemental action...");
  this->supplemental_action_ = action;
  this->trigger_supplemental_action_();
}

void ThermostatClimate::trigger_supplemental_action_() {
  Trigger<> *trig = nullptr;

  switch (this->supplemental_action_) {
    case climate::CLIMATE_ACTION_COOLING:
      if (!this->timer_active_(thermostat::TIMER_COOLING_MAX_RUN_TIME)) {
        this->start_timer_(thermostat::TIMER_COOLING_MAX_RUN_TIME);
      }
      trig = this->supplemental_cool_action_trigger_;
      ESP_LOGVV(TAG, "Calling supplemental COOLING action");
      break;
    case climate::CLIMATE_ACTION_HEATING:
      if (!this->timer_active_(thermostat::TIMER_HEATING_MAX_RUN_TIME)) {
        this->start_timer_(thermostat::TIMER_HEATING_MAX_RUN_TIME);
      }
      trig = this->supplemental_heat_action_trigger_;
      ESP_LOGVV(TAG, "Calling supplemental HEATING action");
      break;
    default:
      break;
  }

  if (trig != nullptr) {
    assert(trig != nullptr);
    trig->trigger();
  }
}

void ThermostatClimate::switch_to_fan_mode_(climate::ClimateFanMode fan_mode, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((fan_mode == this->prev_fan_mode_) && this->setup_complete_)
    // already in target mode
    return;

  this->fan_mode = fan_mode;
  if (publish_state)
    this->publish_state();

  if (this->fan_mode_ready_()) {
    Trigger<> *trig = this->fan_mode_auto_trigger_;
    switch (fan_mode) {
      case climate::CLIMATE_FAN_ON:
        trig = this->fan_mode_on_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_ON mode");
        break;
      case climate::CLIMATE_FAN_OFF:
        trig = this->fan_mode_off_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_OFF mode");
        break;
      case climate::CLIMATE_FAN_AUTO:
        // trig = this->fan_mode_auto_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_AUTO mode");
        break;
      case climate::CLIMATE_FAN_LOW:
        trig = this->fan_mode_low_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_LOW mode");
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        trig = this->fan_mode_medium_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_MEDIUM mode");
        break;
      case climate::CLIMATE_FAN_HIGH:
        trig = this->fan_mode_high_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_HIGH mode");
        break;
      case climate::CLIMATE_FAN_MIDDLE:
        trig = this->fan_mode_middle_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_MIDDLE mode");
        break;
      case climate::CLIMATE_FAN_FOCUS:
        trig = this->fan_mode_focus_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_FOCUS mode");
        break;
      case climate::CLIMATE_FAN_DIFFUSE:
        trig = this->fan_mode_diffuse_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_DIFFUSE mode");
        break;
      default:
        // we cannot report an invalid mode back to HA (even if it asked for one)
        //  and must assume some valid value
        fan_mode = climate::CLIMATE_FAN_AUTO;
        // trig = this->fan_mode_auto_trigger_;
    }
    if (this->prev_fan_mode_trigger_ != nullptr) {
      this->prev_fan_mode_trigger_->stop_action();
      this->prev_fan_mode_trigger_ = nullptr;
    }
    this->start_timer_(thermostat::TIMER_FAN_MODE);
    assert(trig != nullptr);
    trig->trigger();
    this->prev_fan_mode_ = fan_mode;
    this->prev_fan_mode_trigger_ = trig;
  }
}

void ThermostatClimate::switch_to_mode_(climate::ClimateMode mode, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((mode == this->prev_mode_) && this->setup_complete_)
    // already in target mode
    return;

  if (this->prev_mode_trigger_ != nullptr) {
    this->prev_mode_trigger_->stop_action();
    this->prev_mode_trigger_ = nullptr;
  }
  Trigger<> *trig = this->auto_mode_trigger_;
  switch (mode) {
    case climate::CLIMATE_MODE_OFF:
      trig = this->off_mode_trigger_;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      // trig = this->auto_mode_trigger_;
      break;
    case climate::CLIMATE_MODE_COOL:
      trig = this->cool_mode_trigger_;
      break;
    case climate::CLIMATE_MODE_HEAT:
      trig = this->heat_mode_trigger_;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      trig = this->fan_only_mode_trigger_;
      break;
    case climate::CLIMATE_MODE_DRY:
      trig = this->dry_mode_trigger_;
      break;
    default:
      // we cannot report an invalid mode back to HA (even if it asked for one)
      //  and must assume some valid value
      mode = climate::CLIMATE_MODE_HEAT_COOL;
      // trig = this->auto_mode_trigger_;
  }
  assert(trig != nullptr);
  trig->trigger();
  this->mode = mode;
  this->prev_mode_ = mode;
  this->prev_mode_trigger_ = trig;
  if (publish_state)
    this->publish_state();
}

void ThermostatClimate::switch_to_swing_mode_(climate::ClimateSwingMode swing_mode, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((swing_mode == this->prev_swing_mode_) && this->setup_complete_)
    // already in target mode
    return;

  if (this->prev_swing_mode_trigger_ != nullptr) {
    this->prev_swing_mode_trigger_->stop_action();
    this->prev_swing_mode_trigger_ = nullptr;
  }
  Trigger<> *trig = this->swing_mode_off_trigger_;
  switch (swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
      trig = this->swing_mode_both_trigger_;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      trig = this->swing_mode_horizontal_trigger_;
      break;
    case climate::CLIMATE_SWING_OFF:
      // trig = this->swing_mode_off_trigger_;
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      trig = this->swing_mode_vertical_trigger_;
      break;
    default:
      // we cannot report an invalid mode back to HA (even if it asked for one)
      //  and must assume some valid value
      swing_mode = climate::CLIMATE_SWING_OFF;
      // trig = this->swing_mode_off_trigger_;
  }
  assert(trig != nullptr);
  trig->trigger();
  this->swing_mode = swing_mode;
  this->prev_swing_mode_ = swing_mode;
  this->prev_swing_mode_trigger_ = trig;
  if (publish_state)
    this->publish_state();
}

bool ThermostatClimate::idle_action_ready_() {
  if (this->supports_fan_only_action_uses_fan_mode_timer_) {
    return !(this->timer_active_(thermostat::TIMER_COOLING_ON) || this->timer_active_(thermostat::TIMER_FAN_MODE) ||
             this->timer_active_(thermostat::TIMER_HEATING_ON));
  }
  return !(this->timer_active_(thermostat::TIMER_COOLING_ON) || this->timer_active_(thermostat::TIMER_FANNING_ON) ||
           this->timer_active_(thermostat::TIMER_HEATING_ON));
}

bool ThermostatClimate::cooling_action_ready_() {
  return !(this->timer_active_(thermostat::TIMER_IDLE_ON) || this->timer_active_(thermostat::TIMER_FANNING_OFF) ||
           this->timer_active_(thermostat::TIMER_COOLING_OFF) || this->timer_active_(thermostat::TIMER_HEATING_ON));
}

bool ThermostatClimate::drying_action_ready_() {
  return !(this->timer_active_(thermostat::TIMER_IDLE_ON) || this->timer_active_(thermostat::TIMER_FANNING_OFF) ||
           this->timer_active_(thermostat::TIMER_COOLING_OFF) || this->timer_active_(thermostat::TIMER_HEATING_ON));
}

bool ThermostatClimate::fan_mode_ready_() { return !(this->timer_active_(thermostat::TIMER_FAN_MODE)); }

bool ThermostatClimate::fanning_action_ready_() {
  if (this->supports_fan_only_action_uses_fan_mode_timer_) {
    return !(this->timer_active_(thermostat::TIMER_FAN_MODE));
  }
  return !(this->timer_active_(thermostat::TIMER_IDLE_ON) || this->timer_active_(thermostat::TIMER_FANNING_OFF));
}

bool ThermostatClimate::heating_action_ready_() {
  return !(this->timer_active_(thermostat::TIMER_IDLE_ON) || this->timer_active_(thermostat::TIMER_COOLING_ON) ||
           this->timer_active_(thermostat::TIMER_FANNING_OFF) || this->timer_active_(thermostat::TIMER_HEATING_OFF));
}

void ThermostatClimate::start_timer_(const ThermostatClimateTimerIndex timer_index) {
  if (this->timer_duration_(timer_index) > 0) {
    this->set_timeout(this->timer_[timer_index].name, this->timer_duration_(timer_index),
                      this->timer_cbf_(timer_index));
    this->timer_[timer_index].active = true;
  }
}

bool ThermostatClimate::cancel_timer_(ThermostatClimateTimerIndex timer_index) {
  this->timer_[timer_index].active = false;
  return this->cancel_timeout(this->timer_[timer_index].name);
}

bool ThermostatClimate::timer_active_(ThermostatClimateTimerIndex timer_index) {
  return this->timer_[timer_index].active;
}

uint32_t ThermostatClimate::timer_duration_(ThermostatClimateTimerIndex timer_index) {
  return this->timer_[timer_index].time;
}

std::function<void()> ThermostatClimate::timer_cbf_(ThermostatClimateTimerIndex timer_index) {
  return this->timer_[timer_index].func;
}

void ThermostatClimate::cooling_max_run_time_timer_callback_() {
  ESP_LOGVV(TAG, "cooling_max_run_time timer expired");
  this->timer_[thermostat::TIMER_COOLING_MAX_RUN_TIME].active = false;
  this->cooling_max_runtime_exceeded_ = true;
  this->trigger_supplemental_action_();
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::cooling_off_timer_callback_() {
  ESP_LOGVV(TAG, "cooling_off timer expired");
  this->timer_[thermostat::TIMER_COOLING_OFF].active = false;
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::cooling_on_timer_callback_() {
  ESP_LOGVV(TAG, "cooling_on timer expired");
  this->timer_[thermostat::TIMER_COOLING_ON].active = false;
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::fan_mode_timer_callback_() {
  ESP_LOGVV(TAG, "fan_mode timer expired");
  this->timer_[thermostat::TIMER_FAN_MODE].active = false;
  this->switch_to_fan_mode_(this->fan_mode.value_or(climate::CLIMATE_FAN_ON));
  if (this->supports_fan_only_action_uses_fan_mode_timer_)
    this->switch_to_action_(this->compute_action_());
}

void ThermostatClimate::fanning_off_timer_callback_() {
  ESP_LOGVV(TAG, "fanning_off timer expired");
  this->timer_[thermostat::TIMER_FANNING_OFF].active = false;
  this->switch_to_action_(this->compute_action_());
}

void ThermostatClimate::fanning_on_timer_callback_() {
  ESP_LOGVV(TAG, "fanning_on timer expired");
  this->timer_[thermostat::TIMER_FANNING_ON].active = false;
  this->switch_to_action_(this->compute_action_());
}

void ThermostatClimate::heating_max_run_time_timer_callback_() {
  ESP_LOGVV(TAG, "heating_max_run_time timer expired");
  this->timer_[thermostat::TIMER_HEATING_MAX_RUN_TIME].active = false;
  this->heating_max_runtime_exceeded_ = true;
  this->trigger_supplemental_action_();
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::heating_off_timer_callback_() {
  ESP_LOGVV(TAG, "heating_off timer expired");
  this->timer_[thermostat::TIMER_HEATING_OFF].active = false;
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::heating_on_timer_callback_() {
  ESP_LOGVV(TAG, "heating_on timer expired");
  this->timer_[thermostat::TIMER_HEATING_ON].active = false;
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::idle_on_timer_callback_() {
  ESP_LOGVV(TAG, "idle_on timer expired");
  this->timer_[thermostat::TIMER_IDLE_ON].active = false;
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::check_temperature_change_trigger_() {
  if (this->supports_two_points_) {
    // setup_complete_ helps us ensure an action is called immediately after boot
    if ((this->prev_target_temperature_low_ == this->target_temperature_low) &&
        (this->prev_target_temperature_high_ == this->target_temperature_high) && this->setup_complete_) {
      return;  // nothing changed, no reason to trigger
    } else {
      // save the new temperatures so we can check them again later; the trigger will fire below
      this->prev_target_temperature_low_ = this->target_temperature_low;
      this->prev_target_temperature_high_ = this->target_temperature_high;
    }
  } else {
    if ((this->prev_target_temperature_ == this->target_temperature) && this->setup_complete_) {
      return;  // nothing changed, no reason to trigger
    } else {
      // save the new temperature so we can check it again later; the trigger will fire below
      this->prev_target_temperature_ = this->target_temperature;
    }
  }
  // trigger the action
  Trigger<> *trig = this->temperature_change_trigger_;
  assert(trig != nullptr);
  trig->trigger();
}

bool ThermostatClimate::cooling_required_() {
  auto temperature = this->supports_two_points_ ? this->target_temperature_high : this->target_temperature;

  if (this->supports_cool_) {
    if (this->current_temperature > temperature + this->cooling_deadband_) {
      // if the current temperature exceeds the target + deadband, cooling is required
      return true;
    } else if (this->current_temperature < temperature - this->cooling_overrun_) {
      // if the current temperature is less than the target - overrun, cooling should stop
      return false;
    } else {
      // if we get here, the current temperature is between target + deadband and target - overrun,
      //  so the action should not change unless it conflicts with the current mode
      return (this->action == climate::CLIMATE_ACTION_COOLING) &&
             ((this->mode == climate::CLIMATE_MODE_HEAT_COOL) || (this->mode == climate::CLIMATE_MODE_COOL));
    }
  }
  return false;
}

bool ThermostatClimate::fanning_required_() {
  auto temperature = this->supports_two_points_ ? this->target_temperature_high : this->target_temperature;

  if (this->supports_fan_only_) {
    if (this->supports_fan_only_cooling_) {
      if (this->current_temperature > temperature + this->cooling_deadband_) {
        // if the current temperature exceeds the target + deadband, fanning is required
        return true;
      } else if (this->current_temperature < temperature - this->cooling_overrun_) {
        // if the current temperature is less than the target - overrun, fanning should stop
        return false;
      } else {
        // if we get here, the current temperature is between target + deadband and target - overrun,
        //  so the action should not change unless it conflicts with the current mode
        return (this->action == climate::CLIMATE_ACTION_FAN) && (this->mode == climate::CLIMATE_MODE_FAN_ONLY);
      }
    } else {
      return true;
    }
  }
  return false;
}

bool ThermostatClimate::heating_required_() {
  auto temperature = this->supports_two_points_ ? this->target_temperature_low : this->target_temperature;

  if (this->supports_heat_) {
    if (this->current_temperature < temperature - this->heating_deadband_) {
      // if the current temperature is below the target - deadband, heating is required
      return true;
    } else if (this->current_temperature > temperature + this->heating_overrun_) {
      // if the current temperature is above the target + overrun, heating should stop
      return false;
    } else {
      // if we get here, the current temperature is between target - deadband and target + overrun,
      //  so the action should not change unless it conflicts with the current mode
      return (this->action == climate::CLIMATE_ACTION_HEATING) &&
             ((this->mode == climate::CLIMATE_MODE_HEAT_COOL) || (this->mode == climate::CLIMATE_MODE_HEAT));
    }
  }
  return false;
}

bool ThermostatClimate::supplemental_cooling_required_() {
  auto temperature = this->supports_two_points_ ? this->target_temperature_high : this->target_temperature;
  // the component must supports_cool_ and the climate action must be climate::CLIMATE_ACTION_COOLING. then...
  // supplemental cooling is required if the max delta or max runtime was exceeded or the action is already engaged
  return this->supports_cool_ && (this->action == climate::CLIMATE_ACTION_COOLING) &&
         (this->cooling_max_runtime_exceeded_ ||
          (this->current_temperature > temperature + this->supplemental_cool_delta_) ||
          (this->supplemental_action_ == climate::CLIMATE_ACTION_COOLING));
}

bool ThermostatClimate::supplemental_heating_required_() {
  auto temperature = this->supports_two_points_ ? this->target_temperature_low : this->target_temperature;
  // the component must supports_heat_ and the climate action must be climate::CLIMATE_ACTION_HEATING. then...
  // supplemental heating is required if the max delta or max runtime was exceeded or the action is already engaged
  return this->supports_heat_ && (this->action == climate::CLIMATE_ACTION_HEATING) &&
         (this->heating_max_runtime_exceeded_ ||
          (this->current_temperature < temperature - this->supplemental_heat_delta_) ||
          (this->supplemental_action_ == climate::CLIMATE_ACTION_HEATING));
}

void ThermostatClimate::change_away_(bool away) {
  if (!away) {
    if (this->supports_two_points_) {
      this->target_temperature_low = this->normal_config_.default_temperature_low;
      this->target_temperature_high = this->normal_config_.default_temperature_high;
    } else
      this->target_temperature = this->normal_config_.default_temperature;
  } else {
    if (this->supports_two_points_) {
      this->target_temperature_low = this->away_config_.default_temperature_low;
      this->target_temperature_high = this->away_config_.default_temperature_high;
    } else
      this->target_temperature = this->away_config_.default_temperature;
  }
  this->preset = away ? climate::CLIMATE_PRESET_AWAY : climate::CLIMATE_PRESET_HOME;
}

void ThermostatClimate::set_normal_config(const ThermostatClimateTargetTempConfig &normal_config) {
  this->normal_config_ = normal_config;
}

void ThermostatClimate::set_away_config(const ThermostatClimateTargetTempConfig &away_config) {
  this->supports_away_ = true;
  this->away_config_ = away_config;
}

ThermostatClimate::ThermostatClimate()
    : cool_action_trigger_(new Trigger<>()),
      supplemental_cool_action_trigger_(new Trigger<>()),
      cool_mode_trigger_(new Trigger<>()),
      dry_action_trigger_(new Trigger<>()),
      dry_mode_trigger_(new Trigger<>()),
      heat_action_trigger_(new Trigger<>()),
      supplemental_heat_action_trigger_(new Trigger<>()),
      heat_mode_trigger_(new Trigger<>()),
      auto_mode_trigger_(new Trigger<>()),
      idle_action_trigger_(new Trigger<>()),
      off_mode_trigger_(new Trigger<>()),
      fan_only_action_trigger_(new Trigger<>()),
      fan_only_mode_trigger_(new Trigger<>()),
      fan_mode_on_trigger_(new Trigger<>()),
      fan_mode_off_trigger_(new Trigger<>()),
      fan_mode_auto_trigger_(new Trigger<>()),
      fan_mode_low_trigger_(new Trigger<>()),
      fan_mode_medium_trigger_(new Trigger<>()),
      fan_mode_high_trigger_(new Trigger<>()),
      fan_mode_middle_trigger_(new Trigger<>()),
      fan_mode_focus_trigger_(new Trigger<>()),
      fan_mode_diffuse_trigger_(new Trigger<>()),
      swing_mode_both_trigger_(new Trigger<>()),
      swing_mode_off_trigger_(new Trigger<>()),
      swing_mode_horizontal_trigger_(new Trigger<>()),
      swing_mode_vertical_trigger_(new Trigger<>()),
      temperature_change_trigger_(new Trigger<>()) {}

void ThermostatClimate::set_default_mode(climate::ClimateMode default_mode) { this->default_mode_ = default_mode; }
void ThermostatClimate::set_set_point_minimum_differential(float differential) {
  this->set_point_minimum_differential_ = differential;
}
void ThermostatClimate::set_cool_deadband(float deadband) { this->cooling_deadband_ = deadband; }
void ThermostatClimate::set_cool_overrun(float overrun) { this->cooling_overrun_ = overrun; }
void ThermostatClimate::set_heat_deadband(float deadband) { this->heating_deadband_ = deadband; }
void ThermostatClimate::set_heat_overrun(float overrun) { this->heating_overrun_ = overrun; }
void ThermostatClimate::set_supplemental_cool_delta(float delta) { this->supplemental_cool_delta_ = delta; }
void ThermostatClimate::set_supplemental_heat_delta(float delta) { this->supplemental_heat_delta_ = delta; }
void ThermostatClimate::set_cooling_maximum_run_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_COOLING_MAX_RUN_TIME].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_cooling_minimum_off_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_COOLING_OFF].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_cooling_minimum_run_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_COOLING_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_fan_mode_minimum_switching_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_FAN_MODE].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_fanning_minimum_off_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_FANNING_OFF].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_fanning_minimum_run_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_FANNING_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_heating_maximum_run_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_HEATING_MAX_RUN_TIME].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_heating_minimum_off_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_HEATING_OFF].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_heating_minimum_run_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_HEATING_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_idle_minimum_time_in_sec(uint32_t time) {
  this->timer_[thermostat::TIMER_IDLE_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
void ThermostatClimate::set_use_startup_delay(bool use_startup_delay) { this->use_startup_delay_ = use_startup_delay; }
void ThermostatClimate::set_supports_heat_cool(bool supports_heat_cool) {
  this->supports_heat_cool_ = supports_heat_cool;
}
void ThermostatClimate::set_supports_auto(bool supports_auto) { this->supports_auto_ = supports_auto; }
void ThermostatClimate::set_supports_cool(bool supports_cool) { this->supports_cool_ = supports_cool; }
void ThermostatClimate::set_supports_dry(bool supports_dry) { this->supports_dry_ = supports_dry; }
void ThermostatClimate::set_supports_fan_only(bool supports_fan_only) { this->supports_fan_only_ = supports_fan_only; }
void ThermostatClimate::set_supports_fan_only_action_uses_fan_mode_timer(
    bool supports_fan_only_action_uses_fan_mode_timer) {
  this->supports_fan_only_action_uses_fan_mode_timer_ = supports_fan_only_action_uses_fan_mode_timer;
}
void ThermostatClimate::set_supports_fan_only_cooling(bool supports_fan_only_cooling) {
  this->supports_fan_only_cooling_ = supports_fan_only_cooling;
}
void ThermostatClimate::set_supports_fan_with_cooling(bool supports_fan_with_cooling) {
  this->supports_fan_with_cooling_ = supports_fan_with_cooling;
}
void ThermostatClimate::set_supports_fan_with_heating(bool supports_fan_with_heating) {
  this->supports_fan_with_heating_ = supports_fan_with_heating;
}
void ThermostatClimate::set_supports_heat(bool supports_heat) { this->supports_heat_ = supports_heat; }
void ThermostatClimate::set_supports_fan_mode_on(bool supports_fan_mode_on) {
  this->supports_fan_mode_on_ = supports_fan_mode_on;
}
void ThermostatClimate::set_supports_fan_mode_off(bool supports_fan_mode_off) {
  this->supports_fan_mode_off_ = supports_fan_mode_off;
}
void ThermostatClimate::set_supports_fan_mode_auto(bool supports_fan_mode_auto) {
  this->supports_fan_mode_auto_ = supports_fan_mode_auto;
}
void ThermostatClimate::set_supports_fan_mode_low(bool supports_fan_mode_low) {
  this->supports_fan_mode_low_ = supports_fan_mode_low;
}
void ThermostatClimate::set_supports_fan_mode_medium(bool supports_fan_mode_medium) {
  this->supports_fan_mode_medium_ = supports_fan_mode_medium;
}
void ThermostatClimate::set_supports_fan_mode_high(bool supports_fan_mode_high) {
  this->supports_fan_mode_high_ = supports_fan_mode_high;
}
void ThermostatClimate::set_supports_fan_mode_middle(bool supports_fan_mode_middle) {
  this->supports_fan_mode_middle_ = supports_fan_mode_middle;
}
void ThermostatClimate::set_supports_fan_mode_focus(bool supports_fan_mode_focus) {
  this->supports_fan_mode_focus_ = supports_fan_mode_focus;
}
void ThermostatClimate::set_supports_fan_mode_diffuse(bool supports_fan_mode_diffuse) {
  this->supports_fan_mode_diffuse_ = supports_fan_mode_diffuse;
}
void ThermostatClimate::set_supports_swing_mode_both(bool supports_swing_mode_both) {
  this->supports_swing_mode_both_ = supports_swing_mode_both;
}
void ThermostatClimate::set_supports_swing_mode_off(bool supports_swing_mode_off) {
  this->supports_swing_mode_off_ = supports_swing_mode_off;
}
void ThermostatClimate::set_supports_swing_mode_horizontal(bool supports_swing_mode_horizontal) {
  this->supports_swing_mode_horizontal_ = supports_swing_mode_horizontal;
}
void ThermostatClimate::set_supports_swing_mode_vertical(bool supports_swing_mode_vertical) {
  this->supports_swing_mode_vertical_ = supports_swing_mode_vertical;
}
void ThermostatClimate::set_supports_two_points(bool supports_two_points) {
  this->supports_two_points_ = supports_two_points;
}

Trigger<> *ThermostatClimate::get_cool_action_trigger() const { return this->cool_action_trigger_; }
Trigger<> *ThermostatClimate::get_supplemental_cool_action_trigger() const {
  return this->supplemental_cool_action_trigger_;
}
Trigger<> *ThermostatClimate::get_dry_action_trigger() const { return this->dry_action_trigger_; }
Trigger<> *ThermostatClimate::get_fan_only_action_trigger() const { return this->fan_only_action_trigger_; }
Trigger<> *ThermostatClimate::get_heat_action_trigger() const { return this->heat_action_trigger_; }
Trigger<> *ThermostatClimate::get_supplemental_heat_action_trigger() const {
  return this->supplemental_heat_action_trigger_;
}
Trigger<> *ThermostatClimate::get_idle_action_trigger() const { return this->idle_action_trigger_; }
Trigger<> *ThermostatClimate::get_auto_mode_trigger() const { return this->auto_mode_trigger_; }
Trigger<> *ThermostatClimate::get_cool_mode_trigger() const { return this->cool_mode_trigger_; }
Trigger<> *ThermostatClimate::get_dry_mode_trigger() const { return this->dry_mode_trigger_; }
Trigger<> *ThermostatClimate::get_fan_only_mode_trigger() const { return this->fan_only_mode_trigger_; }
Trigger<> *ThermostatClimate::get_heat_mode_trigger() const { return this->heat_mode_trigger_; }
Trigger<> *ThermostatClimate::get_off_mode_trigger() const { return this->off_mode_trigger_; }
Trigger<> *ThermostatClimate::get_fan_mode_on_trigger() const { return this->fan_mode_on_trigger_; }
Trigger<> *ThermostatClimate::get_fan_mode_off_trigger() const { return this->fan_mode_off_trigger_; }
Trigger<> *ThermostatClimate::get_fan_mode_auto_trigger() const { return this->fan_mode_auto_trigger_; }
Trigger<> *ThermostatClimate::get_fan_mode_low_trigger() const { return this->fan_mode_low_trigger_; }
Trigger<> *ThermostatClimate::get_fan_mode_medium_trigger() const { return this->fan_mode_medium_trigger_; }
Trigger<> *ThermostatClimate::get_fan_mode_high_trigger() const { return this->fan_mode_high_trigger_; }
Trigger<> *ThermostatClimate::get_fan_mode_middle_trigger() const { return this->fan_mode_middle_trigger_; }
Trigger<> *ThermostatClimate::get_fan_mode_focus_trigger() const { return this->fan_mode_focus_trigger_; }
Trigger<> *ThermostatClimate::get_fan_mode_diffuse_trigger() const { return this->fan_mode_diffuse_trigger_; }
Trigger<> *ThermostatClimate::get_swing_mode_both_trigger() const { return this->swing_mode_both_trigger_; }
Trigger<> *ThermostatClimate::get_swing_mode_off_trigger() const { return this->swing_mode_off_trigger_; }
Trigger<> *ThermostatClimate::get_swing_mode_horizontal_trigger() const { return this->swing_mode_horizontal_trigger_; }
Trigger<> *ThermostatClimate::get_swing_mode_vertical_trigger() const { return this->swing_mode_vertical_trigger_; }
Trigger<> *ThermostatClimate::get_temperature_change_trigger() const { return this->temperature_change_trigger_; }

void ThermostatClimate::dump_config() {
  LOG_CLIMATE("", "Thermostat", this);
  if (this->supports_heat_) {
    if (this->supports_two_points_)
      ESP_LOGCONFIG(TAG, "  Default Target Temperature Low: %.1f°C", this->normal_config_.default_temperature_low);
    else
      ESP_LOGCONFIG(TAG, "  Default Target Temperature Low: %.1f°C", this->normal_config_.default_temperature);
  }
  if ((this->supports_cool_) || (this->supports_fan_only_ && this->supports_fan_only_cooling_)) {
    if (this->supports_two_points_)
      ESP_LOGCONFIG(TAG, "  Default Target Temperature High: %.1f°C", this->normal_config_.default_temperature_high);
    else
      ESP_LOGCONFIG(TAG, "  Default Target Temperature High: %.1f°C", this->normal_config_.default_temperature);
  }
  if (this->supports_two_points_)
    ESP_LOGCONFIG(TAG, "  Minimum Set Point Differential: %.1f°C", this->set_point_minimum_differential_);
  ESP_LOGCONFIG(TAG, "  Start-up Delay Enabled: %s", YESNO(this->use_startup_delay_));
  if (this->supports_cool_) {
    ESP_LOGCONFIG(TAG, "  Cooling Parameters:");
    ESP_LOGCONFIG(TAG, "    Deadband: %.1f°C", this->cooling_deadband_);
    ESP_LOGCONFIG(TAG, "    Overrun: %.1f°C", this->cooling_overrun_);
    if ((this->supplemental_cool_delta_ > 0) || (this->timer_duration_(thermostat::TIMER_COOLING_MAX_RUN_TIME) > 0)) {
      ESP_LOGCONFIG(TAG, "    Supplemental Delta: %.1f°C", this->supplemental_cool_delta_);
      ESP_LOGCONFIG(TAG, "    Maximum Run Time: %us",
                    this->timer_duration_(thermostat::TIMER_COOLING_MAX_RUN_TIME) / 1000);
    }
    ESP_LOGCONFIG(TAG, "    Minimum Off Time: %us", this->timer_duration_(thermostat::TIMER_COOLING_OFF) / 1000);
    ESP_LOGCONFIG(TAG, "    Minimum Run Time: %us", this->timer_duration_(thermostat::TIMER_COOLING_ON) / 1000);
  }
  if (this->supports_heat_) {
    ESP_LOGCONFIG(TAG, "  Heating Parameters:");
    ESP_LOGCONFIG(TAG, "    Deadband: %.1f°C", this->heating_deadband_);
    ESP_LOGCONFIG(TAG, "    Overrun: %.1f°C", this->heating_overrun_);
    if ((this->supplemental_heat_delta_ > 0) || (this->timer_duration_(thermostat::TIMER_HEATING_MAX_RUN_TIME) > 0)) {
      ESP_LOGCONFIG(TAG, "    Supplemental Delta: %.1f°C", this->supplemental_heat_delta_);
      ESP_LOGCONFIG(TAG, "    Maximum Run Time: %us",
                    this->timer_duration_(thermostat::TIMER_HEATING_MAX_RUN_TIME) / 1000);
    }
    ESP_LOGCONFIG(TAG, "    Minimum Off Time: %us", this->timer_duration_(thermostat::TIMER_HEATING_OFF) / 1000);
    ESP_LOGCONFIG(TAG, "    Minimum Run Time: %us", this->timer_duration_(thermostat::TIMER_HEATING_ON) / 1000);
  }
  if (this->supports_fan_only_) {
    ESP_LOGCONFIG(TAG, "  Fanning Minimum Off Time: %us", this->timer_duration_(thermostat::TIMER_FANNING_OFF) / 1000);
    ESP_LOGCONFIG(TAG, "  Fanning Minimum Run Time: %us", this->timer_duration_(thermostat::TIMER_FANNING_ON) / 1000);
  }
  if (this->supports_fan_mode_on_ || this->supports_fan_mode_off_ || this->supports_fan_mode_auto_ ||
      this->supports_fan_mode_low_ || this->supports_fan_mode_medium_ || this->supports_fan_mode_high_ ||
      this->supports_fan_mode_middle_ || this->supports_fan_mode_focus_ || this->supports_fan_mode_diffuse_) {
    ESP_LOGCONFIG(TAG, "  Minimum Fan Mode Switching Time: %us",
                  this->timer_duration_(thermostat::TIMER_FAN_MODE) / 1000);
  }
  ESP_LOGCONFIG(TAG, "  Minimum Idle Time: %us", this->timer_[thermostat::TIMER_IDLE_ON].time / 1000);
  ESP_LOGCONFIG(TAG, "  Supports AUTO: %s", YESNO(this->supports_auto_));
  ESP_LOGCONFIG(TAG, "  Supports HEAT/COOL: %s", YESNO(this->supports_heat_cool_));
  ESP_LOGCONFIG(TAG, "  Supports COOL: %s", YESNO(this->supports_cool_));
  ESP_LOGCONFIG(TAG, "  Supports DRY: %s", YESNO(this->supports_dry_));
  ESP_LOGCONFIG(TAG, "  Supports FAN_ONLY: %s", YESNO(this->supports_fan_only_));
  ESP_LOGCONFIG(TAG, "  Supports FAN_ONLY_ACTION_USES_FAN_MODE_TIMER: %s",
                YESNO(this->supports_fan_only_action_uses_fan_mode_timer_));
  ESP_LOGCONFIG(TAG, "  Supports FAN_ONLY_COOLING: %s", YESNO(this->supports_fan_only_cooling_));
  if (this->supports_cool_)
    ESP_LOGCONFIG(TAG, "  Supports FAN_WITH_COOLING: %s", YESNO(this->supports_fan_with_cooling_));
  if (this->supports_heat_)
    ESP_LOGCONFIG(TAG, "  Supports FAN_WITH_HEATING: %s", YESNO(this->supports_fan_with_heating_));
  ESP_LOGCONFIG(TAG, "  Supports HEAT: %s", YESNO(this->supports_heat_));
  ESP_LOGCONFIG(TAG, "  Supports FAN MODE ON: %s", YESNO(this->supports_fan_mode_on_));
  ESP_LOGCONFIG(TAG, "  Supports FAN MODE OFF: %s", YESNO(this->supports_fan_mode_off_));
  ESP_LOGCONFIG(TAG, "  Supports FAN MODE AUTO: %s", YESNO(this->supports_fan_mode_auto_));
  ESP_LOGCONFIG(TAG, "  Supports FAN MODE LOW: %s", YESNO(this->supports_fan_mode_low_));
  ESP_LOGCONFIG(TAG, "  Supports FAN MODE MEDIUM: %s", YESNO(this->supports_fan_mode_medium_));
  ESP_LOGCONFIG(TAG, "  Supports FAN MODE HIGH: %s", YESNO(this->supports_fan_mode_high_));
  ESP_LOGCONFIG(TAG, "  Supports FAN MODE MIDDLE: %s", YESNO(this->supports_fan_mode_middle_));
  ESP_LOGCONFIG(TAG, "  Supports FAN MODE FOCUS: %s", YESNO(this->supports_fan_mode_focus_));
  ESP_LOGCONFIG(TAG, "  Supports FAN MODE DIFFUSE: %s", YESNO(this->supports_fan_mode_diffuse_));
  ESP_LOGCONFIG(TAG, "  Supports SWING MODE BOTH: %s", YESNO(this->supports_swing_mode_both_));
  ESP_LOGCONFIG(TAG, "  Supports SWING MODE OFF: %s", YESNO(this->supports_swing_mode_off_));
  ESP_LOGCONFIG(TAG, "  Supports SWING MODE HORIZONTAL: %s", YESNO(this->supports_swing_mode_horizontal_));
  ESP_LOGCONFIG(TAG, "  Supports SWING MODE VERTICAL: %s", YESNO(this->supports_swing_mode_vertical_));
  ESP_LOGCONFIG(TAG, "  Supports TWO SET POINTS: %s", YESNO(this->supports_two_points_));
  ESP_LOGCONFIG(TAG, "  Supports AWAY mode: %s", YESNO(this->supports_away_));
  if (this->supports_away_) {
    if (this->supports_heat_) {
      if (this->supports_two_points_)
        ESP_LOGCONFIG(TAG, "    Away Default Target Temperature Low: %.1f°C",
                      this->away_config_.default_temperature_low);
      else
        ESP_LOGCONFIG(TAG, "    Away Default Target Temperature Low: %.1f°C", this->away_config_.default_temperature);
    }
    if ((this->supports_cool_) || (this->supports_fan_only_)) {
      if (this->supports_two_points_)
        ESP_LOGCONFIG(TAG, "    Away Default Target Temperature High: %.1f°C",
                      this->away_config_.default_temperature_high);
      else
        ESP_LOGCONFIG(TAG, "    Away Default Target Temperature High: %.1f°C", this->away_config_.default_temperature);
    }
  }
}

ThermostatClimateTargetTempConfig::ThermostatClimateTargetTempConfig() = default;

ThermostatClimateTargetTempConfig::ThermostatClimateTargetTempConfig(float default_temperature)
    : default_temperature(default_temperature) {}

ThermostatClimateTargetTempConfig::ThermostatClimateTargetTempConfig(float default_temperature_low,
                                                                     float default_temperature_high)
    : default_temperature_low(default_temperature_low), default_temperature_high(default_temperature_high) {}

}  // namespace thermostat
}  // namespace esphome
