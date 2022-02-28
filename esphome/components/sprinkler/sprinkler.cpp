#include "automation.h"
#include "sprinkler.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace sprinkler {

static const char *const TAG = "sprinkler";

SprinklerSwitch::SprinklerSwitch() : turn_on_trigger_(new Trigger<>()), turn_off_trigger_(new Trigger<>()) {}

void SprinklerSwitch::loop() {
  if (!this->f_.has_value())
    return;
  auto s = (*this->f_)();
  if (!s.has_value())
    return;

  this->publish_state(*s);
}

void SprinklerSwitch::write_state(bool state) {
  if (this->prev_trigger_ != nullptr) {
    this->prev_trigger_->stop_action();
  }

  if (state) {
    this->prev_trigger_ = this->turn_on_trigger_;
    this->turn_on_trigger_->trigger();
  } else {
    this->prev_trigger_ = this->turn_off_trigger_;
    this->turn_off_trigger_->trigger();
  }

  if (this->optimistic_)
    this->publish_state(state);
}

void SprinklerSwitch::set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
bool SprinklerSwitch::assumed_state() { return this->assumed_state_; }
void SprinklerSwitch::set_state_lambda(std::function<optional<bool>()> &&f) { this->f_ = f; }
float SprinklerSwitch::get_setup_priority() const { return setup_priority::HARDWARE; }

Trigger<> *SprinklerSwitch::get_turn_on_trigger() const { return this->turn_on_trigger_; }
Trigger<> *SprinklerSwitch::get_turn_off_trigger() const { return this->turn_off_trigger_; }

void SprinklerSwitch::setup() {
  if (!this->restore_state_)
    return;

  auto restored = this->get_initial_state();
  if (!restored.has_value())
    return;

  ESP_LOGD(TAG, "  Restored state %s", ONOFF(*restored));
  if (*restored) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

void SprinklerSwitch::dump_config() {
  LOG_SWITCH("", "Sprinkler Switch", this);
  ESP_LOGCONFIG(TAG, "  Restore State: %s", YESNO(this->restore_state_));
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}

void SprinklerSwitch::set_restore_state(bool restore_state) { this->restore_state_ = restore_state; }

void SprinklerSwitch::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

void Sprinkler::setup() { this->all_valves_off_(true); }

Sprinkler::Sprinkler() {}
Sprinkler::Sprinkler(const std::string &name) : EntityBase(name) {}

void Sprinkler::add_valve(const std::string &valve_sw_name, const std::string &enable_sw_name) {
  auto new_valve_number = this->number_of_valves();
  this->valve_.resize(new_valve_number + 1);
  SprinklerValve *new_valve = &this->valve_[new_valve_number];

  new_valve->controller_switch = make_unique<SprinklerSwitch>();
  new_valve->controller_switch->set_component_source("sprinkler.switch");
  App.register_component(new_valve->controller_switch.get());
  App.register_switch(new_valve->controller_switch.get());
  new_valve->controller_switch->set_name(valve_sw_name);
  new_valve->controller_switch->set_state_lambda(
      [=]() -> optional<bool> { return this->active_valve() == new_valve_number; });

  new_valve->valve_turn_off_automation =
      make_unique<Automation<>>(new_valve->controller_switch->get_turn_off_trigger());
  new_valve->valve_shutdown_action = make_unique<sprinkler::ShutdownAction<>>(this);
  new_valve->valve_turn_off_automation->add_actions({new_valve->valve_shutdown_action.get()});

  new_valve->valve_turn_on_automation = make_unique<Automation<>>(new_valve->controller_switch->get_turn_on_trigger());
  new_valve->valve_resumeorstart_action = make_unique<sprinkler::StartSingleValveAction<>>(this);
  new_valve->valve_resumeorstart_action->set_valve_to_start(new_valve_number);
  new_valve->valve_turn_on_automation->add_actions({new_valve->valve_resumeorstart_action.get()});

  if (!enable_sw_name.empty()) {
    new_valve->enable_switch = make_unique<SprinklerSwitch>();
    new_valve->enable_switch->set_component_source("sprinkler.switch");
    App.register_component(new_valve->enable_switch.get());
    App.register_switch(new_valve->enable_switch.get());
    new_valve->enable_switch->set_name(enable_sw_name);
    new_valve->enable_switch->set_optimistic(true);
    new_valve->enable_switch->set_restore_state(true);
  }
}

void Sprinkler::add_controller(Sprinkler *other_controller) { this->other_controllers_.push_back(other_controller); }

void Sprinkler::set_controller_main_switch(SprinklerSwitch *controller_switch) {
  this->controller_sw_ = controller_switch;
  controller_switch->set_state_lambda([=]() -> optional<bool> { return this->active_valve().has_value(); });

  this->sprinkler_turn_off_automation_ = make_unique<Automation<>>(controller_switch->get_turn_off_trigger());
  this->sprinkler_shutdown_action_ = make_unique<sprinkler::ShutdownAction<>>(this);
  this->sprinkler_turn_off_automation_->add_actions({sprinkler_shutdown_action_.get()});

  this->sprinkler_turn_on_automation_ = make_unique<Automation<>>(controller_switch->get_turn_on_trigger());
  this->sprinkler_resumeorstart_action_ = make_unique<sprinkler::ResumeOrStartAction<>>(this);
  this->sprinkler_turn_on_automation_->add_actions({sprinkler_resumeorstart_action_.get()});
}

void Sprinkler::set_controller_auto_adv_switch(SprinklerSwitch *auto_adv_switch) {
  this->auto_adv_sw_ = auto_adv_switch;
  auto_adv_switch->set_optimistic(true);
  auto_adv_switch->set_restore_state(true);
}

void Sprinkler::set_controller_reverse_switch(SprinklerSwitch *reverse_switch) {
  this->reverse_sw_ = reverse_switch;
  reverse_switch->set_optimistic(true);
  reverse_switch->set_restore_state(true);
}

void Sprinkler::configure_valve_switch(const size_t valve_number, switch_::Switch *valve_switch,
                                       uint32_t valve_run_duration, switch_::Switch *pump_switch) {
  if (this->is_a_valid_valve(valve_number)) {
    this->valve_[valve_number].valve_switch = valve_switch;
    this->valve_[valve_number].valve_run_duration = valve_run_duration;
    this->valve_[valve_number].pump_switch = pump_switch;
  }
}

void Sprinkler::set_multiplier(const optional<float> multiplier) {
  if (multiplier.has_value()) {
    if (multiplier.value() > 0) {
      this->multiplier_ = multiplier.value();
    }
  }
}

void Sprinkler::set_valve_open_delay(const uint32_t valve_open_delay) {
  if (valve_open_delay > 0) {
    this->valve_overlap_ = false;
    this->switching_delay_ = valve_open_delay;
  } else {
    this->switching_delay_.reset();
  }
}

void Sprinkler::set_valve_overlap(uint32_t valve_overlap) {
  if (valve_overlap > 0) {
    this->valve_overlap_ = true;
    this->switching_delay_ = valve_overlap;
  } else {
    this->switching_delay_.reset();
  }
}

void Sprinkler::set_manual_selection_delay(uint32_t manual_selection_delay) {
  if (manual_selection_delay > 0) {
    this->manual_selection_delay_ = manual_selection_delay;
  } else {
    this->manual_selection_delay_.reset();
  }
}

void Sprinkler::set_valve_run_duration(const optional<size_t> valve_number,
                                       const optional<uint32_t> valve_run_duration) {
  if (valve_number.has_value() && valve_run_duration.has_value()) {
    if (this->is_a_valid_valve(valve_number.value())) {
      this->valve_[valve_number.value()].valve_run_duration = valve_run_duration.value();
    }
  }
}

void Sprinkler::set_auto_advance(const bool auto_advance) {
  if (this->auto_adv_sw_ != nullptr) {
    this->auto_adv_sw_->publish_state(auto_advance);
  }
}

void Sprinkler::set_repeat(optional<uint32_t> repeat) { this->target_repeats_ = repeat; }

void Sprinkler::set_reverse(const bool reverse) {
  if (this->reverse_sw_ != nullptr) {
    this->reverse_sw_->publish_state(reverse);
  }
}

uint32_t Sprinkler::valve_run_duration(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].valve_run_duration;
  }
  return 0;
}

uint32_t Sprinkler::valve_run_duration_adjusted(const size_t valve_number) {
  uint32_t run_duration = 0;

  if (this->is_a_valid_valve(valve_number)) {
    run_duration = this->valve_[valve_number].valve_run_duration;
  }
  run_duration = static_cast<uint32_t>(roundf(run_duration * this->multiplier_));

  return run_duration;
}

bool Sprinkler::auto_advance() {
  if (this->auto_adv_sw_ != nullptr) {
    return this->auto_adv_sw_->state;
  }
  return false;
}

optional<uint32_t> Sprinkler::repeat() { return this->target_repeats_; }

optional<uint32_t> Sprinkler::repeat_count() {
  // if there is an active valve and auto-advance is enabled, we may be repeating, so return the count
  if (this->auto_adv_sw_ != nullptr) {
    if (this->active_valve_.has_value() && this->auto_adv_sw_->state) {
      return this->repeat_count_;
    }
  }
  return nullopt;
}

bool Sprinkler::reverse() {
  if (this->reverse_sw_ != nullptr) {
    return this->reverse_sw_->state;
  }
  return false;
}

void Sprinkler::start_full_cycle(bool restart) {
  // if auto-advance is enabled and there is an active valve then a cycle is already in progress so don't do anything
  if (this->auto_adv_sw_ != nullptr) {
    if (!(this->auto_adv_sw_->state && this->active_valve_.has_value()) || restart) {
      // if no valves are enabled, enable them all so that auto-advance can work
      if (!this->any_valve_is_enabled_()) {
        for (auto &valve : this->valve_) {
          if (valve.enable_switch != nullptr) {
            valve.enable_switch->publish_state(true);
          }
        }
      }
      this->auto_adv_sw_->publish_state(true);
      this->reset_cycle_states_();
      // if there is no active valve, start the first valve in the cycle
      if (!this->active_valve_.has_value() || restart) {
        // activate the first valve
        this->start_valve_(this->next_valve_number_in_cycle_(this->active_valve_).value_or(0));
      }
    }
  }
}

void Sprinkler::start_single_valve(const optional<size_t> valve_number) {
  if (valve_number.has_value()) {
    if (this->auto_adv_sw_ != nullptr) {
      this->auto_adv_sw_->publish_state(false);
    }
    this->reset_cycle_states_();
    this->start_valve_(valve_number.value());
  }
}

void Sprinkler::queue_single_valve(optional<size_t> valve_number) {
  if (valve_number.has_value()) {
    if (this->is_a_valid_valve(valve_number.value())) {
      this->queued_valve_ = valve_number;
    } else {
      this->queued_valve_.reset();
    }
  }
}

void Sprinkler::next_valve() {
  if (this->manual_selection_delay_.has_value()) {
    this->manual_valve_ = this->next_valve_number_(
        this->manual_valve_.value_or(this->active_valve_.value_or(this->number_of_valves() - 1)));
    this->set_timer_duration_(sprinkler::TIMER_VALVE_SWITCHING_DELAY, this->manual_selection_delay_.value());
    this->start_timer_(sprinkler::TIMER_VALVE_SWITCHING_DELAY);
  } else {
    this->start_valve_(this->next_valve_number_(this->active_valve_.value_or(this->number_of_valves() - 1)));
  }
}

void Sprinkler::previous_valve() {
  if (this->manual_selection_delay_.has_value()) {
    this->manual_valve_ = this->previous_valve_number_(this->manual_valve_.value_or(this->active_valve_.value_or(0)));
    this->set_timer_duration_(sprinkler::TIMER_VALVE_SWITCHING_DELAY, this->manual_selection_delay_.value());
    this->start_timer_(sprinkler::TIMER_VALVE_SWITCHING_DELAY);
  } else {
    this->start_valve_(this->previous_valve_number_(this->active_valve_.value_or(0)));
  }
}

void Sprinkler::shutdown(bool clear_queue) {
  this->cancel_timer_(sprinkler::TIMER_VALVE_RUN);
  this->cancel_timer_(sprinkler::TIMER_VALVE_SWITCHING_DELAY);
  this->all_valves_off_(true);
  this->active_valve_.reset();
  this->manual_valve_.reset();
  if (clear_queue) {
    this->queued_valve_.reset();
    this->repeat_count_ = 0;
  }
}

void Sprinkler::pause() {
  // we can't pause if we're already paused
  if (this->paused_valve_.has_value()) {
    return;
  }
  this->paused_valve_ = this->active_valve();
  this->resume_duration_ = this->time_remaining();
  this->shutdown(false);
}

void Sprinkler::resume() {
  if (this->paused_valve_.has_value() && (this->resume_duration_.has_value())) {
    this->start_valve_(this->paused_valve_.value(), this->resume_duration_.value());
    this->reset_resume_();
  }
}

void Sprinkler::resume_or_start_full_cycle() {
  if (this->paused_valve_.has_value() && (this->resume_duration_.has_value())) {
    this->resume();
  } else {
    this->start_full_cycle();
  }
}

const char *Sprinkler::valve_name(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].controller_switch->get_name().c_str();
  }
  return nullptr;
}

optional<uint8_t> Sprinkler::active_valve() { return this->active_valve_; }
optional<uint8_t> Sprinkler::paused_valve() { return this->paused_valve_; }
optional<uint8_t> Sprinkler::queued_valve() { return this->queued_valve_; }
optional<uint8_t> Sprinkler::manual_valve() { return this->manual_valve_; }

size_t Sprinkler::number_of_valves() { return this->valve_.size(); }

bool Sprinkler::is_a_valid_valve(const size_t valve_number) {
  return ((valve_number >= 0) && (valve_number < this->number_of_valves()));
}

bool Sprinkler::pump_in_use(switch_::Switch *pump_switch) {
  if (this->active_valve_.has_value()) {
    if (this->is_a_valid_valve(this->active_valve_.value())) {
      return this->valve_pump_switch_(this->active_valve_.value()) == pump_switch;
    }
  }
  return false;
}

optional<uint32_t> Sprinkler::time_remaining() {
  optional<uint32_t> secs_remaining;
  if (this->active_valve_.has_value()) {
    if (this->is_a_valid_valve(this->active_valve_.value())) {
      secs_remaining = (this->timer_[sprinkler::TIMER_VALVE_RUN].start_time +
                        this->timer_[sprinkler::TIMER_VALVE_RUN].time - millis()) /
                       1000;
    }
  }
  return secs_remaining;
}

SprinklerSwitch *Sprinkler::control_switch(size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].controller_switch.get();
  }
  return nullptr;
}

SprinklerSwitch *Sprinkler::enable_switch(size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].enable_switch.get();
  }
  return nullptr;
}

uint32_t Sprinkler::hash_base() { return 3129891955UL; }

bool Sprinkler::valve_is_enabled_(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    if (this->valve_[valve_number].enable_switch != nullptr) {
      return this->valve_[valve_number].enable_switch->state;
    } else {
      return true;
    }
  }
  return false;
}

void Sprinkler::mark_valve_cycle_complete_(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    this->valve_[valve_number].valve_cycle_complete = true;
  }
}

bool Sprinkler::valve_cycle_complete_(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].valve_cycle_complete;
  }
  return false;
}

switch_::Switch *Sprinkler::valve_switch_(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].valve_switch;
  }
  return nullptr;
}

switch_::Switch *Sprinkler::valve_pump_switch_(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].pump_switch;
  }
  return nullptr;
}

uint8_t Sprinkler::next_valve_number_(const uint8_t first_valve) {
  if (this->is_a_valid_valve(first_valve) && (first_valve + 1 < this->number_of_valves()))
    return first_valve + 1;

  return 0;
}

uint8_t Sprinkler::previous_valve_number_(const uint8_t first_valve) {
  if (this->is_a_valid_valve(first_valve) && (first_valve - 1 >= 0))
    return first_valve - 1;

  return this->number_of_valves() - 1;
}

optional<uint8_t> Sprinkler::next_valve_number_in_cycle_(const optional<uint8_t> first_valve) {
  if (this->reverse_sw_ != nullptr) {
    if (this->reverse_sw_->state) {
      return this->previous_enabled_incomplete_valve_number_(first_valve);
    }
  }
  return this->next_enabled_incomplete_valve_number_(first_valve);
}

optional<uint8_t> Sprinkler::next_enabled_incomplete_valve_number_(const optional<uint8_t> first_valve) {
  auto new_valve_number = this->next_valve_number_(first_valve.value_or(this->number_of_valves() - 1));

  if (first_valve.has_value()) {
    while (new_valve_number != first_valve.value()) {
      if (this->valve_is_enabled_(new_valve_number) && (!this->valve_cycle_complete_(new_valve_number))) {
        return new_valve_number;
      } else {
        new_valve_number = this->next_valve_number_(new_valve_number);
      }
    }
  } else {
    return new_valve_number;
  }
  return nullopt;
}

optional<uint8_t> Sprinkler::previous_enabled_incomplete_valve_number_(const optional<uint8_t> first_valve) {
  auto new_valve_number = this->previous_valve_number_(first_valve.value_or(0));

  if (first_valve.has_value()) {
    while (new_valve_number != first_valve.value()) {
      if (this->valve_is_enabled_(new_valve_number) && (!this->valve_cycle_complete_(new_valve_number))) {
        return new_valve_number;
      } else {
        new_valve_number = this->previous_valve_number_(new_valve_number);
      }
    }
  } else {
    return new_valve_number;
  }
  return nullopt;
}

bool Sprinkler::any_valve_is_enabled_() {
  for (size_t valve_number = 0; valve_number < this->number_of_valves(); valve_number++) {
    if (this->valve_is_enabled_(valve_number))
      return true;
  }
  return false;
}

void Sprinkler::set_pump_state_(const size_t valve_number, const bool pump_state) {
  if (this->is_a_valid_valve(valve_number) && ((this->valve_[valve_number].pump_switch != nullptr))) {
    if (pump_state) {
      this->valve_[valve_number].pump_switch->turn_on();
    } else {
      // don't turn off the pump if it is in use by another controller
      for (auto &controller : this->other_controllers_) {
        if (controller != this) {  // dummy check
          if (controller->pump_in_use(valve_pump_switch_(valve_number))) {
            return;
          }
        }
      }
      this->valve_[valve_number].pump_switch->turn_off();
    }
  }
}

void Sprinkler::start_valve_(const optional<size_t> valve_number, optional<uint32_t> run_duration) {
  // we can't do anything if valve_number isn't present or isn't valid
  if (!valve_number.has_value()) {
    return;
  }
  if (!this->is_a_valid_valve(valve_number.value())) {
    return;
  }
  // we also won't do anything if valve_number matches the active_valve_
  if (this->active_valve_.has_value()) {
    if ((this->active_valve_.value() == valve_number.value()) && this->timer_active_(sprinkler::TIMER_VALVE_RUN)) {
      return;
    }
  }
  // sanity checks passed, compute run_duration, make valve_number the active_valve_ and start it
  this->active_valve_ = valve_number;
  if (!run_duration.has_value() || (run_duration.value() == 0)) {
    run_duration = this->valve_run_duration_adjusted(this->active_valve_.value());
  }
  this->reset_resume_();
  ESP_LOGD(TAG, "Starting valve %i for %i seconds", this->active_valve_.value(), run_duration.value());
  this->set_timer_duration_(sprinkler::TIMER_VALVE_RUN, run_duration.value());
  this->start_timer_(sprinkler::TIMER_VALVE_RUN);

  if (this->switching_delay_.has_value()) {
    this->set_timer_duration_(sprinkler::TIMER_VALVE_SWITCHING_DELAY, this->switching_delay_.value());
    this->start_timer_(sprinkler::TIMER_VALVE_SWITCHING_DELAY);
    if (this->valve_overlap_) {
      this->switch_to_valve_(this->active_valve_.value(), true);
      this->switch_to_pump_(this->active_valve_.value(), true);
    } else {
      this->switch_to_pump_(this->active_valve_.value());
      this->all_valves_off_();
    }
  } else {
    this->switch_to_valve_(this->active_valve_.value());
    this->switch_to_pump_(this->active_valve_.value());
  }
}

void Sprinkler::switch_to_pump_(size_t valve_number, bool no_shutdown) {
  if (this->is_a_valid_valve(valve_number)) {
    for (size_t valve_index = 0; valve_index < this->number_of_valves(); valve_index++) {
      if (valve_index == valve_number) {
        // always turn on the "new" pump
        this->set_pump_state_(valve_index, true);
      } else if ((valve_pump_switch_(valve_index) != valve_pump_switch_(valve_number)) && (!no_shutdown)) {
        // don't change the switch state if it's the same switch the previous valve was using
        this->set_pump_state_(valve_index, false);
      }
    }
  }
}

void Sprinkler::switch_to_valve_(size_t valve_number, bool no_shutdown) {
  if (this->is_a_valid_valve(valve_number)) {
    for (size_t valve_index = 0; valve_index < this->number_of_valves(); valve_index++) {
      if (valve_index == valve_number) {
        this->valve_[valve_index].valve_switch->turn_on();
      } else if (!no_shutdown) {
        this->valve_[valve_index].valve_switch->turn_off();
      }
    }
  }
}

void Sprinkler::all_valves_off_(const bool include_pump) {
  for (size_t valve_index = 0; valve_index < this->number_of_valves(); valve_index++) {
    this->valve_[valve_index].valve_switch->turn_off();
    if (include_pump) {
      this->set_pump_state_(valve_index, false);
    }
  }
}

void Sprinkler::reset_cycle_states_() {
  for (auto &valve : this->valve_) {
    valve.valve_cycle_complete = false;
  }
}

void Sprinkler::reset_resume_() {
  this->paused_valve_.reset();
  this->resume_duration_.reset();
}

void Sprinkler::start_timer_(const SprinklerTimerIndex timer_index) {
  if (this->timer_duration_(timer_index) > 0) {
    this->set_timeout(this->timer_[timer_index].name, this->timer_duration_(timer_index),
                      this->timer_cbf_(timer_index));
    this->timer_[timer_index].start_time = millis();
    this->timer_[timer_index].active = true;
  }
}

bool Sprinkler::cancel_timer_(const SprinklerTimerIndex timer_index) {
  this->timer_[timer_index].active = false;
  return this->cancel_timeout(this->timer_[timer_index].name);
}

bool Sprinkler::timer_active_(const SprinklerTimerIndex timer_index) { return this->timer_[timer_index].active; }

void Sprinkler::set_timer_duration_(const SprinklerTimerIndex timer_index, const uint32_t time) {
  this->timer_[timer_index].time = 1000 * time;
}

uint32_t Sprinkler::timer_duration_(const SprinklerTimerIndex timer_index) { return this->timer_[timer_index].time; }

std::function<void()> Sprinkler::timer_cbf_(const SprinklerTimerIndex timer_index) {
  return this->timer_[timer_index].func;
}

void Sprinkler::valve_cycle_timer_callback_() {
  ESP_LOGD(TAG, "Valve cycle_timer expired");
  this->timer_[sprinkler::TIMER_VALVE_RUN].active = false;
  if (!this->active_valve_.has_value()) {
    return;
  }
  this->mark_valve_cycle_complete_(this->active_valve_.value());
  if (this->auto_adv_sw_ != nullptr) {
    if (this->queued_valve_.has_value()) {
      ESP_LOGD(TAG, "  Advancing to queued valve %u", this->queued_valve_.value_or(0));
      this->start_valve_(this->queued_valve_.value());
      this->queued_valve_.reset();
    } else if (this->auto_adv_sw_->state &&
               this->next_valve_number_in_cycle_(this->active_valve_.value()).has_value()) {
      ESP_LOGD(TAG, "  Advancing to valve %u",
               this->next_valve_number_in_cycle_(this->active_valve_.value()).value_or(0));
      this->start_valve_(this->next_valve_number_in_cycle_(this->active_valve_.value()).value());
    } else if (this->auto_adv_sw_->state && (this->repeat_count_++ < this->target_repeats_.value_or(0))) {
      ESP_LOGD(TAG, "  Starting cycle %u", this->repeat_count_ + 1);
      this->start_full_cycle(true);
    } else {
      ESP_LOGD(TAG, "  Shutting down");
      this->shutdown(true);
    }
  }
}

void Sprinkler::valve_switching_delay_callback_() {
  this->timer_[sprinkler::TIMER_VALVE_SWITCHING_DELAY].active = false;
  ESP_LOGD(TAG, "Valve switching_delay timer expired");
  if (this->manual_valve_.has_value()) {
    this->start_valve_(this->manual_valve_.value());
    this->manual_valve_.reset();
  } else if (this->queued_valve_.has_value()) {
    this->start_valve_(this->queued_valve_.value());
    this->queued_valve_.reset();
  } else if (this->active_valve_.has_value()) {
    if (this->valve_overlap_) {
      ESP_LOGD(TAG, "  Deactivating previous valve, valve %u remains active", this->active_valve_.value());
      this->switch_to_pump_(this->active_valve_.value());
      this->switch_to_valve_(this->active_valve_.value());
    } else {
      ESP_LOGD(TAG, "  Activating valve %u", this->active_valve_.value());
      this->switch_to_valve_(this->active_valve_.value());
    }
  }
}

void Sprinkler::dump_config() {
  ESP_LOGCONFIG(TAG, "Sprinkler Controller -- %s", this->name_.c_str());
  if (this->manual_selection_delay_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Manual Selection Delay: %u seconds", this->manual_selection_delay_.value_or(0));
  }
  if (this->target_repeats_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Repeat Cycles: %u times", this->target_repeats_.value_or(0));
  }
  if (this->switching_delay_.has_value()) {
    if (this->valve_overlap_) {
      ESP_LOGCONFIG(TAG, "  Valve Overlap: %u seconds", this->switching_delay_.value_or(0));
    } else {
      ESP_LOGCONFIG(TAG, "  Valve Open Delay: %u seconds", this->switching_delay_.value_or(0));
    }
  }
  for (size_t valve_number = 0; valve_number < this->number_of_valves(); valve_number++) {
    ESP_LOGCONFIG(TAG, "  Valve %u:", valve_number);
    ESP_LOGCONFIG(TAG, "    Name: %s", this->valve_name(valve_number));
    ESP_LOGCONFIG(TAG, "    Run Duration: %u seconds", this->valve_[valve_number].valve_run_duration);
  }
}

}  // namespace sprinkler
}  // namespace esphome
