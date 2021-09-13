#include "sprinkler.h"

#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace sprinkler {

static const char *const TAG = "sprinkler";

void Sprinkler::setup() { this->all_valves_off_(true); }

Sprinkler::Sprinkler() {}

void Sprinkler::add_valve(switch_::Switch *valve_switch, const uint32_t valve_run_duration, std::string valve_name) {
  this->valve_.push_back(SprinklerValve{std::move(valve_name), valve_switch, nullptr, valve_run_duration, false});
}

void Sprinkler::add_valve(switch_::Switch *valve_switch, switch_::Switch *enable_switch,
                          const uint32_t valve_run_duration, std::string valve_name) {
  this->valve_.push_back(SprinklerValve{std::move(valve_name), valve_switch, enable_switch, valve_run_duration, false});
}

void Sprinkler::set_pump_switch(switch_::Switch *pump_switch) { this->pump_switch_ = pump_switch; }

void Sprinkler::set_multiplier(const float multiplier) {
  if (multiplier > 0) {
    this->multiplier_ = multiplier;
  }
}

void Sprinkler::set_valve_open_delay(const uint32_t valve_open_delay) {
  this->set_timer_duration_(sprinkler::TIMER_VALVE_OPEN_DELAY, valve_open_delay);
}

void Sprinkler::set_valve_run_duration(const uint8_t valve_number, const uint32_t valve_run_duration) {
  if (this->is_a_valid_valve(valve_number)) {
    this->valve_[valve_number].valve_run_duration = valve_run_duration;
  }
}

void Sprinkler::set_auto_advance(const bool auto_advance) { this->auto_advance_ = auto_advance; }

void Sprinkler::set_reverse(const bool reverse) { this->reverse_ = reverse; }

uint32_t Sprinkler::valve_run_duration(const uint8_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].valve_run_duration;
  }
  return 0;
}

uint32_t Sprinkler::valve_run_duration_adjusted(const uint8_t valve_number) {
  uint32_t run_duration = 0;

  if (this->is_a_valid_valve(valve_number)) {
    run_duration = this->valve_[valve_number].valve_run_duration;
  }
  run_duration = static_cast<uint32_t>(roundf(run_duration * this->multiplier_));

  return run_duration;
}

bool Sprinkler::auto_advance() { return this->auto_advance_; }

bool Sprinkler::reverse() { return this->reverse_; }

void Sprinkler::start_full_cycle() {
  // if auto_advance_ is enabled and there_is_an_active_valve_ then a cycle is already in progress so don't do anything
  if (!(this->auto_advance_ && this->there_is_an_active_valve_())) {
    // if no valves are enabled, enable them all so that auto-advance can work
    if (!this->any_valve_is_enabled_()) {
      for (auto &valve : this->valve_) {
        valve.enable_switch->turn_on();
      }
    }
    this->auto_advance_ = true;
    this->reset_cycle_states_();
    if (!this->there_is_an_active_valve_()) {
      // activate the first valve
      this->start_valve_(this->next_valve_number_in_cycle_(this->active_valve_));
    }
  }
}

void Sprinkler::start_single_valve(const uint8_t valve_number) {
  this->auto_advance_ = false;
  this->reset_cycle_states_();
  this->start_valve_(valve_number);
}

void Sprinkler::next_valve() { this->start_valve_(this->next_valve_number_(this->active_valve_)); }

void Sprinkler::previous_valve() { this->start_valve_(this->previous_valve_number_(this->active_valve_)); }

void Sprinkler::shutdown() {
  this->cancel_timer_(sprinkler::TIMER_VALVE_RUN_DURATION);
  this->cancel_timer_(sprinkler::TIMER_VALVE_OPEN_DELAY);
  this->all_valves_off_(true);
  this->active_valve_ = this->none_active;
}

const char *Sprinkler::valve_name(const uint8_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].valve_name.c_str();
  }
  return nullptr;
}

int8_t Sprinkler::active_valve() { return this->active_valve_; }

bool Sprinkler::is_a_valid_valve(const int8_t valve_number) {
  return ((valve_number >= 0) && (valve_number < this->number_of_valves()));
}

size_t Sprinkler::number_of_valves() { return this->valve_.size(); }

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

bool Sprinkler::valve_is_enabled_(const uint8_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    if (this->valve_[valve_number].enable_switch != nullptr) {
      return this->valve_[valve_number].enable_switch->state;
    } else {
      return true;
    }
  }
  return false;
}

void Sprinkler::mark_valve_cycle_complete_(const uint8_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    this->valve_[valve_number].valve_cycle_complete = true;
  }
}

bool Sprinkler::valve_cycle_complete_(const uint8_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].valve_cycle_complete;
  }
  return false;
}

switch_::Switch *Sprinkler::valve_switch_(const uint8_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].valve_switch;
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
  if (this->reverse_)
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
  for (uint8_t valve_number = 0; valve_number < this->number_of_valves(); valve_number++) {
    if (this->valve_is_enabled_(valve_number))
      return true;
  }
  return false;
}

void Sprinkler::set_pump_state_(const bool pump_state) {
  if (this->pump_switch_ != nullptr) {
    if (pump_state) {
      this->pump_switch_->turn_on();
    } else {
      this->pump_switch_->turn_off();
    }
  }
}

void Sprinkler::activate_active_valve_() {
  if (this->is_a_valid_valve(this->active_valve_)) {
    this->valve_switch_(this->active_valve_)->turn_on();
  }
}

void Sprinkler::start_valve_(const uint8_t valve_number) {
  if (this->is_a_valid_valve(valve_number) && (valve_number != this->active_valve_)) {
    this->all_valves_off_();
    this->active_valve_ = valve_number;
    this->set_pump_state_(true);
    this->set_timer_duration_(sprinkler::TIMER_VALVE_RUN_DURATION,
                              this->valve_run_duration_adjusted(this->active_valve_));
    ESP_LOGD(TAG, "Starting valve %i for %i seconds", this->active_valve_,
             this->valve_run_duration_adjusted(this->active_valve_));
    this->start_timer_(sprinkler::TIMER_VALVE_RUN_DURATION);
    if (this->timer_duration_(sprinkler::TIMER_VALVE_OPEN_DELAY) > 0) {
      this->start_timer_(sprinkler::TIMER_VALVE_OPEN_DELAY);
    } else {
      this->activate_active_valve_();
    }
  }
}

void Sprinkler::all_valves_off_(const bool include_pump) {
  if (include_pump) {
    this->set_pump_state_(false);
  }
  for (auto &valve : this->valve_) {
    valve.valve_switch->turn_off();
  }
}

void Sprinkler::reset_cycle_states_() {
  for (auto &valve : this->valve_) {
    valve.valve_cycle_complete = false;
  }
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
  this->activate_active_valve_();
}

void Sprinkler::valve_cycle_complete_callback_() {
  ESP_LOGD(TAG, "cycle_complete timer expired:");
  this->timer_[sprinkler::TIMER_VALVE_RUN_DURATION].active = false;
  this->mark_valve_cycle_complete_(this->active_valve_);
  if (this->auto_advance_ && this->is_a_valid_valve(this->next_valve_number_in_cycle_(this->active_valve_))) {
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
  ESP_LOGCONFIG(TAG, "  Supports PUMP: %s", YESNO(this->pump_switch_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Valve Open Delay: %u seconds", this->timer_duration_(sprinkler::TIMER_VALVE_OPEN_DELAY));
  for (size_t valve_number = 0; valve_number < this->number_of_valves(); valve_number++) {
    ESP_LOGCONFIG(TAG, "  Valve %u:", valve_number);
    ESP_LOGCONFIG(TAG, "    Run Duration: %u seconds", this->valve_[valve_number].valve_run_duration);
    ESP_LOGCONFIG(TAG, "    Name: %s", this->valve_[valve_number].valve_name.c_str());
  }
}

}  // namespace sprinkler
}  // namespace esphome
