#include "hygrostat_humidifier.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hygrostat {

static const char *const TAG = "hygrostat.humidifier";

void HygrostatHumidifier::setup() {
  if (this->use_startup_delay_) {
    // start timers so that no actions are called for a moment
    this->start_timer_(hygrostat::TIMER_DEHUMIDIFYING_OFF);
    this->start_timer_(hygrostat::TIMER_HUMIDIFYING_OFF);
  }
  // add a callback so that whenever the sensor state changes we can take action
  this->sensor_->add_on_state_callback([this](float state) {
    this->current_humidity = state;
    // required action may have changed, recompute, refresh, we'll publish_state() later
    this->switch_to_action_(this->compute_action_(), false);
    // current humidity and possibly action changed, so publish the new state
    this->publish_state();
  });
  this->current_humidity = this->sensor_->state;
  // restore all humidifier data, if possible
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    // restore from defaults, change_away handles humidities for us
    this->mode = this->default_mode_;
    this->change_away_(false);
  }
  // refresh the humidifier action based on the restored settings, we'll publish_state() later
  this->switch_to_action_(this->compute_action_(), false);
  this->setup_complete_ = true;
  this->publish_state();
}

float HygrostatHumidifier::dehumidify_deadband() { return this->dehumidifying_deadband_; }
float HygrostatHumidifier::dehumidify_overrun() { return this->dehumidifying_overrun_; }
float HygrostatHumidifier::humidify_deadband() { return this->humidifying_deadband_; }
float HygrostatHumidifier::humidify_overrun() { return this->humidifying_overrun_; }

void HygrostatHumidifier::refresh() {
  this->switch_to_mode_(this->mode, false);
  this->switch_to_action_(this->compute_action_(), false);
  this->check_humidity_change_trigger_();
  this->publish_state();
}

bool HygrostatHumidifier::humidifier_action_change_delayed() {
  bool state_mismatch = this->action != this->compute_action_(true);

  switch (this->compute_action_(true)) {
    case humidifier::HUMIDIFIER_ACTION_OFF:
    case humidifier::HUMIDIFIER_ACTION_IDLE:
      return state_mismatch && (!this->idle_action_ready_());
    case humidifier::HUMIDIFIER_ACTION_DEHUMIDIFYING:
      return state_mismatch && (!this->dehumidifying_action_ready_());
    case humidifier::HUMIDIFIER_ACTION_HUMIDIFYING:
      return state_mismatch && (!this->humidifying_action_ready_());
    default:
      break;
  }
  return false;
}

humidifier::HumidifierAction HygrostatHumidifier::delayed_humidifier_action() { return this->compute_action_(true); }

bool HygrostatHumidifier::hysteresis_valid() {
  if (this->supports_dehumidify_ &&
      (std::isnan(this->dehumidifying_deadband_) || std::isnan(this->dehumidifying_overrun_)))
    return false;

  if (this->supports_humidify_ && (std::isnan(this->humidifying_deadband_) || std::isnan(this->humidifying_overrun_)))
    return false;

  return true;
}

void HygrostatHumidifier::validate_target_humidity() {
  if (std::isnan(this->target_humidity)) {
    this->target_humidity =
        ((this->get_traits().get_visual_max_humidity() - this->get_traits().get_visual_min_humidity()) / 2) +
        this->get_traits().get_visual_min_humidity();
  } else {
    // target_humidity must be between the visual minimum and the visual maximum
    if (this->target_humidity < this->get_traits().get_visual_min_humidity())
      this->target_humidity = this->get_traits().get_visual_min_humidity();
    if (this->target_humidity > this->get_traits().get_visual_max_humidity())
      this->target_humidity = this->get_traits().get_visual_max_humidity();
  }
}

void HygrostatHumidifier::validate_target_humiditys() {
  if (this->supports_two_points_) {
    this->validate_target_humidity_low();
    this->validate_target_humidity_high();
  } else {
    this->validate_target_humidity();
  }
}

void HygrostatHumidifier::validate_target_humidity_low() {
  if (std::isnan(this->target_humidity_low)) {
    this->target_humidity_low = this->get_traits().get_visual_min_humidity();
  } else {
    // target_humidity_low must not be lower than the visual minimum
    if (this->target_humidity_low < this->get_traits().get_visual_min_humidity())
      this->target_humidity_low = this->get_traits().get_visual_min_humidity();
    // target_humidity_low must not be greater than the visual maximum minus set_point_minimum_differential_
    if (this->target_humidity_low >
        this->get_traits().get_visual_max_humidity() - this->set_point_minimum_differential_) {
      this->target_humidity_low = this->get_traits().get_visual_max_humidity() - this->set_point_minimum_differential_;
    }
    // if target_humidity_low is set greater than target_humidity_high, move up target_humidity_high
    if (this->target_humidity_low > this->target_humidity_high - this->set_point_minimum_differential_)
      this->target_humidity_high = this->target_humidity_low + this->set_point_minimum_differential_;
  }
}

void HygrostatHumidifier::validate_target_humidity_high() {
  if (std::isnan(this->target_humidity_high)) {
    this->target_humidity_high = this->get_traits().get_visual_max_humidity();
  } else {
    // target_humidity_high must not be lower than the visual maximum
    if (this->target_humidity_high > this->get_traits().get_visual_max_humidity())
      this->target_humidity_high = this->get_traits().get_visual_max_humidity();
    // target_humidity_high must not be lower than the visual minimum plus set_point_minimum_differential_
    if (this->target_humidity_high <
        this->get_traits().get_visual_min_humidity() + this->set_point_minimum_differential_) {
      this->target_humidity_high = this->get_traits().get_visual_min_humidity() + this->set_point_minimum_differential_;
    }
    // if target_humidity_high is set less than target_humidity_low, move down target_humidity_low
    if (this->target_humidity_high < this->target_humidity_low + this->set_point_minimum_differential_)
      this->target_humidity_low = this->target_humidity_high - this->set_point_minimum_differential_;
  }
}

void HygrostatHumidifier::control(const humidifier::HumidifierCall &call) {
  if (call.get_preset().has_value()) {
    // setup_complete_ blocks modifying/resetting the humidities immediately after boot
    if (this->setup_complete_) {
      this->change_away_(*call.get_preset() == humidifier::HUMIDIFIER_PRESET_AWAY);
    } else {
      this->preset = *call.get_preset();
    }
  }
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (this->supports_two_points_) {
    if (call.get_target_humidity_low().has_value()) {
      this->target_humidity_low = *call.get_target_humidity_low();
      validate_target_humidity_low();
    }
    if (call.get_target_humidity_high().has_value()) {
      this->target_humidity_high = *call.get_target_humidity_high();
      validate_target_humidity_high();
    }
  } else {
    if (call.get_target_humidity().has_value()) {
      this->target_humidity = *call.get_target_humidity();
      validate_target_humidity();
    }
  }
  // make any changes happen
  refresh();
}

humidifier::HumidifierTraits HygrostatHumidifier::traits() {
  auto traits = humidifier::HumidifierTraits();
  traits.set_supports_current_humidity(true);
  if (supports_auto_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_AUTO);
  if (supports_humidify_dehumidify_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY);
  if (supports_dehumidify_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_DEHUMIDIFY);
  if (supports_humidify_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_HUMIDIFY);

  if (supports_away_) {
    traits.set_supported_presets({humidifier::HUMIDIFIER_PRESET_HOME, humidifier::HUMIDIFIER_PRESET_AWAY});
  } else {
    traits.set_supported_presets({humidifier::HUMIDIFIER_PRESET_HOME});
  }

  traits.set_supports_two_point_target_humidity(this->supports_two_points_);
  traits.set_supports_action(true);
  return traits;
}

humidifier::HumidifierAction HygrostatHumidifier::compute_action_(const bool ignore_timers) {
  auto target_action = humidifier::HUMIDIFIER_ACTION_IDLE;
  // if any hysteresis values or current_humidity is not valid, we go to OFF;
  if (std::isnan(this->current_humidity) || !this->hysteresis_valid()) {
    return humidifier::HUMIDIFIER_ACTION_OFF;
  }
  // do not change the action if an "ON" timer is running
  if ((!ignore_timers) &&
      (timer_active_(hygrostat::TIMER_IDLE_ON) || timer_active_(hygrostat::TIMER_DEHUMIDIFYING_ON) ||
       timer_active_(hygrostat::TIMER_HUMIDIFYING_ON))) {
    return this->action;
  }

  // ensure set point(s) is/are valid before computing the action
  this->validate_target_humiditys();
  // everything has been validated so we can now safely compute the action
  switch (this->mode) {
    // if the humidifier mode is OFF then the humidifier action must be OFF
    case humidifier::HUMIDIFIER_MODE_OFF:
      target_action = humidifier::HUMIDIFIER_ACTION_OFF;
      break;
    case humidifier::HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY:
      if (this->dehumidifying_required_() && this->humidifying_required_()) {
        // this is bad and should never happen, so just stop.
        // target_action = humidifier::HUMIDIFIER_ACTION_IDLE;
      } else if (this->dehumidifying_required_()) {
        target_action = humidifier::HUMIDIFIER_ACTION_DEHUMIDIFYING;
      } else if (this->humidifying_required_()) {
        target_action = humidifier::HUMIDIFIER_ACTION_HUMIDIFYING;
      }
      break;
    case humidifier::HUMIDIFIER_MODE_DEHUMIDIFY:
      if (this->dehumidifying_required_()) {
        target_action = humidifier::HUMIDIFIER_ACTION_DEHUMIDIFYING;
      }
      break;
    case humidifier::HUMIDIFIER_MODE_HUMIDIFY:
      if (this->humidifying_required_()) {
        target_action = humidifier::HUMIDIFIER_ACTION_HUMIDIFYING;
      }
      break;
    default:
      break;
  }
  // do not abruptly switch actions. cycle through IDLE, first. we'll catch this at the next update.
  if ((((this->action == humidifier::HUMIDIFIER_ACTION_DEHUMIDIFYING)) &&
       (target_action == humidifier::HUMIDIFIER_ACTION_HUMIDIFYING)) ||
      ((this->action == humidifier::HUMIDIFIER_ACTION_HUMIDIFYING) &&
       ((target_action == humidifier::HUMIDIFIER_ACTION_DEHUMIDIFYING)))) {
    return humidifier::HUMIDIFIER_ACTION_IDLE;
  }

  return target_action;
}

void HygrostatHumidifier::switch_to_action_(humidifier::HumidifierAction action, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((action == this->action) && this->setup_complete_) {
    // already in target mode
    return;
  }

  if (((action == humidifier::HUMIDIFIER_ACTION_OFF && this->action == humidifier::HUMIDIFIER_ACTION_IDLE) ||
       (action == humidifier::HUMIDIFIER_ACTION_IDLE && this->action == humidifier::HUMIDIFIER_ACTION_OFF)) &&
      this->setup_complete_) {
    // switching from OFF to IDLE or vice-versa -- this is only a visual difference.
    // OFF means user manually disabled, IDLE means the humidity is in target range.
    this->action = action;
    if (publish_state)
      this->publish_state();
    return;
  }

  bool action_ready = false;
  Trigger<> *trig = this->idle_action_trigger_;
  switch (action) {
    case humidifier::HUMIDIFIER_ACTION_OFF:
    case humidifier::HUMIDIFIER_ACTION_IDLE:
      if (this->idle_action_ready_()) {
        this->start_timer_(hygrostat::TIMER_IDLE_ON);
        if (this->action == humidifier::HUMIDIFIER_ACTION_DEHUMIDIFYING)
          this->start_timer_(hygrostat::TIMER_DEHUMIDIFYING_OFF);
        if (this->action == humidifier::HUMIDIFIER_ACTION_HUMIDIFYING)
          this->start_timer_(hygrostat::TIMER_HUMIDIFYING_OFF);
        // trig = this->idle_action_trigger_;
        ESP_LOGVV(TAG, "Switching to IDLE/OFF action");
        this->dehumidifying_max_runtime_exceeded_ = false;
        this->humidifying_max_runtime_exceeded_ = false;
        action_ready = true;
      }
      break;
    case humidifier::HUMIDIFIER_ACTION_DEHUMIDIFYING:
      if (this->dehumidifying_action_ready_()) {
        this->start_timer_(hygrostat::TIMER_DEHUMIDIFYING_ON);
        this->start_timer_(hygrostat::TIMER_DEHUMIDIFYING_MAX_RUN_TIME);
        trig = this->dehumidify_action_trigger_;
        ESP_LOGVV(TAG, "Switching to DEHUMIDIFYING action");
        action_ready = true;
      }
      break;
    case humidifier::HUMIDIFIER_ACTION_HUMIDIFYING:
      if (this->humidifying_action_ready_()) {
        this->start_timer_(hygrostat::TIMER_HUMIDIFYING_ON);
        this->start_timer_(hygrostat::TIMER_HUMIDIFYING_MAX_RUN_TIME);
        trig = this->humidify_action_trigger_;
        ESP_LOGVV(TAG, "Switching to HUMIDIFYING action");
        action_ready = true;
      }
      break;
    default:
      // we cannot report an invalid mode back to HA (even if it asked for one)
      //  and must assume some valid value
      action = humidifier::HUMIDIFIER_ACTION_OFF;
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
    if (publish_state)
      this->publish_state();
  }
}

void HygrostatHumidifier::switch_to_mode_(humidifier::HumidifierMode mode, bool publish_state) {
  // setup_complete_ helps us ensure an action is called immediately after boot
  if ((mode == this->prev_mode_) && this->setup_complete_) {
    // already in target mode
    return;
  }

  if (this->prev_mode_trigger_ != nullptr) {
    this->prev_mode_trigger_->stop_action();
    this->prev_mode_trigger_ = nullptr;
  }
  Trigger<> *trig = this->auto_mode_trigger_;
  switch (mode) {
    case humidifier::HUMIDIFIER_MODE_OFF:
      trig = this->off_mode_trigger_;
      break;
    case humidifier::HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY:
      // trig = this->auto_mode_trigger_;
      break;
    case humidifier::HUMIDIFIER_MODE_DEHUMIDIFY:
      trig = this->dehumidify_mode_trigger_;
      break;
    case humidifier::HUMIDIFIER_MODE_HUMIDIFY:
      trig = this->humidify_mode_trigger_;
      break;
    default:
      // we cannot report an invalid mode back to HA (even if it asked for one)
      //  and must assume some valid value
      mode = humidifier::HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY;
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

bool HygrostatHumidifier::idle_action_ready_() {
  return !(this->timer_active_(hygrostat::TIMER_DEHUMIDIFYING_ON) ||
           this->timer_active_(hygrostat::TIMER_HUMIDIFYING_ON));
}

bool HygrostatHumidifier::dehumidifying_action_ready_() {
  return !(this->timer_active_(hygrostat::TIMER_IDLE_ON) || this->timer_active_(hygrostat::TIMER_DEHUMIDIFYING_OFF) ||
           this->timer_active_(hygrostat::TIMER_HUMIDIFYING_ON));
}

bool HygrostatHumidifier::humidifying_action_ready_() {
  return !(this->timer_active_(hygrostat::TIMER_IDLE_ON) || this->timer_active_(hygrostat::TIMER_DEHUMIDIFYING_ON) ||
           this->timer_active_(hygrostat::TIMER_HUMIDIFYING_OFF));
}

void HygrostatHumidifier::start_timer_(const HygrostatHumidifierTimerIndex timer_index) {
  if (this->timer_duration_(timer_index) > 0) {
    this->set_timeout(this->timer_[timer_index].name, this->timer_duration_(timer_index),
                      this->timer_cbf_(timer_index));
    this->timer_[timer_index].active = true;
  }
}

bool HygrostatHumidifier::cancel_timer_(HygrostatHumidifierTimerIndex timer_index) {
  this->timer_[timer_index].active = false;
  return this->cancel_timeout(this->timer_[timer_index].name);
}

bool HygrostatHumidifier::timer_active_(HygrostatHumidifierTimerIndex timer_index) {
  return this->timer_[timer_index].active;
}

uint32_t HygrostatHumidifier::timer_duration_(HygrostatHumidifierTimerIndex timer_index) {
  return this->timer_[timer_index].time;
}

std::function<void()> HygrostatHumidifier::timer_cbf_(HygrostatHumidifierTimerIndex timer_index) {
  return this->timer_[timer_index].func;
}

void HygrostatHumidifier::dehumidifying_max_run_time_timer_callback_() {
  ESP_LOGVV(TAG, "dehumidifying_max_run_time timer expired");
  this->timer_[hygrostat::TIMER_DEHUMIDIFYING_MAX_RUN_TIME].active = false;
  this->dehumidifying_max_runtime_exceeded_ = true;
}

void HygrostatHumidifier::dehumidifying_off_timer_callback_() {
  ESP_LOGVV(TAG, "dehumidifying_off timer expired");
  this->timer_[hygrostat::TIMER_DEHUMIDIFYING_OFF].active = false;
  this->switch_to_action_(this->compute_action_());
}

void HygrostatHumidifier::dehumidifying_on_timer_callback_() {
  ESP_LOGVV(TAG, "dehumidifying_on timer expired");
  this->timer_[hygrostat::TIMER_DEHUMIDIFYING_ON].active = false;
  this->switch_to_action_(this->compute_action_());
}

void HygrostatHumidifier::humidifying_max_run_time_timer_callback_() {
  ESP_LOGVV(TAG, "humidifying_max_run_time timer expired");
  this->timer_[hygrostat::TIMER_HUMIDIFYING_MAX_RUN_TIME].active = false;
  this->humidifying_max_runtime_exceeded_ = true;
}

void HygrostatHumidifier::humidifying_off_timer_callback_() {
  ESP_LOGVV(TAG, "humidifying_off timer expired");
  this->timer_[hygrostat::TIMER_HUMIDIFYING_OFF].active = false;
  this->switch_to_action_(this->compute_action_());
}

void HygrostatHumidifier::humidifying_on_timer_callback_() {
  ESP_LOGVV(TAG, "humidifying_on timer expired");
  this->timer_[hygrostat::TIMER_HUMIDIFYING_ON].active = false;
  this->switch_to_action_(this->compute_action_());
}

void HygrostatHumidifier::idle_on_timer_callback_() {
  ESP_LOGVV(TAG, "idle_on timer expired");
  this->timer_[hygrostat::TIMER_IDLE_ON].active = false;
  this->switch_to_action_(this->compute_action_());
}

void HygrostatHumidifier::check_humidity_change_trigger_() {
  if (this->supports_two_points_) {
    // setup_complete_ helps us ensure an action is called immediately after boot
    if ((this->prev_target_humidity_low_ == this->target_humidity_low) &&
        (this->prev_target_humidity_high_ == this->target_humidity_high) && this->setup_complete_) {
      return;  // nothing changed, no reason to trigger
    } else {
      // save the new humiditys so we can check them again later; the trigger will fire below
      this->prev_target_humidity_low_ = this->target_humidity_low;
      this->prev_target_humidity_high_ = this->target_humidity_high;
    }
  } else {
    if ((this->prev_target_humidity_ == this->target_humidity) && this->setup_complete_) {
      return;  // nothing changed, no reason to trigger
    } else {
      // save the new humidity so we can check it again later; the trigger will fire below
      this->prev_target_humidity_ = this->target_humidity;
    }
  }
  // trigger the action
  Trigger<> *trig = this->humidity_change_trigger_;
  assert(trig != nullptr);
  trig->trigger();
}

bool HygrostatHumidifier::dehumidifying_required_() {
  auto humidity = this->supports_two_points_ ? this->target_humidity_high : this->target_humidity;

  if (this->supports_dehumidify_) {
    if (this->current_humidity > humidity + this->dehumidifying_deadband_) {
      // if the current humidity exceeds the target + deadband, dehumidifying is required
      return true;
    } else if (this->current_humidity < humidity - this->dehumidifying_overrun_) {
      // if the current humidity is less than the target - overrun, dehumidifying should stop
      return false;
    } else {
      // if we get here, the current humidity is between target + deadband and target - overrun,
      //  so the action should not change unless it conflicts with the current mode
      return (this->action == humidifier::HUMIDIFIER_ACTION_DEHUMIDIFYING) &&
             ((this->mode == humidifier::HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY) ||
              (this->mode == humidifier::HUMIDIFIER_MODE_DEHUMIDIFY));
    }
  }
  return false;
}

bool HygrostatHumidifier::humidifying_required_() {
  auto humidity = this->supports_two_points_ ? this->target_humidity_low : this->target_humidity;

  if (this->supports_humidify_) {
    if (this->current_humidity < humidity - this->humidifying_deadband_) {
      // if the current humidity is below the target - deadband, humidifying is required
      return true;
    } else if (this->current_humidity > humidity + this->humidifying_overrun_) {
      // if the current humidity is above the target + overrun, humidifying should stop
      return false;
    } else {
      // if we get here, the current humidity is between target - deadband and target + overrun,
      //  so the action should not change unless it conflicts with the current mode
      return (this->action == humidifier::HUMIDIFIER_ACTION_HUMIDIFYING) &&
             ((this->mode == humidifier::HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY) ||
              (this->mode == humidifier::HUMIDIFIER_MODE_HUMIDIFY));
    }
  }
  return false;
}

void HygrostatHumidifier::change_away_(bool away) {
  if (!away) {
    if (this->supports_two_points_) {
      this->target_humidity_low = this->normal_config_.default_humidity_low;
      this->target_humidity_high = this->normal_config_.default_humidity_high;
    } else
      this->target_humidity = this->normal_config_.default_humidity;
  } else {
    if (this->supports_two_points_) {
      this->target_humidity_low = this->away_config_.default_humidity_low;
      this->target_humidity_high = this->away_config_.default_humidity_high;
    } else
      this->target_humidity = this->away_config_.default_humidity;
  }
  this->preset = away ? humidifier::HUMIDIFIER_PRESET_AWAY : humidifier::HUMIDIFIER_PRESET_HOME;
}

void HygrostatHumidifier::set_normal_config(const HygrostatHumidifierTargetTempConfig &normal_config) {
  this->normal_config_ = normal_config;
}

void HygrostatHumidifier::set_away_config(const HygrostatHumidifierTargetTempConfig &away_config) {
  this->supports_away_ = true;
  this->away_config_ = away_config;
}

HygrostatHumidifier::HygrostatHumidifier()
    : dehumidify_action_trigger_(new Trigger<>()),
      dehumidify_mode_trigger_(new Trigger<>()),
      humidify_action_trigger_(new Trigger<>()),
      humidify_mode_trigger_(new Trigger<>()),
      auto_mode_trigger_(new Trigger<>()),
      idle_action_trigger_(new Trigger<>()),
      off_mode_trigger_(new Trigger<>()),
      humidity_change_trigger_(new Trigger<>()) {}

void HygrostatHumidifier::set_default_mode(humidifier::HumidifierMode default_mode) {
  this->default_mode_ = default_mode;
}
void HygrostatHumidifier::set_set_point_minimum_differential(float differential) {
  this->set_point_minimum_differential_ = differential;
}
void HygrostatHumidifier::set_dehumidify_deadband(float deadband) { this->dehumidifying_deadband_ = deadband; }
void HygrostatHumidifier::set_dehumidify_overrun(float overrun) { this->dehumidifying_overrun_ = overrun; }
void HygrostatHumidifier::set_humidify_deadband(float deadband) { this->humidifying_deadband_ = deadband; }
void HygrostatHumidifier::set_humidify_overrun(float overrun) { this->humidifying_overrun_ = overrun; }
void HygrostatHumidifier::set_dehumidifying_maximum_run_time_in_sec(uint32_t time) {
  this->timer_[hygrostat::TIMER_DEHUMIDIFYING_MAX_RUN_TIME].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void HygrostatHumidifier::set_dehumidifying_minimum_off_time_in_sec(uint32_t time) {
  this->timer_[hygrostat::TIMER_DEHUMIDIFYING_OFF].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void HygrostatHumidifier::set_dehumidifying_minimum_run_time_in_sec(uint32_t time) {
  this->timer_[hygrostat::TIMER_DEHUMIDIFYING_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void HygrostatHumidifier::set_humidifying_maximum_run_time_in_sec(uint32_t time) {
  this->timer_[hygrostat::TIMER_HUMIDIFYING_MAX_RUN_TIME].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void HygrostatHumidifier::set_humidifying_minimum_off_time_in_sec(uint32_t time) {
  this->timer_[hygrostat::TIMER_HUMIDIFYING_OFF].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void HygrostatHumidifier::set_humidifying_minimum_run_time_in_sec(uint32_t time) {
  this->timer_[hygrostat::TIMER_HUMIDIFYING_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void HygrostatHumidifier::set_idle_minimum_time_in_sec(uint32_t time) {
  this->timer_[hygrostat::TIMER_IDLE_ON].time =
      1000 * (time < this->min_timer_duration_ ? this->min_timer_duration_ : time);
}
void HygrostatHumidifier::set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
void HygrostatHumidifier::set_use_startup_delay(bool use_startup_delay) {
  this->use_startup_delay_ = use_startup_delay;
}
void HygrostatHumidifier::set_supports_humidify_dehumidify(bool supports_humidify_dehumidify) {
  this->supports_humidify_dehumidify_ = supports_humidify_dehumidify;
}
void HygrostatHumidifier::set_supports_auto(bool supports_auto) { this->supports_auto_ = supports_auto; }
void HygrostatHumidifier::set_supports_dehumidify(bool supports_dehumidify) {
  this->supports_dehumidify_ = supports_dehumidify;
}
void HygrostatHumidifier::set_supports_humidify(bool supports_humidify) {
  this->supports_humidify_ = supports_humidify;
}
void HygrostatHumidifier::set_supports_two_points(bool supports_two_points) {
  this->supports_two_points_ = supports_two_points;
}

Trigger<> *HygrostatHumidifier::get_dehumidify_action_trigger() const { return this->dehumidify_action_trigger_; }
Trigger<> *HygrostatHumidifier::get_humidify_action_trigger() const { return this->humidify_action_trigger_; }
Trigger<> *HygrostatHumidifier::get_idle_action_trigger() const { return this->idle_action_trigger_; }
Trigger<> *HygrostatHumidifier::get_auto_mode_trigger() const { return this->auto_mode_trigger_; }
Trigger<> *HygrostatHumidifier::get_dehumidify_mode_trigger() const { return this->dehumidify_mode_trigger_; }
Trigger<> *HygrostatHumidifier::get_humidify_mode_trigger() const { return this->humidify_mode_trigger_; }
Trigger<> *HygrostatHumidifier::get_off_mode_trigger() const { return this->off_mode_trigger_; }
Trigger<> *HygrostatHumidifier::get_humidity_change_trigger() const { return this->humidity_change_trigger_; }

void HygrostatHumidifier::dump_config() {
  LOG_HUMIDIFIER("", "Hygrostat", this);
  if (this->supports_humidify_) {
    if (this->supports_two_points_) {
      ESP_LOGCONFIG(TAG, "  Default Target Humidity Low: %.1f%%", this->normal_config_.default_humidity_low);
    } else {
      ESP_LOGCONFIG(TAG, "  Default Target Humidity Low: %.1f%%", this->normal_config_.default_humidity);
    }
  }
  if (this->supports_dehumidify_) {
    if (this->supports_two_points_) {
      ESP_LOGCONFIG(TAG, "  Default Target Humidity High: %.1f%%", this->normal_config_.default_humidity_high);
    } else {
      ESP_LOGCONFIG(TAG, "  Default Target Humidity High: %.1f%%", this->normal_config_.default_humidity);
    }
  }
  if (this->supports_two_points_)
    ESP_LOGCONFIG(TAG, "  Minimum Set Point Differential: %.1f%%", this->set_point_minimum_differential_);
  ESP_LOGCONFIG(TAG, "  Start-up Delay Enabled: %s", YESNO(this->use_startup_delay_));
  if (this->supports_dehumidify_) {
    ESP_LOGCONFIG(TAG, "  Dehumidifying Parameters:");
    ESP_LOGCONFIG(TAG, "    Deadband: %.1f%%", this->dehumidifying_deadband_);
    ESP_LOGCONFIG(TAG, "    Overrun: %.1f%%", this->dehumidifying_overrun_);
    if (this->timer_duration_(hygrostat::TIMER_DEHUMIDIFYING_MAX_RUN_TIME) > 0) {
      ESP_LOGCONFIG(TAG, "    Maximum Run Time: %us",
                    this->timer_duration_(hygrostat::TIMER_DEHUMIDIFYING_MAX_RUN_TIME) / 1000);
    }
    ESP_LOGCONFIG(TAG, "    Minimum Off Time: %us", this->timer_duration_(hygrostat::TIMER_DEHUMIDIFYING_OFF) / 1000);
    ESP_LOGCONFIG(TAG, "    Minimum Run Time: %us", this->timer_duration_(hygrostat::TIMER_DEHUMIDIFYING_ON) / 1000);
  }
  if (this->supports_humidify_) {
    ESP_LOGCONFIG(TAG, "  Humidifying Parameters:");
    ESP_LOGCONFIG(TAG, "    Deadband: %.1f%%", this->humidifying_deadband_);
    ESP_LOGCONFIG(TAG, "    Overrun: %.1f%%", this->humidifying_overrun_);
    if (this->timer_duration_(hygrostat::TIMER_HUMIDIFYING_MAX_RUN_TIME) > 0) {
      ESP_LOGCONFIG(TAG, "    Maximum Run Time: %us",
                    this->timer_duration_(hygrostat::TIMER_HUMIDIFYING_MAX_RUN_TIME) / 1000);
    }
    ESP_LOGCONFIG(TAG, "    Minimum Off Time: %us", this->timer_duration_(hygrostat::TIMER_HUMIDIFYING_OFF) / 1000);
    ESP_LOGCONFIG(TAG, "    Minimum Run Time: %us", this->timer_duration_(hygrostat::TIMER_HUMIDIFYING_ON) / 1000);
  }
  ESP_LOGCONFIG(TAG, "  Minimum Idle Time: %us", this->timer_[hygrostat::TIMER_IDLE_ON].time / 1000);
  ESP_LOGCONFIG(TAG, "  Supports AUTO: %s", YESNO(this->supports_auto_));
  ESP_LOGCONFIG(TAG, "  Supports HUMIDIFY/DEHUMIDIFY: %s", YESNO(this->supports_humidify_dehumidify_));
  ESP_LOGCONFIG(TAG, "  Supports DEHUMIDIFY: %s", YESNO(this->supports_dehumidify_));
  ESP_LOGCONFIG(TAG, "  Supports HUMIDIFY: %s", YESNO(this->supports_humidify_));
  ESP_LOGCONFIG(TAG, "  Supports TWO SET POINTS: %s", YESNO(this->supports_two_points_));
  ESP_LOGCONFIG(TAG, "  Supports AWAY mode: %s", YESNO(this->supports_away_));
  if (this->supports_away_) {
    if (this->supports_humidify_) {
      if (this->supports_two_points_) {
        ESP_LOGCONFIG(TAG, "    Away Default Target Humidity Low: %.1f%%", this->away_config_.default_humidity_low);
      } else {
        ESP_LOGCONFIG(TAG, "    Away Default Target Humidity Low: %.1f%%", this->away_config_.default_humidity);
      }
    }
    if (this->supports_dehumidify_) {
      if (this->supports_two_points_) {
        ESP_LOGCONFIG(TAG, "    Away Default Target Humidity High: %.1f%%", this->away_config_.default_humidity_high);
      } else {
        ESP_LOGCONFIG(TAG, "    Away Default Target Humidity High: %.1f%%", this->away_config_.default_humidity);
      }
    }
  }
}

HygrostatHumidifierTargetTempConfig::HygrostatHumidifierTargetTempConfig() = default;

HygrostatHumidifierTargetTempConfig::HygrostatHumidifierTargetTempConfig(float default_humidity)
    : default_humidity(default_humidity) {}

HygrostatHumidifierTargetTempConfig::HygrostatHumidifierTargetTempConfig(float default_humidity_low,
                                                                         float default_humidity_high)
    : default_humidity_low(default_humidity_low), default_humidity_high(default_humidity_high) {}

}  // namespace hygrostat
}  // namespace esphome
