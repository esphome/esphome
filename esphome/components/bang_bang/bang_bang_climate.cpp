#include "bang_bang_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bang_bang {

static const char *TAG = "bang_bang.climate";

void BangBangClimate::setup() {
  this->sensor_->add_on_state_callback([this](float state) {
    this->current_temperature = state;
    // control may have changed, recompute
    this->compute_state_();
    this->switch_to_fan_mode_(this->fan_mode);
    // current temperature changed, publish state
    this->publish_state();
  });
  this->current_temperature = this->sensor_->state;
  // restore all climate data, if possible
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    // restore from defaults, change_away handles temps for us
    this->mode = climate::CLIMATE_MODE_AUTO;
    this->change_away_(false);
  }
  // refresh the climate action based on the restored settings
  this->switch_to_action_(compute_action_());
  this->setup_complete_ = true;
  this->publish_state();
}
void BangBangClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_fan_mode().has_value())
    this->fan_mode = *call.get_fan_mode();
  if (call.get_target_temperature_low().has_value())
    this->target_temperature_low = *call.get_target_temperature_low();
  if (call.get_target_temperature_high().has_value())
    this->target_temperature_high = *call.get_target_temperature_high();
  if (call.get_away().has_value())
    this->change_away_(*call.get_away());

  this->compute_state_();
  this->switch_to_fan_mode_(this->fan_mode);
  this->publish_state();
}
climate::ClimateTraits BangBangClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_auto_mode(this->supports_auto_);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_dry_mode(this->supports_dry_);
  traits.set_supports_fan_only_mode(this->supports_fan_only_);
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_fan_mode_on(this->supports_fan_mode_on_);
  traits.set_supports_fan_mode_off(this->supports_fan_mode_off_);
  traits.set_supports_fan_mode_auto(this->supports_fan_mode_auto_);
  traits.set_supports_fan_mode_low(this->supports_fan_mode_low_);
  traits.set_supports_fan_mode_medium(this->supports_fan_mode_medium_);
  traits.set_supports_fan_mode_high(this->supports_fan_mode_high_);
  traits.set_supports_fan_mode_middle(this->supports_fan_mode_middle_);
  traits.set_supports_fan_mode_focus(this->supports_fan_mode_focus_);
  traits.set_supports_fan_mode_diffuse(this->supports_fan_mode_diffuse_);
  traits.set_supports_two_point_target_temperature(true);
  traits.set_supports_away(this->supports_away_);
  traits.set_supports_action(true);
  return traits;
}
climate::ClimateAction BangBangClimate::compute_action_() {
  climate::ClimateAction target_action = this->action;
  if (this->supports_two_points_) {
    if (isnan(this->current_temperature) || isnan(this->target_temperature_low) ||
        isnan(this->target_temperature_high) || isnan(this->hysteresis_))
      // if any control parameters are nan, go to OFF action (not IDLE!)
      return climate::CLIMATE_ACTION_OFF;

    if (((this->action == climate::CLIMATE_ACTION_FAN) && (this->mode != climate::CLIMATE_MODE_FAN_ONLY)) ||
        ((this->action == climate::CLIMATE_ACTION_DRYING) && (this->mode != climate::CLIMATE_MODE_DRY))) {
      target_action = climate::CLIMATE_ACTION_IDLE;
    }

    switch (this->mode) {
      case climate::CLIMATE_MODE_FAN_ONLY:
        if (this->supports_fan_only_) {
          if (this->current_temperature > this->target_temperature_high + this->hysteresis_)
            target_action = climate::CLIMATE_ACTION_FAN;
          else if (this->current_temperature < this->target_temperature_high - this->hysteresis_)
            if (this->action == climate::CLIMATE_ACTION_FAN)
              target_action = climate::CLIMATE_ACTION_IDLE;
        }
        break;
      case climate::CLIMATE_MODE_DRY:
        target_action = climate::CLIMATE_ACTION_DRYING;
        break;
      case climate::CLIMATE_MODE_OFF:
        target_action = climate::CLIMATE_ACTION_OFF;
        break;
      case climate::CLIMATE_MODE_AUTO:
      case climate::CLIMATE_MODE_COOL:
      case climate::CLIMATE_MODE_HEAT:
        if (this->supports_cool_) {
          if (this->current_temperature > this->target_temperature_high + this->hysteresis_)
            target_action = climate::CLIMATE_ACTION_COOLING;
          else if (this->current_temperature < this->target_temperature_high - this->hysteresis_)
            if (this->action == climate::CLIMATE_ACTION_COOLING)
              target_action = climate::CLIMATE_ACTION_IDLE;
        }
        if (this->supports_heat_) {
          if (this->current_temperature < this->target_temperature_low - this->hysteresis_)
            target_action = climate::CLIMATE_ACTION_HEATING;
          else if (this->current_temperature > this->target_temperature_low + this->hysteresis_)
            if (this->action == climate::CLIMATE_ACTION_HEATING)
              target_action = climate::CLIMATE_ACTION_IDLE;
        }
        break;
      default:
        break;
    }
  } else {
    if (isnan(this->current_temperature) || isnan(this->target_temperature) || isnan(this->hysteresis_))
      // if any control parameters are nan, go to OFF action (not IDLE!)
      return climate::CLIMATE_ACTION_OFF;

    if (((this->action == climate::CLIMATE_ACTION_FAN) && (this->mode != climate::CLIMATE_MODE_FAN_ONLY)) ||
        ((this->action == climate::CLIMATE_ACTION_DRYING) && (this->mode != climate::CLIMATE_MODE_DRY))) {
      target_action = climate::CLIMATE_ACTION_IDLE;
    }

    switch (this->mode) {
      case climate::CLIMATE_MODE_FAN_ONLY:
        if (this->supports_fan_only_) {
          if (this->current_temperature > this->target_temperature + this->hysteresis_)
            target_action = climate::CLIMATE_ACTION_FAN;
          else if (this->current_temperature < this->target_temperature - this->hysteresis_)
            if (this->action == climate::CLIMATE_ACTION_FAN)
              target_action = climate::CLIMATE_ACTION_IDLE;
        }
        break;
      case climate::CLIMATE_MODE_DRY:
        target_action = climate::CLIMATE_ACTION_DRYING;
        break;
      case climate::CLIMATE_MODE_OFF:
        target_action = climate::CLIMATE_ACTION_OFF;
        break;
      case climate::CLIMATE_MODE_COOL:
        if (this->supports_cool_) {
          if (this->current_temperature > this->target_temperature + this->hysteresis_)
            target_action = climate::CLIMATE_ACTION_COOLING;
          else if (this->current_temperature < this->target_temperature - this->hysteresis_)
            if (this->action == climate::CLIMATE_ACTION_COOLING)
              target_action = climate::CLIMATE_ACTION_IDLE;
        }
      case climate::CLIMATE_MODE_HEAT:
        if (this->supports_heat_) {
          if (this->current_temperature < this->target_temperature - this->hysteresis_)
            target_action = climate::CLIMATE_ACTION_HEATING;
          else if (this->current_temperature > this->target_temperature + this->hysteresis_)
            if (this->action == climate::CLIMATE_ACTION_HEATING)
              target_action = climate::CLIMATE_ACTION_IDLE;
        }
        break;
      default:
        break;
    }
  }
  // do not switch to an action that isn't enabled per the active climate mode
  if ((this->mode == climate::CLIMATE_MODE_COOL) && (target_action == climate::CLIMATE_ACTION_HEATING))
    target_action = climate::CLIMATE_ACTION_IDLE;
  if ((this->mode == climate::CLIMATE_MODE_HEAT) && (target_action == climate::CLIMATE_ACTION_COOLING))
    target_action = climate::CLIMATE_ACTION_IDLE;

  return target_action;
}
void BangBangClimate::switch_to_action_(climate::ClimateAction action) {
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
    this->prev_action_trigger_->stop();
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
  this->publish_state();
}
void BangBangClimate::switch_to_fan_mode_(climate::ClimateFanMode fan_mode) {
  if (fan_mode == this->prev_fan_mode_)
    // already in target mode
    return;

  if (this->prev_fan_mode_trigger_ != nullptr) {
    this->prev_fan_mode_trigger_->stop();
    this->prev_fan_mode_trigger_ = nullptr;
  }
  Trigger<> *trig;
  switch (fan_mode) {
    case climate::CLIMATE_FAN_ON:
      trig = this->fan_mode_on_trigger_;
      break;
    case climate::CLIMATE_FAN_OFF:
      trig = this->fan_mode_off_trigger_;
      break;
    case climate::CLIMATE_FAN_AUTO:
      trig = this->fan_mode_auto_trigger_;
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
      trig = nullptr;
  }
  assert(trig != nullptr);
  trig->trigger();
  // this->fan_mode = fan_mode;   // assigned earlier/above
  this->prev_fan_mode_ = fan_mode;
  this->prev_fan_mode_trigger_ = trig;
  this->publish_state();
}
void BangBangClimate::change_away_(bool away) {
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
  this->away = away;
}
void BangBangClimate::set_normal_config(const BangBangClimateTargetTempConfig &normal_config) {
  this->normal_config_ = normal_config;
}
void BangBangClimate::set_away_config(const BangBangClimateTargetTempConfig &away_config) {
  this->supports_away_ = true;
  this->away_config_ = away_config;
}
BangBangClimate::BangBangClimate()
    : idle_trigger_(new Trigger<>()),
      cool_trigger_(new Trigger<>()),
      heat_trigger_(new Trigger<>()),
      fan_mode_on_trigger_(new Trigger<>()),
      fan_mode_off_trigger_(new Trigger<>()),
      fan_mode_auto_trigger_(new Trigger<>()),
      fan_mode_low_trigger_(new Trigger<>()),
      fan_mode_medium_trigger_(new Trigger<>()),
      fan_mode_high_trigger_(new Trigger<>()),
      fan_mode_middle_trigger_(new Trigger<>()),
      fan_mode_focus_trigger_(new Trigger<>()),
      fan_mode_diffuse_trigger_(new Trigger<>()) {}
void BangBangClimate::set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
void BangBangClimate::set_supports_cool(bool supports_cool) { this->supports_cool_ = supports_cool; }
void BangBangClimate::set_supports_heat(bool supports_heat) { this->supports_heat_ = supports_heat; }
void BangBangClimate::set_supports_fan_mode_on(bool supports_fan_mode_on) { this->supports_fan_mode_on_ = supports_fan_mode_on; }
void BangBangClimate::set_supports_fan_mode_off(bool supports_fan_mode_off) { this->supports_fan_mode_off_ = supports_fan_mode_off; }
void BangBangClimate::set_supports_fan_mode_auto(bool supports_fan_mode_auto) { this->supports_fan_mode_auto_ = supports_fan_mode_auto; }
void BangBangClimate::set_supports_fan_mode_low(bool supports_fan_mode_low) { this->supports_fan_mode_low_ = supports_fan_mode_low; }
void BangBangClimate::set_supports_fan_mode_medium(bool supports_fan_mode_medium) { this->supports_fan_mode_medium_ = supports_fan_mode_medium; }
void BangBangClimate::set_supports_fan_mode_high(bool supports_fan_mode_high) { this->supports_fan_mode_high_ = supports_fan_mode_high; }
void BangBangClimate::set_supports_fan_mode_middle(bool supports_fan_mode_middle) { this->supports_fan_mode_middle_ = supports_fan_mode_middle; }
void BangBangClimate::set_supports_fan_mode_focus(bool supports_fan_mode_focus) { this->supports_fan_mode_focus_ = supports_fan_mode_focus; }
void BangBangClimate::set_supports_fan_mode_diffuse(bool supports_fan_mode_diffuse) { this->supports_fan_mode_diffuse_ = supports_fan_mode_diffuse; }
Trigger<> *BangBangClimate::get_idle_trigger() const { return this->idle_trigger_; }
Trigger<> *BangBangClimate::get_cool_trigger() const { return this->cool_trigger_; }
Trigger<> *BangBangClimate::get_heat_trigger() const { return this->heat_trigger_; }
Trigger<> *BangBangClimate::get_fan_mode_on_trigger() const { return this->fan_mode_on_trigger_; }
Trigger<> *BangBangClimate::get_fan_mode_off_trigger() const { return this->fan_mode_off_trigger_; }
Trigger<> *BangBangClimate::get_fan_mode_auto_trigger() const { return this->fan_mode_auto_trigger_; }
Trigger<> *BangBangClimate::get_fan_mode_low_trigger() const { return this->fan_mode_low_trigger_; }
Trigger<> *BangBangClimate::get_fan_mode_medium_trigger() const { return this->fan_mode_medium_trigger_; }
Trigger<> *BangBangClimate::get_fan_mode_high_trigger() const { return this->fan_mode_high_trigger_; }
Trigger<> *BangBangClimate::get_fan_mode_middle_trigger() const { return this->fan_mode_middle_trigger_; }
Trigger<> *BangBangClimate::get_fan_mode_focus_trigger() const { return this->fan_mode_focus_trigger_; }
Trigger<> *BangBangClimate::get_fan_mode_diffuse_trigger() const { return this->fan_mode_diffuse_trigger_; }
void BangBangClimate::dump_config() {
  LOG_CLIMATE("", "Bang Bang Climate", this);
  ESP_LOGCONFIG(TAG, "  Supports COOL: %s", YESNO(this->supports_cool_));
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
  ESP_LOGCONFIG(TAG, "  Supports AWAY mode: %s", YESNO(this->supports_away_));
  if (this->supports_away_) {
    if (this->supports_heat_)
      ESP_LOGCONFIG(TAG, "    Away Default Target Temperature Low: %.1f°C", this->away_config_.default_temperature_low);
    if ((this->supports_cool_) || (this->supports_fan_only_))
      ESP_LOGCONFIG(TAG, "    Away Default Target Temperature High: %.1f°C",
                    this->away_config_.default_temperature_high);
  }
}

BangBangClimateTargetTempConfig::BangBangClimateTargetTempConfig() = default;
BangBangClimateTargetTempConfig::BangBangClimateTargetTempConfig(float default_temperature)
    : default_temperature(default_temperature) {}
BangBangClimateTargetTempConfig::BangBangClimateTargetTempConfig(float default_temperature_low,
                                                                 float default_temperature_high)
    : default_temperature_low(default_temperature_low), default_temperature_high(default_temperature_high) {}

}  // namespace bang_bang
}  // namespace esphome
