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

void Sprinkler::pre_setup(const std::string &name, const std::string &auto_adv_name, const std::string &reverse_name) {
  this->name_ = name;
  // set up controller's "active" switch
  this->controller_sw_.set_component_source("sprinkler.switch");
  App.register_component(&this->controller_sw_);
  App.register_switch(&this->controller_sw_);
  this->controller_sw_.set_name(name);
  this->controller_sw_.set_state_lambda(
      [=]() -> optional<bool> { return this->is_a_valid_valve(this->active_valve()); });

  this->sprinkler_turn_off_automation_ = make_unique<Automation<>>(this->controller_sw_.get_turn_off_trigger());
  this->sprinkler_shutdown_action_ = make_unique<sprinkler::ShutdownAction<>>(this);
  this->sprinkler_turn_off_automation_->add_actions({sprinkler_shutdown_action_.get()});

  this->sprinkler_turn_on_automation_ = make_unique<Automation<>>(this->controller_sw_.get_turn_on_trigger());
  this->sprinkler_resumeorstart_action_ = make_unique<sprinkler::ResumeOrStartAction<>>(this);
  this->sprinkler_turn_on_automation_->add_actions({sprinkler_resumeorstart_action_.get()});
  // set up controller's "auto-advance" switch
  this->auto_adv_sw_.set_component_source("sprinkler.switch");
  App.register_component(&this->auto_adv_sw_);
  App.register_switch(&this->auto_adv_sw_);
  this->auto_adv_sw_.set_name(auto_adv_name);
  this->auto_adv_sw_.set_optimistic(true);
  this->auto_adv_sw_.set_restore_state(true);
  // set up controller's "reverse" switch
  this->reverse_sw_.set_component_source("sprinkler.switch");
  App.register_component(&this->reverse_sw_);
  App.register_switch(&this->reverse_sw_);
  this->reverse_sw_.set_name(reverse_name);
  this->reverse_sw_.set_optimistic(true);
  this->reverse_sw_.set_restore_state(true);
}

void Sprinkler::setup() { this->all_valves_off_(true); }

Sprinkler::Sprinkler() {}

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

  new_valve->enable_switch = make_unique<SprinklerSwitch>();
  new_valve->enable_switch->set_component_source("sprinkler.switch");
  App.register_component(new_valve->enable_switch.get());
  App.register_switch(new_valve->enable_switch.get());
  new_valve->enable_switch->set_name(enable_sw_name);
  new_valve->enable_switch->set_optimistic(true);
  new_valve->enable_switch->set_restore_state(true);
}

void Sprinkler::add_controller(Sprinkler *other_controller) { this->other_controllers_.push_back(other_controller); }

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
  this->set_timer_duration_(sprinkler::TIMER_VALVE_OVERLAP_DELAY, 0);
  this->set_timer_duration_(sprinkler::TIMER_VALVE_OPEN_DELAY, valve_open_delay);
}

void Sprinkler::set_valve_overlap(uint32_t valve_overlap) {
  this->set_timer_duration_(sprinkler::TIMER_VALVE_OPEN_DELAY, 0);
  this->set_timer_duration_(sprinkler::TIMER_VALVE_OVERLAP_DELAY, valve_overlap);
}

void Sprinkler::set_valve_run_duration(const size_t valve_number, const uint32_t valve_run_duration) {
  if (this->is_a_valid_valve(valve_number)) {
    this->valve_[valve_number].valve_run_duration = valve_run_duration;
  }
}

void Sprinkler::set_auto_advance(const bool auto_advance) { this->auto_adv_sw_.publish_state(auto_advance); }

void Sprinkler::set_reverse(const bool reverse) { this->reverse_sw_.publish_state(reverse); }

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

bool Sprinkler::auto_advance() { return this->auto_adv_sw_.state; }

bool Sprinkler::reverse() { return this->reverse_sw_.state; }

void Sprinkler::start_full_cycle() {
  // if auto-advance is enabled and there_is_an_active_valve_ then a cycle is already in progress so don't do anything
  if (!(this->auto_adv_sw_.state && this->there_is_an_active_valve_())) {
    // if no valves are enabled, enable them all so that auto-advance can work
    if (!this->any_valve_is_enabled_()) {
      for (auto &valve : this->valve_) {
        valve.enable_switch->publish_state(true);
      }
    }
    this->auto_adv_sw_.publish_state(true);
    this->reset_cycle_states_();
    if (!this->there_is_an_active_valve_()) {
      // activate the first valve
      this->start_valve_(this->next_valve_number_in_cycle_(this->active_valve_));
    }
  }
}

void Sprinkler::start_single_valve(const optional<size_t> valve_number) {
  if (valve_number.has_value()) {
    this->auto_adv_sw_.publish_state(false);
    this->reset_cycle_states_();
    this->start_valve_(valve_number.value());
  }
}

void Sprinkler::next_valve() { this->start_valve_(this->next_valve_number_(this->active_valve_)); }

void Sprinkler::previous_valve() { this->start_valve_(this->previous_valve_number_(this->active_valve_)); }

void Sprinkler::shutdown() {
  this->cancel_timer_(sprinkler::TIMER_VALVE_RUN_DURATION);
  this->cancel_timer_(sprinkler::TIMER_VALVE_OPEN_DELAY);
  this->all_valves_off_(true);
  this->active_valve_ = this->none_active;
}

void Sprinkler::pause() {
  if (!this->is_a_valid_valve(this->paused_valve_)) {
    this->paused_valve_ = this->active_valve();
    this->resume_duration_ = this->time_remaining();
    this->shutdown();
  }
}

void Sprinkler::resume() {
  if (this->is_a_valid_valve(this->paused_valve_) && (this->resume_duration_ > 0)) {
    this->start_valve_(this->paused_valve_, this->resume_duration_);
  }
}

void Sprinkler::resume_or_start_full_cycle() {
  if (this->is_a_valid_valve(this->paused_valve_) && (this->resume_duration_ > 0)) {
    this->resume();
  } else {
    this->start_full_cycle();
  }
}

const char *Sprinkler::valve_name(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].valve_switch->get_name().c_str();
  }
  return nullptr;
}

int8_t Sprinkler::active_valve() { return this->active_valve_; }

int8_t Sprinkler::paused_valve() { return this->paused_valve_; }

bool Sprinkler::is_a_valid_valve(const size_t valve_number) {
  return ((valve_number >= 0) && (valve_number < this->number_of_valves()));
}

size_t Sprinkler::number_of_valves() { return this->valve_.size(); }

bool Sprinkler::pump_in_use(switch_::Switch *pump_switch) {
  if (this->is_a_valid_valve(this->active_valve_)) {
    return this->valve_pump_switch_(this->active_valve_) == pump_switch;
  }
  return false;
}

uint32_t Sprinkler::time_remaining() {
  uint32_t secs_remaining = 0;
  if (this->is_a_valid_valve(this->active_valve_)) {
    secs_remaining = (this->timer_[sprinkler::TIMER_VALVE_RUN_DURATION].start_time +
                      this->timer_[sprinkler::TIMER_VALVE_RUN_DURATION].time - millis()) /
                     1000;
  }
  return secs_remaining;
}

bool Sprinkler::there_is_an_active_valve_() {
  return ((this->active_valve_ >= 0) && (this->active_valve_ < this->number_of_valves()));
}

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

int8_t Sprinkler::next_valve_number_(const int8_t first_valve) {
  if (this->is_a_valid_valve(first_valve) && (first_valve + 1 < this->number_of_valves()))
    return first_valve + 1;

  return 0;
}

int8_t Sprinkler::previous_valve_number_(const int8_t first_valve) {
  if (this->is_a_valid_valve(first_valve) && (first_valve - 1 >= 0))
    return first_valve - 1;

  return this->number_of_valves() - 1;
}

int8_t Sprinkler::next_valve_number_in_cycle_(const int8_t first_valve) {
  if (this->reverse_sw_.state)
    return this->previous_enabled_incomplete_valve_number_(first_valve);

  return this->next_enabled_incomplete_valve_number_(first_valve);
}

int8_t Sprinkler::next_enabled_incomplete_valve_number_(const int8_t first_valve) {
  auto new_valve_number = this->next_valve_number_(first_valve);

  while (new_valve_number != first_valve) {
    if (this->valve_is_enabled_(new_valve_number) && (!this->valve_cycle_complete_(new_valve_number))) {
      return new_valve_number;
    } else {
      new_valve_number = this->next_valve_number_(new_valve_number);
    }
  }
  return this->none_active;
}

int8_t Sprinkler::previous_enabled_incomplete_valve_number_(const int8_t first_valve) {
  auto new_valve_number = this->previous_valve_number_(first_valve);

  while (new_valve_number != first_valve) {
    if (this->valve_is_enabled_(new_valve_number) && (!this->valve_cycle_complete_(new_valve_number))) {
      return new_valve_number;
    } else {
      new_valve_number = this->previous_valve_number_(new_valve_number);
    }
  }
  return this->none_active;
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

void Sprinkler::start_valve_(const size_t valve_number, uint32_t run_duration) {
  if (this->is_a_valid_valve(valve_number) && (valve_number != this->active_valve_)) {
    this->active_valve_ = valve_number;
    if (run_duration == 0) {
      run_duration = this->valve_run_duration_adjusted(this->active_valve_);
    }
    this->reset_resume_();
    ESP_LOGD(TAG, "Starting valve %i for %i seconds", this->active_valve_, run_duration);
    this->set_timer_duration_(sprinkler::TIMER_VALVE_RUN_DURATION, run_duration);
    this->start_timer_(sprinkler::TIMER_VALVE_RUN_DURATION);

    if (this->timer_duration_(sprinkler::TIMER_VALVE_OPEN_DELAY) > 0) {
      this->start_timer_(sprinkler::TIMER_VALVE_OPEN_DELAY);
      this->switch_to_pump_(this->active_valve_);
      this->all_valves_off_();
    } else if (this->timer_duration_(sprinkler::TIMER_VALVE_OVERLAP_DELAY) > 0) {
      this->start_timer_(sprinkler::TIMER_VALVE_OVERLAP_DELAY);
      this->switch_to_valve_(this->active_valve_, true);
      this->switch_to_pump_(this->active_valve_, true);
    } else {
      this->switch_to_valve_(this->active_valve_);
      this->switch_to_pump_(this->active_valve_);
    }
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
  this->paused_valve_ = this->none_active;
  this->resume_duration_ = 0;
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

void Sprinkler::valve_open_delay_callback_() {
  ESP_LOGD(TAG, "open_delay timer expired; activating valve %i", this->active_valve_);
  this->timer_[sprinkler::TIMER_VALVE_OPEN_DELAY].active = false;
  this->switch_to_valve_(this->active_valve_);
}

void Sprinkler::valve_overlap_delay_callback_() {
  ESP_LOGD(TAG, "overlap_delay timer expired; deactivating previous valve, valve %i remains active",
           this->active_valve_);
  this->timer_[sprinkler::TIMER_VALVE_OVERLAP_DELAY].active = false;
  this->switch_to_pump_(this->active_valve_);
  this->switch_to_valve_(this->active_valve_);
}

void Sprinkler::valve_cycle_complete_callback_() {
  ESP_LOGD(TAG, "cycle_complete timer expired:");
  this->timer_[sprinkler::TIMER_VALVE_RUN_DURATION].active = false;
  this->mark_valve_cycle_complete_(this->active_valve_);
  if (this->auto_adv_sw_.state && this->is_a_valid_valve(this->next_valve_number_in_cycle_(this->active_valve_))) {
    ESP_LOGD(TAG, "  Advancing to valve %i", this->next_valve_number_in_cycle_(this->active_valve_));
    this->start_valve_(this->next_valve_number_in_cycle_(this->active_valve_));
  } else {
    ESP_LOGD(TAG, "  Shutting down");
    this->all_valves_off_(true);
    this->active_valve_ = this->none_active;
  }
}

void Sprinkler::dump_config() {
  ESP_LOGCONFIG(TAG, "Sprinkler Controller");
  if (this->timer_duration_(sprinkler::TIMER_VALVE_OPEN_DELAY)) {
    ESP_LOGCONFIG(TAG, "  Valve Open Delay: %u seconds", this->timer_duration_(sprinkler::TIMER_VALVE_OPEN_DELAY));
  }
  if (this->timer_duration_(sprinkler::TIMER_VALVE_OVERLAP_DELAY)) {
    ESP_LOGCONFIG(TAG, "  Valve Overlap: %u seconds", this->timer_duration_(sprinkler::TIMER_VALVE_OVERLAP_DELAY));
  }
  for (size_t valve_number = 0; valve_number < this->number_of_valves(); valve_number++) {
    ESP_LOGCONFIG(TAG, "  Valve %u:", valve_number);
    ESP_LOGCONFIG(TAG, "    Name: %s", this->valve_name(valve_number));
    ESP_LOGCONFIG(TAG, "    Run Duration: %u seconds", this->valve_[valve_number].valve_run_duration);
  }
}

}  // namespace sprinkler
}  // namespace esphome
