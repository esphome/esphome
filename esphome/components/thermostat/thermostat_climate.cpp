#include "thermostat_climate.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace thermostat {

static const char *const TAG = "thermostat.climate";

void ThermostatClimate::setup() {
  if (this->use_startup_delay_) {
    // start timers so that no actions are called for a moment
    this->start_timer_(thermostat::THERMOSTAT_TIMER_COOLING_OFF);
    this->start_timer_(thermostat::THERMOSTAT_TIMER_FANNING_OFF);
    this->start_timer_(thermostat::THERMOSTAT_TIMER_HEATING_OFF);
    if (this->supports_fan_only_action_uses_fan_mode_timer_)
      this->start_timer_(thermostat::THERMOSTAT_TIMER_FAN_MODE);
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

  // register for humidity values and get initial state
  if (this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->add_on_state_callback([this](float state) {
      this->current_humidity = state;
      this->switch_to_humidity_control_action_(this->compute_humidity_control_action_());
      this->publish_state();
    });
    this->current_humidity = this->humidity_sensor_->state;
  }

  auto use_default_preset = true;

  if (this->on_boot_restore_from_ == thermostat::OnBootRestoreFrom::MEMORY) {
    // restore all climate data, if possible
    auto restore = this->restore_state_();
    if (restore.has_value()) {
      use_default_preset = false;
      restore->to_call(this).perform();
    }
  }

  // Either we failed to restore state or the user has requested we always apply the default preset
  if (use_default_preset) {
    if (this->default_preset_ != climate::ClimatePreset::CLIMATE_PRESET_NONE) {
      this->change_preset_(this->default_preset_);
    } else if (this->default_custom_preset_ != nullptr) {
      this->change_custom_preset_(this->default_custom_preset_);
    }
  }

  // refresh the climate action based on the restored settings, we'll publish_state() later
  this->switch_to_action_(this->compute_action_(), false);
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
  this->setup_complete_ = true;
  this->publish_state();
}

void ThermostatClimate::loop() {
  for (auto &timer : this->timer_) {
    if (timer.active && (timer.started + timer.time < App.get_loop_component_start_time())) {
      timer.active = false;
      timer.func();
    }
  }
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
  this->switch_to_humidity_control_action_(this->compute_humidity_control_action_());
  this->check_humidity_change_trigger_();
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

bool ThermostatClimate::humidity_hysteresis_valid() {
  return !std::isnan(this->humidity_hysteresis_) && this->humidity_hysteresis_ >= 0.0f &&
         this->humidity_hysteresis_ < 100.0f;
}

bool ThermostatClimate::limit_setpoints_for_heat_cool() {
  return this->mode == climate::CLIMATE_MODE_HEAT_COOL ||
         (this->mode == climate::CLIMATE_MODE_AUTO && this->supports_heat_cool_);
}

void ThermostatClimate::validate_target_temperature() {
  if (std::isnan(this->target_temperature)) {
    // default to the midpoint between visual min and max
    this->target_temperature =
        ((this->get_traits().get_visual_max_temperature() - this->get_traits().get_visual_min_temperature()) / 2) +
        this->get_traits().get_visual_min_temperature();
  } else {
    // target_temperature must be between the visual minimum and the visual maximum
    this->target_temperature = clamp(this->target_temperature, this->get_traits().get_visual_min_temperature(),
                                     this->get_traits().get_visual_max_temperature());
  }
}

void ThermostatClimate::validate_target_temperatures(const bool pin_target_temperature_high) {
  if (!this->supports_two_points_) {
    this->validate_target_temperature();
  } else if (pin_target_temperature_high) {
    // if target_temperature_high is set less than target_temperature_low, move down target_temperature_low
    this->validate_target_temperature_low();
    this->validate_target_temperature_high();
  } else {
    // if target_temperature_low is set greater than target_temperature_high, move up target_temperature_high
    this->validate_target_temperature_high();
    this->validate_target_temperature_low();
  }
}

void ThermostatClimate::validate_target_temperature_low() {
  if (std::isnan(this->target_temperature_low)) {
    this->target_temperature_low = this->get_traits().get_visual_min_temperature();
  } else {
    float target_temperature_low_upper_limit =
        this->limit_setpoints_for_heat_cool()
            ? clamp(this->target_temperature_high - this->set_point_minimum_differential_,
                    this->get_traits().get_visual_min_temperature(), this->get_traits().get_visual_max_temperature())
            : this->get_traits().get_visual_max_temperature();
    this->target_temperature_low = clamp(this->target_temperature_low, this->get_traits().get_visual_min_temperature(),
                                         target_temperature_low_upper_limit);
  }
}

void ThermostatClimate::validate_target_temperature_high() {
  if (std::isnan(this->target_temperature_high)) {
    this->target_temperature_high = this->get_traits().get_visual_max_temperature();
  } else {
    float target_temperature_high_lower_limit =
        this->limit_setpoints_for_heat_cool()
            ? clamp(this->target_temperature_low + this->set_point_minimum_differential_,
                    this->get_traits().get_visual_min_temperature(), this->get_traits().get_visual_max_temperature())
            : this->get_traits().get_visual_min_temperature();
    this->target_temperature_high = clamp(this->target_temperature_high, target_temperature_high_lower_limit,
                                          this->get_traits().get_visual_max_temperature());
  }
}

void ThermostatClimate::validate_target_humidity() {
  if (std::isnan(this->target_humidity)) {
    this->target_humidity =
        (this->get_traits().get_visual_max_humidity() - this->get_traits().get_visual_min_humidity()) / 2.0f;
  } else {
    this->target_humidity = clamp<float>(this->target_humidity, this->get_traits().get_visual_min_humidity(),
                                         this->get_traits().get_visual_max_humidity());
  }
}

void ThermostatClimate::control(const climate::ClimateCall &call) {
  bool target_temperature_high_changed = false;

  if (call.get_preset().has_value()) {
    // setup_complete_ blocks modifying/resetting the temps immediately after boot
    if (this->setup_complete_) {
      this->change_preset_(call.get_preset().value());
    } else {
      this->preset = call.get_preset().value();
    }
  }
  if (call.has_custom_preset()) {
    // setup_complete_ blocks modifying/resetting the temps immediately after boot
    if (this->setup_complete_) {
      this->change_custom_preset_(call.get_custom_preset());
    } else {
      // Use the base class method which handles pointer lookup internally
      this->set_custom_preset_(call.get_custom_preset());
    }
  }

  if (call.get_mode().has_value()) {
    this->mode = call.get_mode().value();
  }
  if (call.get_fan_mode().has_value()) {
    this->fan_mode = call.get_fan_mode().value();
  }
  if (call.get_swing_mode().has_value()) {
    this->swing_mode = call.get_swing_mode().value();
  }
  if (this->supports_two_points_) {
    if (call.get_target_temperature_low().has_value()) {
      this->target_temperature_low = call.get_target_temperature_low().value();
    }
    if (call.get_target_temperature_high().has_value()) {
      target_temperature_high_changed = this->target_temperature_high != call.get_target_temperature_high().value();
      this->target_temperature_high = call.get_target_temperature_high().value();
    }
    // ensure the two set points are valid and adjust one of them if necessary
    this->validate_target_temperatures(target_temperature_high_changed ||
                                       (this->prev_mode_ == climate::CLIMATE_MODE_COOL));
  } else {
    if (call.get_target_temperature().has_value()) {
      this->target_temperature = call.get_target_temperature().value();
      this->validate_target_temperature();
    }
  }
  if (call.get_target_humidity().has_value()) {
    this->target_humidity = call.get_target_humidity().value();
    this->validate_target_humidity();
  }
  // make any changes happen
  this->refresh();
}

climate::ClimateTraits ThermostatClimate::traits() {
  auto traits = climate::ClimateTraits();

  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION | climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);

  if (this->supports_two_points_)
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);

  if (this->humidity_sensor_ != nullptr)
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY);

  if (this->supports_humidification_ || this->supports_dehumidification_)
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY);

  if (this->supports_auto_)
    traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);
  if (this->supports_heat_cool_)
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT_COOL);
  if (this->supports_cool_)
    traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  if (this->supports_dry_)
    traits.add_supported_mode(climate::CLIMATE_MODE_DRY);
  if (this->supports_fan_only_)
    traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);
  if (this->supports_heat_)
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);

  if (this->supports_fan_mode_on_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_ON);
  if (this->supports_fan_mode_off_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_OFF);
  if (this->supports_fan_mode_auto_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
  if (this->supports_fan_mode_low_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_LOW);
  if (this->supports_fan_mode_medium_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_MEDIUM);
  if (this->supports_fan_mode_high_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_HIGH);
  if (this->supports_fan_mode_middle_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_MIDDLE);
  if (this->supports_fan_mode_focus_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_FOCUS);
  if (this->supports_fan_mode_diffuse_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_DIFFUSE);
  if (this->supports_fan_mode_quiet_)
    traits.add_supported_fan_mode(climate::CLIMATE_FAN_QUIET);

  if (this->supports_swing_mode_both_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
  if (this->supports_swing_mode_horizontal_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
  if (this->supports_swing_mode_off_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
  if (this->supports_swing_mode_vertical_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);

  for (const auto &entry : this->preset_config_) {
    traits.add_supported_preset(entry.preset);
  }

  // Extract custom preset names from the custom_preset_config_ vector
  if (!this->custom_preset_config_.empty()) {
    std::vector<const char *> custom_preset_names;
    custom_preset_names.reserve(this->custom_preset_config_.size());
    for (const auto &entry : this->custom_preset_config_) {
      custom_preset_names.push_back(entry.name);
    }
    traits.set_supported_custom_presets(custom_preset_names);
  }

  return traits;
}

climate::ClimateAction ThermostatClimate::compute_action_(const bool ignore_timers) {
  auto target_action = climate::CLIMATE_ACTION_IDLE;
  // if any hysteresis values or current_temperature is not valid, we go to OFF;
  if (std::isnan(this->current_temperature) || !this->hysteresis_valid()) {
    return climate::CLIMATE_ACTION_OFF;
  }
  // do not change the action if an "ON" timer is running
  if ((!ignore_timers) && (this->timer_active_(thermostat::THERMOSTAT_TIMER_IDLE_ON) ||
                           this->timer_active_(thermostat::THERMOSTAT_TIMER_COOLING_ON) ||
                           this->timer_active_(thermostat::THERMOSTAT_TIMER_FANNING_ON) ||
                           this->timer_active_(thermostat::THERMOSTAT_TIMER_HEATING_ON))) {
    return this->action;
  }

  // ensure set point(s) is/are valid before computing the action
  this->validate_target_temperatures(this->prev_mode_ == climate::CLIMATE_MODE_COOL);
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
    case climate::CLIMATE_MODE_AUTO:
      if (this->supports_two_points_) {
        if (this->cooling_required_() && this->heating_required_()) {
          // this is bad and should never happen, so just stop.
          // target_action = climate::CLIMATE_ACTION_IDLE;
        } else if (this->cooling_required_()) {
          target_action = climate::CLIMATE_ACTION_COOLING;
        } else if (this->heating_required_()) {
          target_action = climate::CLIMATE_ACTION_HEATING;
        }
      } else if (this->supports_cool_ && this->cooling_required_()) {
        target_action = climate::CLIMATE_ACTION_COOLING;
      } else if (this->supports_heat_ && this->heating_required_()) {
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
  this->validate_target_temperatures(this->prev_mode_ == climate::CLIMATE_MODE_COOL);
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

HumidificationAction ThermostatClimate::compute_humidity_control_action_() {
  auto target_action = THERMOSTAT_HUMIDITY_CONTROL_ACTION_OFF;
  // if hysteresis value or current_humidity is not valid, we go to OFF
  if (std::isnan(this->current_humidity) || !this->humidity_hysteresis_valid()) {
    return THERMOSTAT_HUMIDITY_CONTROL_ACTION_OFF;
  }

  // ensure set point is valid before computing the action
  this->validate_target_humidity();
  // everything has been validated so we can now safely compute the action
  if (this->dehumidification_required_() && this->humidification_required_()) {
    // this is bad and should never happen, so just stop.
    // target_action = THERMOSTAT_HUMIDITY_CONTROL_ACTION_OFF;
  } else if (this->supports_dehumidification_ && this->dehumidification_required_()) {
    target_action = THERMOSTAT_HUMIDITY_CONTROL_ACTION_DEHUMIDIFY;
  } else if (this->supports_humidification_ && this->humidification_required_()) {
    target_action = THERMOSTAT_HUMIDITY_CONTROL_ACTION_HUMIDIFY;
  }

  return target_action;
}

void ThermostatClimate::switch_to_action_(climate::ClimateAction action, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((action == this->action) && this->setup_complete_) {
    // already in target mode
    return;
  }

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
        this->start_timer_(thermostat::THERMOSTAT_TIMER_IDLE_ON);
        if (this->action == climate::CLIMATE_ACTION_COOLING)
          this->start_timer_(thermostat::THERMOSTAT_TIMER_COOLING_OFF);
        if (this->action == climate::CLIMATE_ACTION_FAN) {
          if (this->supports_fan_only_action_uses_fan_mode_timer_) {
            this->start_timer_(thermostat::THERMOSTAT_TIMER_FAN_MODE);
          } else {
            this->start_timer_(thermostat::THERMOSTAT_TIMER_FANNING_OFF);
          }
        }
        if (this->action == climate::CLIMATE_ACTION_HEATING)
          this->start_timer_(thermostat::THERMOSTAT_TIMER_HEATING_OFF);
        // trig = this->idle_action_trigger_;
        ESP_LOGVV(TAG, "Switching to IDLE/OFF action");
        this->cooling_max_runtime_exceeded_ = false;
        this->heating_max_runtime_exceeded_ = false;
        action_ready = true;
      }
      break;
    case climate::CLIMATE_ACTION_COOLING:
      if (this->cooling_action_ready_()) {
        this->start_timer_(thermostat::THERMOSTAT_TIMER_COOLING_ON);
        this->start_timer_(thermostat::THERMOSTAT_TIMER_COOLING_MAX_RUN_TIME);
        if (this->supports_fan_with_cooling_) {
          this->start_timer_(thermostat::THERMOSTAT_TIMER_FANNING_ON);
          trig_fan = this->fan_only_action_trigger_;
        }
        this->cooling_max_runtime_exceeded_ = false;
        trig = this->cool_action_trigger_;
        ESP_LOGVV(TAG, "Switching to COOLING action");
        action_ready = true;
      }
      break;
    case climate::CLIMATE_ACTION_HEATING:
      if (this->heating_action_ready_()) {
        this->start_timer_(thermostat::THERMOSTAT_TIMER_HEATING_ON);
        this->start_timer_(thermostat::THERMOSTAT_TIMER_HEATING_MAX_RUN_TIME);
        if (this->supports_fan_with_heating_) {
          this->start_timer_(thermostat::THERMOSTAT_TIMER_FANNING_ON);
          trig_fan = this->fan_only_action_trigger_;
        }
        this->heating_max_runtime_exceeded_ = false;
        trig = this->heat_action_trigger_;
        ESP_LOGVV(TAG, "Switching to HEATING action");
        action_ready = true;
      }
      break;
    case climate::CLIMATE_ACTION_FAN:
      if (this->fanning_action_ready_()) {
        if (this->supports_fan_only_action_uses_fan_mode_timer_) {
          this->start_timer_(thermostat::THERMOSTAT_TIMER_FAN_MODE);
        } else {
          this->start_timer_(thermostat::THERMOSTAT_TIMER_FANNING_ON);
        }
        trig = this->fan_only_action_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_ONLY action");
        action_ready = true;
      }
      break;
    case climate::CLIMATE_ACTION_DRYING:
      if (this->drying_action_ready_()) {
        this->start_timer_(thermostat::THERMOSTAT_TIMER_COOLING_ON);
        this->start_timer_(thermostat::THERMOSTAT_TIMER_FANNING_ON);
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
    if (trig != nullptr) {
      trig->trigger();
    }
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
  if ((action == this->supplemental_action_) && this->setup_complete_) {
    // already in target mode
    return;
  }

  switch (action) {
    case climate::CLIMATE_ACTION_OFF:
    case climate::CLIMATE_ACTION_IDLE:
      this->cancel_timer_(thermostat::THERMOSTAT_TIMER_COOLING_MAX_RUN_TIME);
      this->cancel_timer_(thermostat::THERMOSTAT_TIMER_HEATING_MAX_RUN_TIME);
      break;
    case climate::CLIMATE_ACTION_COOLING:
      this->cancel_timer_(thermostat::THERMOSTAT_TIMER_COOLING_MAX_RUN_TIME);
      break;
    case climate::CLIMATE_ACTION_HEATING:
      this->cancel_timer_(thermostat::THERMOSTAT_TIMER_HEATING_MAX_RUN_TIME);
      break;
    default:
      return;
  }
  ESP_LOGVV(TAG, "Updating supplemental action");
  this->supplemental_action_ = action;
  this->trigger_supplemental_action_();
}

void ThermostatClimate::trigger_supplemental_action_() {
  Trigger<> *trig = nullptr;

  switch (this->supplemental_action_) {
    case climate::CLIMATE_ACTION_COOLING:
      if (!this->timer_active_(thermostat::THERMOSTAT_TIMER_COOLING_MAX_RUN_TIME)) {
        this->start_timer_(thermostat::THERMOSTAT_TIMER_COOLING_MAX_RUN_TIME);
      }
      trig = this->supplemental_cool_action_trigger_;
      ESP_LOGVV(TAG, "Calling supplemental COOLING action");
      break;
    case climate::CLIMATE_ACTION_HEATING:
      if (!this->timer_active_(thermostat::THERMOSTAT_TIMER_HEATING_MAX_RUN_TIME)) {
        this->start_timer_(thermostat::THERMOSTAT_TIMER_HEATING_MAX_RUN_TIME);
      }
      trig = this->supplemental_heat_action_trigger_;
      ESP_LOGVV(TAG, "Calling supplemental HEATING action");
      break;
    default:
      break;
  }

  if (trig != nullptr) {
    trig->trigger();
  }
}

void ThermostatClimate::switch_to_humidity_control_action_(HumidificationAction action) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((action == this->humidification_action) && this->setup_complete_) {
    // already in target mode
    return;
  }

  Trigger<> *trig = this->humidity_control_off_action_trigger_;
  switch (action) {
    case THERMOSTAT_HUMIDITY_CONTROL_ACTION_OFF:
      // trig = this->humidity_control_off_action_trigger_;
      ESP_LOGVV(TAG, "Switching to HUMIDIFICATION_OFF action");
      break;
    case THERMOSTAT_HUMIDITY_CONTROL_ACTION_DEHUMIDIFY:
      trig = this->humidity_control_dehumidify_action_trigger_;
      ESP_LOGVV(TAG, "Switching to DEHUMIDIFY action");
      break;
    case THERMOSTAT_HUMIDITY_CONTROL_ACTION_HUMIDIFY:
      trig = this->humidity_control_humidify_action_trigger_;
      ESP_LOGVV(TAG, "Switching to HUMIDIFY action");
      break;
    case THERMOSTAT_HUMIDITY_CONTROL_ACTION_NONE:
    default:
      action = THERMOSTAT_HUMIDITY_CONTROL_ACTION_OFF;
      // trig = this->humidity_control_off_action_trigger_;
  }

  if (this->prev_humidity_control_trigger_ != nullptr) {
    this->prev_humidity_control_trigger_->stop_action();
    this->prev_humidity_control_trigger_ = nullptr;
  }
  this->humidification_action = action;
  this->prev_humidity_control_trigger_ = trig;
  if (trig != nullptr) {
    trig->trigger();
  }
}

void ThermostatClimate::switch_to_fan_mode_(climate::ClimateFanMode fan_mode, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((fan_mode == this->prev_fan_mode_) && this->setup_complete_) {
    // already in target mode
    return;
  }

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
      case climate::CLIMATE_FAN_QUIET:
        trig = this->fan_mode_quiet_trigger_;
        ESP_LOGVV(TAG, "Switching to FAN_QUIET mode");
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
    this->start_timer_(thermostat::THERMOSTAT_TIMER_FAN_MODE);
    if (trig != nullptr) {
      trig->trigger();
    }
    this->prev_fan_mode_ = fan_mode;
    this->prev_fan_mode_trigger_ = trig;
  }
}

void ThermostatClimate::switch_to_mode_(climate::ClimateMode mode, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((mode == this->prev_mode_) && this->setup_complete_) {
    // already in target mode
    return;
  }

  if (this->prev_mode_trigger_ != nullptr) {
    this->prev_mode_trigger_->stop_action();
    this->prev_mode_trigger_ = nullptr;
  }
  Trigger<> *trig = this->off_mode_trigger_;
  switch (mode) {
    case climate::CLIMATE_MODE_AUTO:
      trig = this->auto_mode_trigger_;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      trig = this->heat_cool_mode_trigger_;
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
    case climate::CLIMATE_MODE_OFF:
    default:
      // we cannot report an invalid mode back to HA (even if it asked for one)
      //  and must assume some valid value
      mode = climate::CLIMATE_MODE_OFF;
      // trig = this->off_mode_trigger_;
  }
  if (trig != nullptr) {
    trig->trigger();
  }
  this->mode = mode;
  this->prev_mode_ = mode;
  this->prev_mode_trigger_ = trig;
  if (publish_state) {
    this->publish_state();
  }
}

void ThermostatClimate::switch_to_swing_mode_(climate::ClimateSwingMode swing_mode, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((swing_mode == this->prev_swing_mode_) && this->setup_complete_) {
    // already in target mode
    return;
  }

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
  if (trig != nullptr) {
    trig->trigger();
  }
  this->swing_mode = swing_mode;
  this->prev_swing_mode_ = swing_mode;
  this->prev_swing_mode_trigger_ = trig;
  if (publish_state)
    this->publish_state();
}

bool ThermostatClimate::idle_action_ready_() {
  if (this->supports_fan_only_action_uses_fan_mode_timer_) {
    return !(this->timer_active_(thermostat::THERMOSTAT_TIMER_COOLING_ON) ||
             this->timer_active_(thermostat::THERMOSTAT_TIMER_FAN_MODE) ||
             this->timer_active_(thermostat::THERMOSTAT_TIMER_HEATING_ON));
  }
  return !(this->timer_active_(thermostat::THERMOSTAT_TIMER_COOLING_ON) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_FANNING_ON) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_HEATING_ON));
}

bool ThermostatClimate::cooling_action_ready_() {
  return !(this->timer_active_(thermostat::THERMOSTAT_TIMER_IDLE_ON) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_FANNING_OFF) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_COOLING_OFF) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_HEATING_ON));
}

bool ThermostatClimate::drying_action_ready_() {
  return !(this->timer_active_(thermostat::THERMOSTAT_TIMER_IDLE_ON) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_FANNING_OFF) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_COOLING_OFF) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_HEATING_ON));
}

bool ThermostatClimate::fan_mode_ready_() { return !(this->timer_active_(thermostat::THERMOSTAT_TIMER_FAN_MODE)); }

bool ThermostatClimate::fanning_action_ready_() {
  if (this->supports_fan_only_action_uses_fan_mode_timer_) {
    return !(this->timer_active_(thermostat::THERMOSTAT_TIMER_FAN_MODE));
  }
  return !(this->timer_active_(thermostat::THERMOSTAT_TIMER_IDLE_ON) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_FANNING_OFF));
}

bool ThermostatClimate::heating_action_ready_() {
  return !(this->timer_active_(thermostat::THERMOSTAT_TIMER_IDLE_ON) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_COOLING_ON) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_FANNING_OFF) ||
           this->timer_active_(thermostat::THERMOSTAT_TIMER_HEATING_OFF));
}

void ThermostatClimate::start_timer_(const ThermostatClimateTimerIndex timer_index) {
  if (this->timer_duration_(timer_index) > 0) {
    this->timer_[timer_index].started = millis();
    this->timer_[timer_index].active = true;
  }
}

bool ThermostatClimate::cancel_timer_(ThermostatClimateTimerIndex timer_index) {
  auto ret = this->timer_[timer_index].active;
  this->timer_[timer_index].active = false;
  return ret;
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
  this->cooling_max_runtime_exceeded_ = true;
  this->trigger_supplemental_action_();
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::cooling_off_timer_callback_() {
  ESP_LOGVV(TAG, "cooling_off timer expired");
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::cooling_on_timer_callback_() {
  ESP_LOGVV(TAG, "cooling_on timer expired");
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::fan_mode_timer_callback_() {
  ESP_LOGVV(TAG, "fan_mode timer expired");
  this->switch_to_fan_mode_(this->fan_mode.value_or(climate::CLIMATE_FAN_ON));
  if (this->supports_fan_only_action_uses_fan_mode_timer_)
    this->switch_to_action_(this->compute_action_());
}

void ThermostatClimate::fanning_off_timer_callback_() {
  ESP_LOGVV(TAG, "fanning_off timer expired");
  this->switch_to_action_(this->compute_action_());
}

void ThermostatClimate::fanning_on_timer_callback_() {
  ESP_LOGVV(TAG, "fanning_on timer expired");
  this->switch_to_action_(this->compute_action_());
}

void ThermostatClimate::heating_max_run_time_timer_callback_() {
  ESP_LOGVV(TAG, "heating_max_run_time timer expired");
  this->heating_max_runtime_exceeded_ = true;
  this->trigger_supplemental_action_();
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::heating_off_timer_callback_() {
  ESP_LOGVV(TAG, "heating_off timer expired");
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::heating_on_timer_callback_() {
  ESP_LOGVV(TAG, "heating_on timer expired");
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::idle_on_timer_callback_() {
  ESP_LOGVV(TAG, "idle_on timer expired");
  this->switch_to_action_(this->compute_action_());
  this->switch_to_supplemental_action_(this->compute_supplemental_action_());
}

void ThermostatClimate::check_humidity_change_trigger_() {
  if ((this->prev_target_humidity_ == this->target_humidity) && this->setup_complete_) {
    return;  // nothing changed, no reason to trigger
  } else {
    // save the new temperature so we can check it again later; the trigger will fire below
    this->prev_target_humidity_ = this->target_humidity;
  }
  // trigger the action
  Trigger<> *trig = this->humidity_change_trigger_;
  if (trig != nullptr) {
    trig->trigger();
  }
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
  if (trig != nullptr) {
    trig->trigger();
  }
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

bool ThermostatClimate::dehumidification_required_() {
  if (this->current_humidity > this->target_humidity + this->humidity_hysteresis_) {
    // if the current humidity exceeds the target + hysteresis, dehumidification is required
    return true;
  } else if (this->current_humidity < this->target_humidity - this->humidity_hysteresis_) {
    // if the current humidity is less than the target - hysteresis, dehumidification should stop
    return false;
  }
  // if we get here, the current humidity is between target + hysteresis and target - hysteresis,
  //  so the action should not change
  return this->humidification_action == THERMOSTAT_HUMIDITY_CONTROL_ACTION_DEHUMIDIFY;
}

bool ThermostatClimate::humidification_required_() {
  if (this->current_humidity < this->target_humidity - this->humidity_hysteresis_) {
    // if the current humidity is below the target - hysteresis, humidification is required
    return true;
  } else if (this->current_humidity > this->target_humidity + this->humidity_hysteresis_) {
    // if the current humidity is above the target + hysteresis, humidification should stop
    return false;
  }
  // if we get here, the current humidity is between target - hysteresis and target + hysteresis,
  //  so the action should not change
  return this->humidification_action == THERMOSTAT_HUMIDITY_CONTROL_ACTION_HUMIDIFY;
}

void ThermostatClimate::dump_preset_config_(const char *preset_name, const ThermostatClimateTargetTempConfig &config) {
  if (this->supports_heat_) {
    ESP_LOGCONFIG(TAG, "      Default Target Temperature Low: %.1f°C",
                  this->supports_two_points_ ? config.default_temperature_low : config.default_temperature);
  }
  if ((this->supports_cool_) || (this->supports_fan_only_)) {
    ESP_LOGCONFIG(TAG, "      Default Target Temperature High: %.1f°C",
                  this->supports_two_points_ ? config.default_temperature_high : config.default_temperature);
  }

  if (config.mode_.has_value()) {
    ESP_LOGCONFIG(TAG, "      Default Mode: %s", LOG_STR_ARG(climate::climate_mode_to_string(*config.mode_)));
  }
  if (config.fan_mode_.has_value()) {
    ESP_LOGCONFIG(TAG, "      Default Fan Mode: %s",
                  LOG_STR_ARG(climate::climate_fan_mode_to_string(*config.fan_mode_)));
  }
  if (config.swing_mode_.has_value()) {
    ESP_LOGCONFIG(TAG, "      Default Swing Mode: %s",
                  LOG_STR_ARG(climate::climate_swing_mode_to_string(*config.swing_mode_)));
  }
}

void ThermostatClimate::change_preset_(climate::ClimatePreset preset) {
  // Linear search through preset configurations
  const ThermostatClimateTargetTempConfig *config = nullptr;
  for (const auto &entry : this->preset_config_) {
    if (entry.preset == preset) {
      config = &entry.config;
      break;
    }
  }

  if (config != nullptr) {
    ESP_LOGV(TAG, "Preset %s requested", LOG_STR_ARG(climate::climate_preset_to_string(preset)));
    if (this->change_preset_internal_(*config) || (!this->preset.has_value()) || this->preset.value() != preset) {
      // Fire any preset changed trigger if defined
      Trigger<> *trig = this->preset_change_trigger_;
      this->set_preset_(preset);
      if (trig != nullptr) {
        trig->trigger();
      }

      this->refresh();
      ESP_LOGI(TAG, "Preset %s applied", LOG_STR_ARG(climate::climate_preset_to_string(preset)));
    } else {
      ESP_LOGI(TAG, "No changes required to apply preset %s", LOG_STR_ARG(climate::climate_preset_to_string(preset)));
    }
  } else {
    ESP_LOGW(TAG, "Preset %s not configured; ignoring", LOG_STR_ARG(climate::climate_preset_to_string(preset)));
  }
}

void ThermostatClimate::change_custom_preset_(const char *custom_preset) {
  // Linear search through custom preset configurations
  const ThermostatClimateTargetTempConfig *config = nullptr;
  for (const auto &entry : this->custom_preset_config_) {
    if (strcmp(entry.name, custom_preset) == 0) {
      config = &entry.config;
      break;
    }
  }

  if (config != nullptr) {
    ESP_LOGV(TAG, "Custom preset %s requested", custom_preset);
    if (this->change_preset_internal_(*config) || !this->has_custom_preset() ||
        strcmp(this->get_custom_preset(), custom_preset) != 0) {
      // Fire any preset changed trigger if defined
      Trigger<> *trig = this->preset_change_trigger_;
      // Use the base class method which handles pointer lookup and preset reset internally
      this->set_custom_preset_(custom_preset);
      if (trig != nullptr) {
        trig->trigger();
      }

      this->refresh();
      ESP_LOGI(TAG, "Custom preset %s applied", custom_preset);
    } else {
      ESP_LOGI(TAG, "No changes required to apply custom preset %s", custom_preset);
      // Note: set_custom_preset_() above handles preset.reset() and custom_preset_ assignment internally.
      // The old code had these lines here unconditionally, which was a bug (double assignment, state modification
      // even when no changes were needed). Now properly handled by the protected setter with mutual exclusion.
    }
  } else {
    ESP_LOGW(TAG, "Custom preset %s not configured; ignoring", custom_preset);
  }
}

bool ThermostatClimate::change_preset_internal_(const ThermostatClimateTargetTempConfig &config) {
  bool something_changed = false;

  if (this->supports_two_points_) {
    if (this->target_temperature_low != config.default_temperature_low) {
      this->target_temperature_low = config.default_temperature_low;
      something_changed = true;
    }
    if (this->target_temperature_high != config.default_temperature_high) {
      this->target_temperature_high = config.default_temperature_high;
      something_changed = true;
    }
  } else {
    if (this->target_temperature != config.default_temperature) {
      this->target_temperature = config.default_temperature;
      something_changed = true;
    }
  }

  // Note: The mode, fan_mode and swing_mode can all be defined in the preset but if the climate.control call
  //  also specifies them then the climate.control call's values will override the preset's values for that call
  if (config.mode_.has_value() && (this->mode != config.mode_.value())) {
    ESP_LOGV(TAG, "Setting mode to %s", LOG_STR_ARG(climate::climate_mode_to_string(*config.mode_)));
    this->mode = *config.mode_;
    something_changed = true;
  }

  if (config.fan_mode_.has_value() && (this->fan_mode != config.fan_mode_.value())) {
    ESP_LOGV(TAG, "Setting fan mode to %s", LOG_STR_ARG(climate::climate_fan_mode_to_string(*config.fan_mode_)));
    this->fan_mode = *config.fan_mode_;
    something_changed = true;
  }

  if (config.swing_mode_.has_value() && (this->swing_mode != config.swing_mode_.value())) {
    ESP_LOGV(TAG, "Setting swing mode to %s", LOG_STR_ARG(climate::climate_swing_mode_to_string(*config.swing_mode_)));
    this->swing_mode = *config.swing_mode_;
    something_changed = true;
  }

  return something_changed;
}

void ThermostatClimate::set_preset_config(std::initializer_list<PresetEntry> presets) {
  this->preset_config_ = presets;
}

void ThermostatClimate::set_custom_preset_config(std::initializer_list<CustomPresetEntry> presets) {
  this->custom_preset_config_ = presets;
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
      heat_cool_mode_trigger_(new Trigger<>()),
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
      fan_mode_quiet_trigger_(new Trigger<>()),
      swing_mode_both_trigger_(new Trigger<>()),
      swing_mode_off_trigger_(new Trigger<>()),
      swing_mode_horizontal_trigger_(new Trigger<>()),
      swing_mode_vertical_trigger_(new Trigger<>()),
      humidity_change_trigger_(new Trigger<>()),
      temperature_change_trigger_(new Trigger<>()),
      preset_change_trigger_(new Trigger<>()),
      humidity_control_dehumidify_action_trigger_(new Trigger<>()),
      humidity_control_humidify_action_trigger_(new Trigger<>()),
      humidity_control_off_action_trigger_(new Trigger<>()) {}

void ThermostatClimate::set_default_preset(const char *custom_preset) {
  // Find the preset in custom_preset_config_ and store pointer from there
  for (const auto &entry : this->custom_preset_config_) {
    if (strcmp(entry.name, custom_preset) == 0) {
      this->default_custom_preset_ = entry.name;
      return;
    }
  }
  // If not found, it will be caught during validation
  this->default_custom_preset_ = nullptr;
}

void ThermostatClimate::set_default_preset(climate::ClimatePreset preset) { this->default_preset_ = preset; }

void ThermostatClimate::set_on_boot_restore_from(thermostat::OnBootRestoreFrom on_boot_restore_from) {
  this->on_boot_restore_from_ = on_boot_restore_from;
}
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
  this->timer_[thermostat::THERMOSTAT_TIMER_COOLING_MAX_RUN_TIME].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_cooling_minimum_off_time_in_sec(uint32_t time) {
  this->timer_[thermostat::THERMOSTAT_TIMER_COOLING_OFF].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_cooling_minimum_run_time_in_sec(uint32_t time) {
  this->timer_[thermostat::THERMOSTAT_TIMER_COOLING_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_fan_mode_minimum_switching_time_in_sec(uint32_t time) {
  this->timer_[thermostat::THERMOSTAT_TIMER_FAN_MODE].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_fanning_minimum_off_time_in_sec(uint32_t time) {
  this->timer_[thermostat::THERMOSTAT_TIMER_FANNING_OFF].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_fanning_minimum_run_time_in_sec(uint32_t time) {
  this->timer_[thermostat::THERMOSTAT_TIMER_FANNING_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_heating_maximum_run_time_in_sec(uint32_t time) {
  this->timer_[thermostat::THERMOSTAT_TIMER_HEATING_MAX_RUN_TIME].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_heating_minimum_off_time_in_sec(uint32_t time) {
  this->timer_[thermostat::THERMOSTAT_TIMER_HEATING_OFF].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_heating_minimum_run_time_in_sec(uint32_t time) {
  this->timer_[thermostat::THERMOSTAT_TIMER_HEATING_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_idle_minimum_time_in_sec(uint32_t time) {
  this->timer_[thermostat::THERMOSTAT_TIMER_IDLE_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void ThermostatClimate::set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
void ThermostatClimate::set_humidity_sensor(sensor::Sensor *humidity_sensor) {
  this->humidity_sensor_ = humidity_sensor;
}
void ThermostatClimate::set_humidity_hysteresis(float humidity_hysteresis) {
  this->humidity_hysteresis_ = std::clamp<float>(humidity_hysteresis, 0.0f, 100.0f);
}
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
void ThermostatClimate::set_supports_fan_mode_quiet(bool supports_fan_mode_quiet) {
  this->supports_fan_mode_quiet_ = supports_fan_mode_quiet;
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
void ThermostatClimate::set_supports_dehumidification(bool supports_dehumidification) {
  this->supports_dehumidification_ = supports_dehumidification;
  if (supports_dehumidification) {
    this->supports_humidification_ = false;
  }
}
void ThermostatClimate::set_supports_humidification(bool supports_humidification) {
  this->supports_humidification_ = supports_humidification;
  if (supports_humidification) {
    this->supports_dehumidification_ = false;
  }
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
Trigger<> *ThermostatClimate::get_heat_cool_mode_trigger() const { return this->heat_cool_mode_trigger_; }
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
Trigger<> *ThermostatClimate::get_fan_mode_quiet_trigger() const { return this->fan_mode_quiet_trigger_; }
Trigger<> *ThermostatClimate::get_swing_mode_both_trigger() const { return this->swing_mode_both_trigger_; }
Trigger<> *ThermostatClimate::get_swing_mode_off_trigger() const { return this->swing_mode_off_trigger_; }
Trigger<> *ThermostatClimate::get_swing_mode_horizontal_trigger() const { return this->swing_mode_horizontal_trigger_; }
Trigger<> *ThermostatClimate::get_swing_mode_vertical_trigger() const { return this->swing_mode_vertical_trigger_; }
Trigger<> *ThermostatClimate::get_humidity_change_trigger() const { return this->humidity_change_trigger_; }
Trigger<> *ThermostatClimate::get_temperature_change_trigger() const { return this->temperature_change_trigger_; }
Trigger<> *ThermostatClimate::get_preset_change_trigger() const { return this->preset_change_trigger_; }
Trigger<> *ThermostatClimate::get_humidity_control_dehumidify_action_trigger() const {
  return this->humidity_control_dehumidify_action_trigger_;
}
Trigger<> *ThermostatClimate::get_humidity_control_humidify_action_trigger() const {
  return this->humidity_control_humidify_action_trigger_;
}
Trigger<> *ThermostatClimate::get_humidity_control_off_action_trigger() const {
  return this->humidity_control_off_action_trigger_;
}

void ThermostatClimate::dump_config() {
  LOG_CLIMATE("", "Thermostat", this);

  ESP_LOGCONFIG(TAG,
                "  On boot, restore from: %s\n"
                "  Use Start-up Delay: %s",
                this->on_boot_restore_from_ == thermostat::DEFAULT_PRESET ? "DEFAULT_PRESET" : "MEMORY",
                YESNO(this->use_startup_delay_));
  if (this->supports_two_points_) {
    ESP_LOGCONFIG(TAG, "  Minimum Set Point Differential: %.1f°C", this->set_point_minimum_differential_);
  }
  if (this->supports_cool_) {
    ESP_LOGCONFIG(TAG,
                  "  Cooling Parameters:\n"
                  "    Deadband: %.1f°C\n"
                  "    Overrun: %.1f°C\n"
                  "    Minimum Off Time: %" PRIu32 "s\n"
                  "    Minimum Run Time: %" PRIu32 "s",
                  this->cooling_deadband_, this->cooling_overrun_,
                  this->timer_duration_(thermostat::THERMOSTAT_TIMER_COOLING_OFF) / 1000,
                  this->timer_duration_(thermostat::THERMOSTAT_TIMER_COOLING_ON) / 1000);
    if ((this->supplemental_cool_delta_ > 0) ||
        (this->timer_duration_(thermostat::THERMOSTAT_TIMER_COOLING_MAX_RUN_TIME) > 0)) {
      ESP_LOGCONFIG(TAG,
                    "    Maximum Run Time: %" PRIu32 "s\n"
                    "    Supplemental Delta: %.1f°C",
                    this->timer_duration_(thermostat::THERMOSTAT_TIMER_COOLING_MAX_RUN_TIME) / 1000,
                    this->supplemental_cool_delta_);
    }
  }
  if (this->supports_heat_) {
    ESP_LOGCONFIG(TAG,
                  "  Heating Parameters:\n"
                  "    Deadband: %.1f°C\n"
                  "    Overrun: %.1f°C\n"
                  "    Minimum Off Time: %" PRIu32 "s\n"
                  "    Minimum Run Time: %" PRIu32 "s",
                  this->heating_deadband_, this->heating_overrun_,
                  this->timer_duration_(thermostat::THERMOSTAT_TIMER_HEATING_OFF) / 1000,
                  this->timer_duration_(thermostat::THERMOSTAT_TIMER_HEATING_ON) / 1000);
    if ((this->supplemental_heat_delta_ > 0) ||
        (this->timer_duration_(thermostat::THERMOSTAT_TIMER_HEATING_MAX_RUN_TIME) > 0)) {
      ESP_LOGCONFIG(TAG,
                    "    Maximum Run Time: %" PRIu32 "s\n"
                    "    Supplemental Delta: %.1f°C",
                    this->timer_duration_(thermostat::THERMOSTAT_TIMER_HEATING_MAX_RUN_TIME) / 1000,
                    this->supplemental_heat_delta_);
    }
  }
  if (this->supports_fan_only_) {
    ESP_LOGCONFIG(TAG,
                  "  Fan Parameters:\n"
                  "    Minimum Off Time: %" PRIu32 "s\n"
                  "    Minimum Run Time: %" PRIu32 "s",
                  this->timer_duration_(thermostat::THERMOSTAT_TIMER_FANNING_OFF) / 1000,
                  this->timer_duration_(thermostat::THERMOSTAT_TIMER_FANNING_ON) / 1000);
  }
  if (this->supports_fan_mode_on_ || this->supports_fan_mode_off_ || this->supports_fan_mode_auto_ ||
      this->supports_fan_mode_low_ || this->supports_fan_mode_medium_ || this->supports_fan_mode_high_ ||
      this->supports_fan_mode_middle_ || this->supports_fan_mode_focus_ || this->supports_fan_mode_diffuse_ ||
      this->supports_fan_mode_quiet_) {
    ESP_LOGCONFIG(TAG, "  Minimum Fan Mode Switching Time: %" PRIu32 "s",
                  this->timer_duration_(thermostat::THERMOSTAT_TIMER_FAN_MODE) / 1000);
  }
  ESP_LOGCONFIG(TAG,
                "  Minimum Idle Time: %" PRIu32 "s\n"
                "  Supported MODES:\n"
                "    AUTO: %s\n"
                "    HEAT/COOL: %s\n"
                "    HEAT: %s\n"
                "    COOL: %s\n"
                "    DRY: %s\n"
                "    FAN_ONLY: %s\n"
                "    FAN_ONLY_ACTION_USES_FAN_MODE_TIMER: %s\n"
                "    FAN_ONLY_COOLING: %s",
                this->timer_[thermostat::THERMOSTAT_TIMER_IDLE_ON].time / 1000, YESNO(this->supports_auto_),
                YESNO(this->supports_heat_cool_), YESNO(this->supports_heat_), YESNO(this->supports_cool_),
                YESNO(this->supports_dry_), YESNO(this->supports_fan_only_),
                YESNO(this->supports_fan_only_action_uses_fan_mode_timer_), YESNO(this->supports_fan_only_cooling_));
  if (this->supports_cool_) {
    ESP_LOGCONFIG(TAG, "    FAN_WITH_COOLING: %s", YESNO(this->supports_fan_with_cooling_));
  }
  if (this->supports_heat_) {
    ESP_LOGCONFIG(TAG, "    FAN_WITH_HEATING: %s", YESNO(this->supports_fan_with_heating_));
  }
  ESP_LOGCONFIG(TAG,
                "  Supported FAN MODES:\n"
                "    ON: %s\n"
                "    OFF: %s\n"
                "    AUTO: %s\n"
                "    LOW: %s\n"
                "    MEDIUM: %s\n"
                "    HIGH: %s\n"
                "    MIDDLE: %s\n"
                "    FOCUS: %s\n"
                "    DIFFUSE: %s\n"
                "    QUIET: %s\n"
                "  Supported SWING MODES:\n"
                "    BOTH: %s\n"
                "    OFF: %s\n"
                "    HORIZONTAL: %s\n"
                "    VERTICAL: %s\n"
                "  Supports TWO SET POINTS: %s\n"
                "  Supported Humidity Parameters:\n"
                "    CURRENT: %s\n"
                "    TARGET: %s\n"
                "    DEHUMIDIFICATION: %s\n"
                "    HUMIDIFICATION: %s",
                YESNO(this->supports_fan_mode_on_), YESNO(this->supports_fan_mode_off_),
                YESNO(this->supports_fan_mode_auto_), YESNO(this->supports_fan_mode_low_),
                YESNO(this->supports_fan_mode_medium_), YESNO(this->supports_fan_mode_high_),
                YESNO(this->supports_fan_mode_middle_), YESNO(this->supports_fan_mode_focus_),
                YESNO(this->supports_fan_mode_diffuse_), YESNO(this->supports_fan_mode_quiet_),
                YESNO(this->supports_swing_mode_both_), YESNO(this->supports_swing_mode_off_),
                YESNO(this->supports_swing_mode_horizontal_), YESNO(this->supports_swing_mode_vertical_),
                YESNO(this->supports_two_points_),
                YESNO(this->get_traits().has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY)),
                YESNO(this->supports_dehumidification_ || this->supports_humidification_),
                YESNO(this->supports_dehumidification_), YESNO(this->supports_humidification_));

  if (!this->preset_config_.empty()) {
    ESP_LOGCONFIG(TAG, "  Supported PRESETS:");
    for (const auto &entry : this->preset_config_) {
      const auto *preset_name = LOG_STR_ARG(climate::climate_preset_to_string(entry.preset));
      ESP_LOGCONFIG(TAG, "    %s:%s", preset_name, entry.preset == this->default_preset_ ? " (default)" : "");
      this->dump_preset_config_(preset_name, entry.config);
    }
  }

  if (!this->custom_preset_config_.empty()) {
    ESP_LOGCONFIG(TAG, "  Supported CUSTOM PRESETS:");
    for (const auto &entry : this->custom_preset_config_) {
      const auto *preset_name = entry.name;
      ESP_LOGCONFIG(TAG, "    %s:%s", preset_name,
                    (this->default_custom_preset_ != nullptr && strcmp(entry.name, this->default_custom_preset_) == 0)
                        ? " (default)"
                        : "");
      this->dump_preset_config_(preset_name, entry.config);
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
