#include "thermostat_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace thermostat {

static const char *const TAG = "thermostat.climate";

void ThermostatClimate::setup() {
  // add a callback so that whenever the sensor state changes we can take action
  this->sensor_->add_on_state_callback([this](float state) {
    this->current_temperature = state;
    // required action may have changed, recompute, refresh
    this->switch_to_action_(compute_action_());
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
  // refresh the climate action based on the restored settings
  this->switch_to_action_(compute_action_());
  this->setup_complete_ = true;
  this->publish_state();
}

float ThermostatClimate::cool_deadband() { return this->cool_deadband_; }
float ThermostatClimate::cool_overrun() { return this->cool_overrun_; }
float ThermostatClimate::heat_deadband() { return this->heat_deadband_; }
float ThermostatClimate::heat_overrun() { return this->heat_overrun_; }

void ThermostatClimate::refresh() {
  this->switch_to_mode_(this->mode);
  this->switch_to_action_(compute_action_());
  this->switch_to_fan_mode_(this->fan_mode.value());
  this->switch_to_swing_mode_(this->swing_mode);
  this->check_temperature_change_trigger_();
  this->publish_state();
}

void ThermostatClimate::validate_target_temperature() {
  if (isnan(this->target_temperature)) {
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
    validate_target_temperature_low();
    validate_target_temperature_high();
  } else {
    validate_target_temperature();
  }
}

void ThermostatClimate::validate_target_temperature_low() {
  if (isnan(this->target_temperature_low)) {
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
  if (isnan(this->target_temperature_high)) {
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

climate::ClimateAction ThermostatClimate::compute_action_() {
  climate::ClimateAction target_action = climate::CLIMATE_ACTION_IDLE;
  // if any hysteresis values or current_temperature is not valid, we go to OFF;
  // if the climate mode is OFF then the climate action must be OFF
  if (isnan(this->current_temperature) || isnan(this->cool_deadband_) || isnan(this->cool_overrun_) ||
      isnan(this->heat_deadband_) || isnan(this->heat_overrun_) || (this->mode == climate::CLIMATE_MODE_OFF)) {
    return climate::CLIMATE_ACTION_OFF;
  }
  // ensure set point(s) is/are valid before computing the action
  this->validate_target_temperatures();
  // everything has been validated so we can now safely compute the action
  // first, eliminate a some possible action/mode conflicts
  if (((this->action == climate::CLIMATE_ACTION_FAN) && (this->mode != climate::CLIMATE_MODE_FAN_ONLY)) ||
      ((this->action == climate::CLIMATE_ACTION_DRYING) && (this->mode != climate::CLIMATE_MODE_DRY))) {
    target_action = climate::CLIMATE_ACTION_IDLE;
  }

  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      if (fanning_required_())
        target_action = climate::CLIMATE_ACTION_FAN;
      break;
    case climate::CLIMATE_MODE_DRY:
      target_action = climate::CLIMATE_ACTION_DRYING;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      if (cooling_required_() && heating_required_()) {
        // this is bad and should never happen, so just stop.
        // target_action = climate::CLIMATE_ACTION_IDLE;
      } else if (cooling_required_()) {
        target_action = climate::CLIMATE_ACTION_COOLING;
      } else if (heating_required_()) {
        target_action = climate::CLIMATE_ACTION_HEATING;
      }
      break;
    case climate::CLIMATE_MODE_COOL:
      if (cooling_required_()) {
        target_action = climate::CLIMATE_ACTION_COOLING;
      }
      break;
    case climate::CLIMATE_MODE_HEAT:
      if (heating_required_()) {
        target_action = climate::CLIMATE_ACTION_HEATING;
      }
      break;
    default:
      break;
  }
  return target_action;
}

void ThermostatClimate::switch_to_action_(climate::ClimateAction action) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((action == this->action) && this->setup_complete_)
    // already in target mode
    return;

  if (((action == climate::CLIMATE_ACTION_OFF && this->action == climate::CLIMATE_ACTION_IDLE) ||
       (action == climate::CLIMATE_ACTION_IDLE && this->action == climate::CLIMATE_ACTION_OFF)) &&
      this->setup_complete_) {
    // switching from OFF to IDLE or vice-versa
    // these only have visual difference. OFF means user manually disabled,
    // IDLE means it's in auto mode but value is in target range.
    this->action = action;
    return;
  }

  if (this->prev_action_trigger_ != nullptr) {
    this->prev_action_trigger_->stop_action();
    this->prev_action_trigger_ = nullptr;
  }
  Trigger<> *trig = this->idle_action_trigger_;
  switch (action) {
    case climate::CLIMATE_ACTION_OFF:
    case climate::CLIMATE_ACTION_IDLE:
      // trig = this->idle_action_trigger_;
      break;
    case climate::CLIMATE_ACTION_COOLING:
      trig = this->cool_action_trigger_;
      break;
    case climate::CLIMATE_ACTION_HEATING:
      trig = this->heat_action_trigger_;
      break;
    case climate::CLIMATE_ACTION_FAN:
      trig = this->fan_only_action_trigger_;
      break;
    case climate::CLIMATE_ACTION_DRYING:
      trig = this->dry_action_trigger_;
      break;
    default:
      // we cannot report an invalid mode back to HA (even if it asked for one)
      //  and must assume some valid value
      action = climate::CLIMATE_ACTION_OFF;
      // trig = this->idle_action_trigger_;
  }
  assert(trig != nullptr);
  trig->trigger();
  this->action = action;
  this->prev_action_trigger_ = trig;
}

void ThermostatClimate::switch_to_fan_mode_(climate::ClimateFanMode fan_mode) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((fan_mode == this->prev_fan_mode_) && this->setup_complete_)
    // already in target mode
    return;

  if (this->prev_fan_mode_trigger_ != nullptr) {
    this->prev_fan_mode_trigger_->stop_action();
    this->prev_fan_mode_trigger_ = nullptr;
  }
  Trigger<> *trig = this->fan_mode_auto_trigger_;
  switch (fan_mode) {
    case climate::CLIMATE_FAN_ON:
      trig = this->fan_mode_on_trigger_;
      break;
    case climate::CLIMATE_FAN_OFF:
      trig = this->fan_mode_off_trigger_;
      break;
    case climate::CLIMATE_FAN_AUTO:
      // trig = this->fan_mode_auto_trigger_;
      break;
    case climate::CLIMATE_FAN_LOW:
      trig = this->fan_mode_low_trigger_;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      trig = this->fan_mode_medium_trigger_;
      break;
    case climate::CLIMATE_FAN_HIGH:
      trig = this->fan_mode_high_trigger_;
      break;
    case climate::CLIMATE_FAN_MIDDLE:
      trig = this->fan_mode_middle_trigger_;
      break;
    case climate::CLIMATE_FAN_FOCUS:
      trig = this->fan_mode_focus_trigger_;
      break;
    case climate::CLIMATE_FAN_DIFFUSE:
      trig = this->fan_mode_diffuse_trigger_;
      break;
    default:
      // we cannot report an invalid mode back to HA (even if it asked for one)
      //  and must assume some valid value
      fan_mode = climate::CLIMATE_FAN_AUTO;
      // trig = this->fan_mode_auto_trigger_;
  }
  assert(trig != nullptr);
  trig->trigger();
  this->fan_mode = fan_mode;
  this->prev_fan_mode_ = fan_mode;
  this->prev_fan_mode_trigger_ = trig;
}

void ThermostatClimate::switch_to_mode_(climate::ClimateMode mode) {
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
}

void ThermostatClimate::switch_to_swing_mode_(climate::ClimateSwingMode swing_mode) {
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
  if (this->supports_cool_) {
    if (this->current_temperature > (this->target_temperature_high + this->cool_deadband_)) {
      // if the current temperature exceeds the target + deadband, cooling is required
      return true;
    } else if (this->current_temperature < (this->target_temperature_high - this->cool_overrun_)) {
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
  if (this->supports_fan_only_) {
    if (this->supports_fan_only_cooling_) {
      if (this->current_temperature > (this->target_temperature_high + this->cool_deadband_)) {
        // if the current temperature exceeds the target + deadband, fanning is required
        return true;
      } else if (this->current_temperature < (this->target_temperature_high - this->cool_overrun_)) {
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
  if (this->supports_heat_) {
    if (this->current_temperature < this->target_temperature_low - this->heat_deadband_) {
      // if the current temperature is below the target - deadband, heating is required
      return true;
    } else if (this->current_temperature > this->target_temperature_low + this->heat_overrun_) {
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
      cool_mode_trigger_(new Trigger<>()),
      dry_action_trigger_(new Trigger<>()),
      dry_mode_trigger_(new Trigger<>()),
      heat_action_trigger_(new Trigger<>()),
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
void ThermostatClimate::set_cool_deadband(float deadband) { this->cool_deadband_ = deadband; }
void ThermostatClimate::set_cool_overrun(float overrun) { this->cool_overrun_ = overrun; }
void ThermostatClimate::set_heat_deadband(float deadband) { this->heat_deadband_ = deadband; }
void ThermostatClimate::set_heat_overrun(float overrun) { this->heat_overrun_ = overrun; }
void ThermostatClimate::set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
void ThermostatClimate::set_supports_heat_cool(bool supports_heat_cool) {
  this->supports_heat_cool_ = supports_heat_cool;
}
void ThermostatClimate::set_supports_auto(bool supports_auto) { this->supports_auto_ = supports_auto; }
void ThermostatClimate::set_supports_cool(bool supports_cool) { this->supports_cool_ = supports_cool; }
void ThermostatClimate::set_supports_dry(bool supports_dry) { this->supports_dry_ = supports_dry; }
void ThermostatClimate::set_supports_fan_only(bool supports_fan_only) { this->supports_fan_only_ = supports_fan_only; }
void ThermostatClimate::set_supports_fan_only_cooling(bool supports_fan_only_cooling) {
  this->supports_fan_only_cooling_ = supports_fan_only_cooling;
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
Trigger<> *ThermostatClimate::get_dry_action_trigger() const { return this->dry_action_trigger_; }
Trigger<> *ThermostatClimate::get_fan_only_action_trigger() const { return this->fan_only_action_trigger_; }
Trigger<> *ThermostatClimate::get_heat_action_trigger() const { return this->heat_action_trigger_; }
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
  if ((this->supports_cool_) || (this->supports_fan_only_)) {
    if (this->supports_two_points_)
      ESP_LOGCONFIG(TAG, "  Default Target Temperature High: %.1f°C", this->normal_config_.default_temperature_high);
    else
      ESP_LOGCONFIG(TAG, "  Default Target Temperature High: %.1f°C", this->normal_config_.default_temperature);
  }
  ESP_LOGCONFIG(TAG, "  Minimum Set Point Differential: %.1f°C", this->set_point_minimum_differential_);
  ESP_LOGCONFIG(TAG, "  Cool Deadband: %.1f°C", this->cool_deadband_);
  ESP_LOGCONFIG(TAG, "  Cool Overrun: %.1f°C", this->cool_overrun_);
  ESP_LOGCONFIG(TAG, "  Heat Deadband: %.1f°C", this->heat_deadband_);
  ESP_LOGCONFIG(TAG, "  Heat Overrun: %.1f°C", this->heat_overrun_);
  ESP_LOGCONFIG(TAG, "  Supports AUTO: %s", YESNO(this->supports_auto_));
  ESP_LOGCONFIG(TAG, "  Supports HEAT/COOL: %s", YESNO(this->supports_heat_cool_));
  ESP_LOGCONFIG(TAG, "  Supports COOL: %s", YESNO(this->supports_cool_));
  ESP_LOGCONFIG(TAG, "  Supports DRY: %s", YESNO(this->supports_dry_));
  ESP_LOGCONFIG(TAG, "  Supports FAN_ONLY: %s", YESNO(this->supports_fan_only_));
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