#include "automation.h"
#include "sprinkler.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace sprinkler {

static const char *const TAG = "sprinkler";

SprinklerSwitch::SprinklerSwitch() {}
SprinklerSwitch::SprinklerSwitch(switch_::Switch *sprinkler_switch) : on_switch_(sprinkler_switch) {}
SprinklerSwitch::SprinklerSwitch(switch_::Switch *off_switch, switch_::Switch *on_switch, uint32_t pulse_duration)
    : pulse_duration_(pulse_duration), off_switch_(off_switch), on_switch_(on_switch) {}

bool SprinklerSwitch::is_latching_valve() { return (this->off_switch_ != nullptr) && (this->on_switch_ != nullptr); }

void SprinklerSwitch::loop() {
  if ((this->pinned_millis_) && (millis() > this->pinned_millis_ + this->pulse_duration_)) {
    this->pinned_millis_ = 0;  // reset tracker
    if (this->off_switch_->state) {
      this->off_switch_->turn_off();
    }
    if (this->on_switch_->state) {
      this->on_switch_->turn_off();
    }
  }
}

void SprinklerSwitch::turn_off() {
  if (!this->state()) {  // do nothing if we're already in the requested state
    return;
  }
  if (this->off_switch_ != nullptr) {  // latching valve, start a pulse
    if (!this->off_switch_->state) {
      this->off_switch_->turn_on();
    }
    this->pinned_millis_ = millis();
  } else if (this->on_switch_ != nullptr) {  // non-latching valve
    this->on_switch_->turn_off();
  }
  this->state_ = false;
}

void SprinklerSwitch::turn_on() {
  if (this->state()) {  // do nothing if we're already in the requested state
    return;
  }
  if (this->off_switch_ != nullptr) {  // latching valve, start a pulse
    if (!this->on_switch_->state) {
      this->on_switch_->turn_on();
    }
    this->pinned_millis_ = millis();
  } else if (this->on_switch_ != nullptr) {  // non-latching valve
    this->on_switch_->turn_on();
  }
  this->state_ = true;
}

bool SprinklerSwitch::state() {
  if ((this->off_switch_ == nullptr) && (this->on_switch_ != nullptr)) {  // latching valve is not configured...
    return this->on_switch_->state;                                       // ...so just return the pump switch state
  }
  return this->state_;
}

void SprinklerSwitch::sync_valve_state(bool latch_state) {
  if (this->is_latching_valve()) {
    this->state_ = latch_state;
  } else if (this->on_switch_ != nullptr) {
    this->state_ = this->on_switch_->state;
  }
}

SprinklerControllerSwitch::SprinklerControllerSwitch()
    : turn_on_trigger_(new Trigger<>()), turn_off_trigger_(new Trigger<>()) {}

void SprinklerControllerSwitch::loop() {
  if (!this->f_.has_value())
    return;
  auto s = (*this->f_)();
  if (!s.has_value())
    return;

  this->publish_state(*s);
}

void SprinklerControllerSwitch::write_state(bool state) {
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

void SprinklerControllerSwitch::set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
bool SprinklerControllerSwitch::assumed_state() { return this->assumed_state_; }
void SprinklerControllerSwitch::set_state_lambda(std::function<optional<bool>()> &&f) { this->f_ = f; }
float SprinklerControllerSwitch::get_setup_priority() const { return setup_priority::HARDWARE; }

Trigger<> *SprinklerControllerSwitch::get_turn_on_trigger() const { return this->turn_on_trigger_; }
Trigger<> *SprinklerControllerSwitch::get_turn_off_trigger() const { return this->turn_off_trigger_; }

void SprinklerControllerSwitch::setup() {
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

void SprinklerControllerSwitch::dump_config() {
  LOG_SWITCH("", "Sprinkler Switch", this);
  ESP_LOGCONFIG(TAG, "  Restore State: %s", YESNO(this->restore_state_));
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}

void SprinklerControllerSwitch::set_restore_state(bool restore_state) { this->restore_state_ = restore_state; }

void SprinklerControllerSwitch::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

SprinklerValveOperator::SprinklerValveOperator() {}
SprinklerValveOperator::SprinklerValveOperator(SprinklerValve *valve, Sprinkler *controller)
    : controller_(controller), valve_(valve) {}

void SprinklerValveOperator::loop() {
  if (millis() >= this->pinned_millis_) {  // dummy check
    switch (this->state_) {
      case STARTING:
        if (millis() > (this->pinned_millis_ + this->start_delay_)) {
          this->run_();  // start_delay_ has been exceeded, so ensure both valves are on and update the state
        }
        break;

      case ACTIVE:
        if (millis() > (this->pinned_millis_ + this->start_delay_ + this->run_duration_)) {
          this->stop();  // start_delay_ + run_duration_ has been exceeded, start shutting down
        }
        break;

      case STOPPING:
        if (millis() > (this->pinned_millis_ + this->stop_delay_)) {
          this->kill_();  // stop_delay_has been exceeded, ensure all valves are off
        }
        break;

      default:
        break;
    }
  } else {         // perhaps millis() rolled over...or something else is horribly wrong!
    this->stop();  // bail out (TODO: handle this highly unlikely situation better...)
  }
}

void SprinklerValveOperator::set_controller(Sprinkler *controller) {
  if (controller != nullptr) {
    this->controller_ = controller;
  }
}

void SprinklerValveOperator::set_valve(SprinklerValve *valve) {
  if (valve != nullptr) {
    this->state_ = IDLE;       // reset state
    this->run_duration_ = 0;   // reset to ensure the valve isn't started without updating it
    this->pinned_millis_ = 0;  // reset because (new) valve has not been started yet
    this->kill_();             // ensure everything is off before we let go!
    this->valve_ = valve;      // finally, set the pointer to the new valve
  }
}

void SprinklerValveOperator::set_run_duration(uint32_t run_duration) {
  if (run_duration) {
    this->run_duration_ = run_duration * 1000;
  }
}

void SprinklerValveOperator::set_start_delay(uint32_t start_delay, bool start_delay_is_valve_delay) {
  this->start_delay_is_valve_delay_ = start_delay_is_valve_delay;
  this->start_delay_ = start_delay * 1000;  // because 1000 milliseconds is one second
}

void SprinklerValveOperator::set_stop_delay(uint32_t stop_delay, bool stop_delay_is_valve_delay) {
  this->stop_delay_is_valve_delay_ = stop_delay_is_valve_delay;
  this->stop_delay_ = stop_delay * 1000;  // because 1000 milliseconds is one second
}

void SprinklerValveOperator::start() {
  if (!this->run_duration_) {  // can't start if zero run duration
    return;
  }
  if (this->start_delay_ && (this->pump_switch() != nullptr)) {
    this->state_ = STARTING;  // STARTING state requires both a pump and a start_delay_
    if (this->start_delay_is_valve_delay_) {
      this->pump_on_();
    } else if (!this->pump_switch()->state()) {  // if the pump is already on, wait to switch on the valve
      this->valve_on_();                         //  to ensure consistent run time
    }
  } else {
    this->run_();  // there is no start_delay_, so just start the pump and valve
  }
  this->pinned_millis_ = millis();  // save the time the start request was made
}

void SprinklerValveOperator::stop() {
  if ((this->state_ == IDLE) || (this->state_ == STOPPING)) {  // can't stop if already stopped or stopping
    return;
  }
  if (this->stop_delay_ && (this->pump_switch() != nullptr)) {
    this->state_ = STOPPING;  // STOPPING state requires both a pump and a stop_delay_
    if (this->stop_delay_is_valve_delay_) {
      this->pump_off_();
    } else {
      this->valve_off_();
    }
    if (this->pump_switch()->state()) {  // if the pump is still on at this point, it may be in use...
      this->valve_off_();                // ...so just switch the valve off now to ensure consistent run time
    }
    this->pinned_millis_ = millis();  // save the time the stop request was made
  } else {
    this->kill_();  // there is no stop_delay_, so just stop the pump and valve
  }
}

uint32_t SprinklerValveOperator::run_duration() { return this->run_duration_; }

uint32_t SprinklerValveOperator::time_remaining() {
  if ((this->state_ == STARTING) || (this->state_ == ACTIVE)) {
    return (this->pinned_millis_ + this->start_delay_ + this->run_duration_ - millis()) / 1000;
  }
  return 0;
}

SprinklerState SprinklerValveOperator::state() { return this->state_; }

SprinklerSwitch *SprinklerValveOperator::pump_switch() {
  if ((this->controller_ == nullptr) || (this->valve_ == nullptr)) {
    return nullptr;
  }
  if (this->valve_->pump_switch_index.has_value()) {
    return this->controller_->valve_pump_switch_by_pump_index(this->valve_->pump_switch_index.value());
  }
  return nullptr;
}

void SprinklerValveOperator::pump_off_() {
  if ((this->valve_ == nullptr) || (this->pump_switch() == nullptr)) {  // safety first!
    return;
  }
  if (this->controller_ == nullptr) {  // safety first!
    this->pump_switch()->turn_off();   // if no controller was set, just switch off the pump
  } else {                             // ...otherwise, do it "safely"
    auto state = this->state_;         // this is silly, but...
    this->state_ = BYPASS;             // ...exclude me from the pump-in-use check that set_pump_state() does
    this->controller_->set_pump_state(this->pump_switch(), false);
    this->state_ = state;
  }
}

void SprinklerValveOperator::pump_on_() {
  if ((this->valve_ == nullptr) || (this->pump_switch() == nullptr)) {  // safety first!
    return;
  }
  if (this->controller_ == nullptr) {  // safety first!
    this->pump_switch()->turn_on();    // if no controller was set, just switch on the pump
  } else {                             // ...otherwise, do it "safely"
    auto state = this->state_;         // this is silly, but...
    this->state_ = BYPASS;             // ...exclude me from the pump-in-use check that set_pump_state() does
    this->controller_->set_pump_state(this->pump_switch(), true);
    this->state_ = state;
  }
}

void SprinklerValveOperator::valve_off_() {
  if (this->valve_ == nullptr) {  // safety first!
    return;
  }
  if (this->valve_->valve_switch.state()) {
    this->valve_->valve_switch.turn_off();
  }
}

void SprinklerValveOperator::valve_on_() {
  if (this->valve_ == nullptr) {  // safety first!
    return;
  }
  if (!this->valve_->valve_switch.state()) {
    this->valve_->valve_switch.turn_on();
  }
}

void SprinklerValveOperator::kill_() {
  this->state_ = IDLE;
  this->valve_off_();
  this->pump_off_();
}

void SprinklerValveOperator::run_() {
  this->state_ = ACTIVE;
  this->valve_on_();
  this->pump_on_();
}

SprinklerValveRunRequest::SprinklerValveRunRequest() {}
SprinklerValveRunRequest::SprinklerValveRunRequest(size_t valve_number, uint32_t run_duration,
                                                   SprinklerValveOperator *valve_op)
    : valve_number_(valve_number), run_duration_(run_duration), valve_op_(valve_op) {}

bool SprinklerValveRunRequest::has_request() { return this->has_valve_; }
bool SprinklerValveRunRequest::has_valve_operator() { return !(this->valve_op_ == nullptr); }

void SprinklerValveRunRequest::set_run_duration(uint32_t run_duration) { this->run_duration_ = run_duration; }

void SprinklerValveRunRequest::set_valve(size_t valve_number) {
  this->valve_number_ = valve_number;
  this->run_duration_ = 0;
  this->valve_op_ = nullptr;
  this->has_valve_ = true;
}

void SprinklerValveRunRequest::set_valve_operator(SprinklerValveOperator *valve_op) {
  if (valve_op != nullptr) {
    this->valve_op_ = valve_op;
  }
}

void SprinklerValveRunRequest::reset() {
  this->has_valve_ = false;
  this->run_duration_ = 0;
  this->valve_op_ = nullptr;
}

uint32_t SprinklerValveRunRequest::run_duration() { return this->run_duration_; }

size_t SprinklerValveRunRequest::valve() { return this->valve_number_; }

optional<size_t> SprinklerValveRunRequest::valve_as_opt() {
  if (this->has_valve_) {
    return this->valve_number_;
  }
  return nullopt;
}

SprinklerValveOperator *SprinklerValveRunRequest::valve_operator() { return this->valve_op_; }

Sprinkler::Sprinkler() {}
Sprinkler::Sprinkler(const std::string &name) : EntityBase(name) {}

void Sprinkler::setup() { this->all_valves_off_(true); }

void Sprinkler::loop() {
  for (auto &p : this->pump_) {
    p.loop();
  }
  for (auto &v : this->valve_) {
    v.valve_switch.loop();
  }
  for (auto &vo : this->valve_op_) {
    vo.loop();
  }
}

void Sprinkler::add_valve(SprinklerControllerSwitch *valve_sw, SprinklerControllerSwitch *enable_sw) {
  auto new_valve_number = this->number_of_valves();
  this->valve_.resize(new_valve_number + 1);
  SprinklerValve *new_valve = &this->valve_[new_valve_number];

  new_valve->controller_switch = valve_sw;
  new_valve->controller_switch->set_state_lambda([=]() -> optional<bool> {
    if (this->valve_pump_switch(new_valve_number) != nullptr) {
      return this->valve_switch(new_valve_number)->state() && this->valve_pump_switch(new_valve_number)->state();
    }
    return this->valve_switch(new_valve_number)->state();
  });

  new_valve->valve_turn_off_automation =
      make_unique<Automation<>>(new_valve->controller_switch->get_turn_off_trigger());
  new_valve->valve_shutdown_action = make_unique<sprinkler::ShutdownAction<>>(this);
  new_valve->valve_turn_off_automation->add_actions({new_valve->valve_shutdown_action.get()});

  new_valve->valve_turn_on_automation = make_unique<Automation<>>(new_valve->controller_switch->get_turn_on_trigger());
  new_valve->valve_resumeorstart_action = make_unique<sprinkler::StartSingleValveAction<>>(this);
  new_valve->valve_resumeorstart_action->set_valve_to_start(new_valve_number);
  new_valve->valve_turn_on_automation->add_actions({new_valve->valve_resumeorstart_action.get()});

  if (enable_sw != nullptr) {
    new_valve->enable_switch = enable_sw;
    new_valve->enable_switch->set_optimistic(true);
    new_valve->enable_switch->set_restore_state(true);
  }
}

void Sprinkler::add_controller(Sprinkler *other_controller) { this->other_controllers_.push_back(other_controller); }

void Sprinkler::set_controller_main_switch(SprinklerControllerSwitch *controller_switch) {
  this->controller_sw_ = controller_switch;
  controller_switch->set_state_lambda([=]() -> optional<bool> {
    for (size_t valve_number = 0; valve_number < this->number_of_valves(); valve_number++) {
      if (this->valve_[valve_number].controller_switch->state) {
        return true;
      }
    }
    return this->active_req_.has_request();
  });

  this->sprinkler_turn_off_automation_ = make_unique<Automation<>>(controller_switch->get_turn_off_trigger());
  this->sprinkler_shutdown_action_ = make_unique<sprinkler::ShutdownAction<>>(this);
  this->sprinkler_turn_off_automation_->add_actions({sprinkler_shutdown_action_.get()});

  this->sprinkler_turn_on_automation_ = make_unique<Automation<>>(controller_switch->get_turn_on_trigger());
  this->sprinkler_resumeorstart_action_ = make_unique<sprinkler::ResumeOrStartAction<>>(this);
  this->sprinkler_turn_on_automation_->add_actions({sprinkler_resumeorstart_action_.get()});
}

void Sprinkler::set_controller_auto_adv_switch(SprinklerControllerSwitch *auto_adv_switch) {
  this->auto_adv_sw_ = auto_adv_switch;
  auto_adv_switch->set_optimistic(true);
  auto_adv_switch->set_restore_state(true);
}

void Sprinkler::set_controller_queue_enable_switch(SprinklerControllerSwitch *queue_enable_switch) {
  this->queue_enable_sw_ = queue_enable_switch;
  queue_enable_switch->set_optimistic(true);
  queue_enable_switch->set_restore_state(true);
}

void Sprinkler::set_controller_reverse_switch(SprinklerControllerSwitch *reverse_switch) {
  this->reverse_sw_ = reverse_switch;
  reverse_switch->set_optimistic(true);
  reverse_switch->set_restore_state(true);
}

void Sprinkler::configure_valve_switch(size_t valve_number, switch_::Switch *valve_switch, uint32_t run_duration) {
  if (this->is_a_valid_valve(valve_number)) {
    this->valve_[valve_number].valve_switch.set_on_switch(valve_switch);
    this->valve_[valve_number].run_duration = run_duration;
  }
}

void Sprinkler::configure_valve_switch_pulsed(size_t valve_number, switch_::Switch *valve_switch_off,
                                              switch_::Switch *valve_switch_on, uint32_t pulse_duration,
                                              uint32_t run_duration) {
  if (this->is_a_valid_valve(valve_number)) {
    this->valve_[valve_number].valve_switch.set_off_switch(valve_switch_off);
    this->valve_[valve_number].valve_switch.set_on_switch(valve_switch_on);
    this->valve_[valve_number].valve_switch.set_pulse_duration(pulse_duration);
    this->valve_[valve_number].run_duration = run_duration;
  }
}

void Sprinkler::configure_valve_pump_switch(size_t valve_number, switch_::Switch *pump_switch) {
  if (this->is_a_valid_valve(valve_number)) {
    for (size_t i = 0; i < this->pump_.size(); i++) {      // check each existing registered pump
      if (this->pump_[i].on_switch() == pump_switch) {     // if the "new" pump matches one we already have...
        this->valve_[valve_number].pump_switch_index = i;  // ...save its index in the SprinklerSwitch vector pump_...
        return;                                            // ...and we are done
      }
    }  // if we end up here, no pumps matched, so add a new one and set the valve's SprinklerSwitch at it
    this->pump_.resize(this->pump_.size() + 1);
    this->pump_.back().set_on_switch(pump_switch);
    this->valve_[valve_number].pump_switch_index = this->pump_.size() - 1;  // save the index to the new pump
  }
}

void Sprinkler::configure_valve_pump_switch_pulsed(size_t valve_number, switch_::Switch *pump_switch_off,
                                                   switch_::Switch *pump_switch_on, uint32_t pulse_duration) {
  if (this->is_a_valid_valve(valve_number)) {
    for (size_t i = 0; i < this->pump_.size(); i++) {  // check each existing registered pump
      if ((this->pump_[i].off_switch() == pump_switch_off) &&
          (this->pump_[i].on_switch() == pump_switch_on)) {  // if the "new" pump matches one we already have...
        this->valve_[valve_number].pump_switch_index = i;    // ...save its index in the SprinklerSwitch vector pump_...
        return;                                              // ...and we are done
      }
    }  // if we end up here, no pumps matched, so add a new one and set the valve's SprinklerSwitch at it
    this->pump_.resize(this->pump_.size() + 1);
    this->pump_.back().set_off_switch(pump_switch_off);
    this->pump_.back().set_on_switch(pump_switch_on);
    this->pump_.back().set_pulse_duration(pulse_duration);
    this->valve_[valve_number].pump_switch_index = this->pump_.size() - 1;  // save the index to the new pump
  }
}

void Sprinkler::set_multiplier(const optional<float> multiplier) {
  if (multiplier.has_value()) {
    if (multiplier.value() > 0) {
      this->multiplier_ = multiplier.value();
    }
  }
}

void Sprinkler::set_pump_start_delay(uint32_t start_delay) {
  this->start_delay_is_valve_delay_ = false;
  this->start_delay_ = start_delay;
}

void Sprinkler::set_pump_stop_delay(uint32_t stop_delay) {
  this->stop_delay_is_valve_delay_ = false;
  this->stop_delay_ = stop_delay;
}

void Sprinkler::set_valve_start_delay(uint32_t start_delay) {
  this->start_delay_is_valve_delay_ = true;
  this->start_delay_ = start_delay;
}

void Sprinkler::set_valve_stop_delay(uint32_t stop_delay) {
  this->stop_delay_is_valve_delay_ = true;
  this->stop_delay_ = stop_delay;
}

void Sprinkler::set_pump_switch_off_during_valve_open_delay(bool pump_switch_off_during_valve_open_delay) {
  this->pump_switch_off_during_valve_open_delay_ = pump_switch_off_during_valve_open_delay;
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
  this->pump_switch_off_during_valve_open_delay_ = false;  // incompatible option
}

void Sprinkler::set_manual_selection_delay(uint32_t manual_selection_delay) {
  if (manual_selection_delay > 0) {
    this->manual_selection_delay_ = manual_selection_delay;
  } else {
    this->manual_selection_delay_.reset();
  }
}

void Sprinkler::set_valve_run_duration(const optional<size_t> valve_number, const optional<uint32_t> run_duration) {
  if (valve_number.has_value() && run_duration.has_value()) {
    if (this->is_a_valid_valve(valve_number.value())) {
      this->valve_[valve_number.value()].run_duration = run_duration.value();
    }
  }
}

void Sprinkler::set_auto_advance(const bool auto_advance) {
  if (this->auto_adv_sw_ != nullptr) {
    this->auto_adv_sw_->publish_state(auto_advance);
  }
}

void Sprinkler::set_repeat(optional<uint32_t> repeat) { this->target_repeats_ = repeat; }

void Sprinkler::set_queue_enable(bool queue_enable) {
  if (this->queue_enable_sw_ != nullptr) {
    this->queue_enable_sw_->publish_state(queue_enable);
  }
}

void Sprinkler::set_reverse(const bool reverse) {
  if (this->reverse_sw_ != nullptr) {
    this->reverse_sw_->publish_state(reverse);
  }
}

uint32_t Sprinkler::valve_run_duration(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].run_duration;
  }
  return 0;
}

uint32_t Sprinkler::valve_run_duration_adjusted(const size_t valve_number) {
  uint32_t run_duration = 0;

  if (this->is_a_valid_valve(valve_number)) {
    run_duration = this->valve_[valve_number].run_duration;
  }
  run_duration = static_cast<uint32_t>(roundf(run_duration * this->multiplier_));
  // run_duration must not be less than any of these
  if ((run_duration < this->start_delay_) || (run_duration < this->stop_delay_) ||
      (run_duration < this->switching_delay_.value_or(0) * 2)) {
    return std::max(this->switching_delay_.value_or(0) * 2, std::max(this->start_delay_, this->stop_delay_));
  }
  return run_duration;
}

bool Sprinkler::auto_advance() {
  if (this->auto_adv_sw_ != nullptr) {
    return this->auto_adv_sw_->state;
  }
  return false;
}

float Sprinkler::multiplier() { return this->multiplier_; }

optional<uint32_t> Sprinkler::repeat() { return this->target_repeats_; }

optional<uint32_t> Sprinkler::repeat_count() {
  // if there is an active valve and auto-advance is enabled, we may be repeating, so return the count
  if (this->auto_adv_sw_ != nullptr) {
    if (this->active_req_.has_request() && this->auto_adv_sw_->state) {
      return this->repeat_count_;
    }
  }
  return nullopt;
}

bool Sprinkler::queue_enabled() {
  if (this->queue_enable_sw_ != nullptr) {
    return this->queue_enable_sw_->state;
  }
  return true;
}

bool Sprinkler::reverse() {
  if (this->reverse_sw_ != nullptr) {
    return this->reverse_sw_->state;
  }
  return false;
}

void Sprinkler::start_from_queue() {
  if (this->queued_valves_.empty()) {
    return;  // if there is nothing in the queue, don't do anything
  }
  if (this->queue_enabled() && this->active_valve().has_value()) {
    return;  // if there is already a valve running from the queue, do nothing
  }

  if (this->auto_adv_sw_ != nullptr) {
    this->auto_adv_sw_->publish_state(false);
  }
  if (this->queue_enable_sw_ != nullptr) {
    this->queue_enable_sw_->publish_state(true);
  }
  this->reset_cycle_states_();  // just in case auto-advance is switched on later
  this->repeat_count_ = 0;
  this->fsm_kick_();  // will automagically pick up from the queue (it has priority)
}

void Sprinkler::start_full_cycle() {
  if (this->auto_advance() && this->active_valve().has_value()) {
    return;  // if auto-advance is already enabled and there is already a valve running, do nothing
  }

  if (this->queue_enable_sw_ != nullptr) {
    this->queue_enable_sw_->publish_state(false);
  }
  this->prep_full_cycle_();
  this->repeat_count_ = 0;
  // if there is no active valve already, start the first valve in the cycle
  if (!this->active_req_.has_request()) {
    this->fsm_kick_();
  }
}

void Sprinkler::start_single_valve(const optional<size_t> valve_number) {
  if (!valve_number.has_value() || (valve_number == this->active_valve())) {
    return;
  }

  if (this->auto_adv_sw_ != nullptr) {
    this->auto_adv_sw_->publish_state(false);
  }
  if (this->queue_enable_sw_ != nullptr) {
    this->queue_enable_sw_->publish_state(false);
  }
  this->reset_cycle_states_();  // just in case auto-advance is switched on later
  this->repeat_count_ = 0;
  this->fsm_request_(valve_number.value());
}

void Sprinkler::queue_valve(optional<size_t> valve_number, optional<uint32_t> run_duration) {
  if (valve_number.has_value()) {
    if (this->is_a_valid_valve(valve_number.value()) && (this->queued_valves_.size() < this->max_queue_size_)) {
      SprinklerQueueItem item{valve_number.value(), run_duration.value()};
      this->queued_valves_.insert(this->queued_valves_.begin(), item);
      ESP_LOGD(TAG, "Valve %u placed into queue with run duration of %u seconds", valve_number.value_or(0),
               run_duration.value_or(0));
    }
  }
}

void Sprinkler::clear_queued_valves() {
  this->queued_valves_.clear();
  ESP_LOGD(TAG, "Queue cleared");
}

void Sprinkler::next_valve() {
  if (this->state_ == IDLE) {
    this->reset_cycle_states_();  // just in case auto-advance is switched on later
  }
  this->manual_valve_ = this->next_valve_number_(
      this->manual_valve_.value_or(this->active_req_.valve_as_opt().value_or(this->number_of_valves() - 1)));
  if (this->manual_selection_delay_.has_value()) {
    this->set_timer_duration_(sprinkler::TIMER_VALVE_SELECTION, this->manual_selection_delay_.value());
    this->start_timer_(sprinkler::TIMER_VALVE_SELECTION);
  } else {
    this->fsm_request_(this->manual_valve_.value());
  }
}

void Sprinkler::previous_valve() {
  if (this->state_ == IDLE) {
    this->reset_cycle_states_();  // just in case auto-advance is switched on later
  }
  this->manual_valve_ =
      this->previous_valve_number_(this->manual_valve_.value_or(this->active_req_.valve_as_opt().value_or(0)));
  if (this->manual_selection_delay_.has_value()) {
    this->set_timer_duration_(sprinkler::TIMER_VALVE_SELECTION, this->manual_selection_delay_.value());
    this->start_timer_(sprinkler::TIMER_VALVE_SELECTION);
  } else {
    this->fsm_request_(this->manual_valve_.value());
  }
}

void Sprinkler::shutdown(bool clear_queue) {
  this->cancel_timer_(sprinkler::TIMER_VALVE_SELECTION);
  this->active_req_.reset();
  this->manual_valve_.reset();
  this->next_req_.reset();
  for (auto &vo : this->valve_op_) {
    vo.stop();
  }
  this->fsm_transition_to_shutdown_();
  if (clear_queue) {
    this->clear_queued_valves();
    this->repeat_count_ = 0;
  }
}

void Sprinkler::pause() {
  if (this->paused_valve_.has_value() || !this->active_req_.has_request()) {
    return;  // we can't pause if we're already paused or if there is no active valve
  }
  this->paused_valve_ = this->active_valve();
  this->resume_duration_ = this->time_remaining();
  this->shutdown(false);
  ESP_LOGD(TAG, "Paused valve %u with %u seconds remaining", this->paused_valve_.value_or(0),
           this->resume_duration_.value_or(0));
}

void Sprinkler::resume() {
  if (this->paused_valve_.has_value() && (this->resume_duration_.has_value())) {
    ESP_LOGD(TAG, "Resuming valve %u with %u seconds remaining", this->paused_valve_.value_or(0),
             this->resume_duration_.value_or(0));
    this->fsm_request_(this->paused_valve_.value(), this->resume_duration_.value());
    this->reset_resume();
  } else {
    ESP_LOGD(TAG, "No valve to resume!");
  }
}

void Sprinkler::resume_or_start_full_cycle() {
  if (this->paused_valve_.has_value() && (this->resume_duration_.has_value())) {
    this->resume();
  } else {
    this->start_full_cycle();
  }
}

void Sprinkler::reset_resume() {
  this->paused_valve_.reset();
  this->resume_duration_.reset();
}

const char *Sprinkler::valve_name(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].controller_switch->get_name().c_str();
  }
  return nullptr;
}

optional<size_t> Sprinkler::active_valve() { return this->active_req_.valve_as_opt(); }
optional<size_t> Sprinkler::paused_valve() { return this->paused_valve_; }

optional<size_t> Sprinkler::queued_valve() {
  if (!this->queued_valves_.empty()) {
    return this->queued_valves_.back().valve_number;
  }
  return nullopt;
}

optional<size_t> Sprinkler::manual_valve() { return this->manual_valve_; }

size_t Sprinkler::number_of_valves() { return this->valve_.size(); }

bool Sprinkler::is_a_valid_valve(const size_t valve_number) {
  return ((valve_number >= 0) && (valve_number < this->number_of_valves()));
}

bool Sprinkler::pump_in_use(SprinklerSwitch *pump_switch) {
  if (pump_switch == nullptr) {
    return false;  // we can't do anything if there's nothing to check
  }
  // a pump must be considered "in use" if a (distribution) valve it supplies is active. this means:
  //  - at least one SprinklerValveOperator:
  //    - has a valve loaded that depends on this pump
  //    - is in a state that depends on the pump: (ACTIVE and _possibly_ STARTING/STOPPING)
  //  - if NO SprinklerValveOperator is active but there is a run request pending (active_req_.has_request()) and the
  //     controller state is STARTING, valve open delay is configured but NOT pump_switch_off_during_valve_open_delay_
  for (auto &vo : this->valve_op_) {  // first, check if any SprinklerValveOperator has a valve dependent on this pump
    if ((vo.state() != BYPASS) && (vo.pump_switch() != nullptr)) {
      // the SprinklerValveOperator is configured with a pump; now check if it is the pump of interest
      if ((vo.pump_switch()->off_switch() == pump_switch->off_switch()) &&
          (vo.pump_switch()->on_switch() == pump_switch->on_switch())) {
        // now if the SprinklerValveOperator has a pump and it is either ACTIVE, is STARTING with a valve delay or
        // is
        //  STOPPING with a valve delay, its pump can be considered "in use", so just return indicating this now
        if ((vo.state() == ACTIVE) ||
            ((vo.state() == STARTING) && this->start_delay_ && this->start_delay_is_valve_delay_) ||
            ((vo.state() == STOPPING) && this->stop_delay_ && this->stop_delay_is_valve_delay_)) {
          return true;
        }
      }
    }
  }  // if we end up here, no SprinklerValveOperator was in a "give-away" state indicating that the pump is in use...
  if (!this->valve_overlap_ && !this->pump_switch_off_during_valve_open_delay_ && this->switching_delay_.has_value() &&
      this->active_req_.has_request() && (this->state_ != STOPPING)) {
    // ...the controller is configured to keep the pump on during a valve open delay, so just return
    //  whether or not the next valve shares the same pump
    return (pump_switch->off_switch() == this->valve_pump_switch(this->active_req_.valve())->off_switch()) &&
           (pump_switch->on_switch() == this->valve_pump_switch(this->active_req_.valve())->on_switch());
  }
  return false;
}

void Sprinkler::set_pump_state(SprinklerSwitch *pump_switch, bool state) {
  if (pump_switch == nullptr) {
    return;  // we can't do anything if there's nothing to check
  }

  bool hold_pump_on = false;

  for (auto &controller : this->other_controllers_) {  // check if the pump is in use by another controller
    if (controller != this) {                          // dummy check
      if (controller->pump_in_use(pump_switch)) {
        hold_pump_on = true;  // if another controller says it's using this pump, keep it on
                              // at this point we know if there exists another SprinklerSwitch that is "on" with its
                              //  off_switch_ and on_switch_ pointers pointing to the same pair of switch objects
      }
    }
  }
  if (hold_pump_on) {
    // at this point we know if there exists another SprinklerSwitch that is "on" with its
    //  off_switch_ and on_switch_ pointers pointing to the same pair of switch objects...
    pump_switch->sync_valve_state(true);  // ...so ensure our state is consistent
    ESP_LOGD(TAG, "Leaving pump on because another controller instance is using it");
  }

  if (state) {  // ...and now we can set the new state of the switch
    pump_switch->turn_on();
  } else if (!hold_pump_on && !this->pump_in_use(pump_switch)) {
    pump_switch->turn_off();
  } else if (hold_pump_on) {               // we must assume the other controller will switch off the pump when done...
    pump_switch->sync_valve_state(false);  // ...this only impacts latching valves
  }
}

optional<uint32_t> Sprinkler::time_remaining() {
  if (this->active_req_.has_request()) {  // first try to return the value based on active_req_...
    if (this->active_req_.valve_operator() != nullptr) {
      return this->active_req_.valve_operator()->time_remaining();
    }
  }
  for (auto &vo : this->valve_op_) {  // ...else return the value from the first non-IDLE SprinklerValveOperator
    if (vo.state() != IDLE) {
      return vo.time_remaining();
    }
  }
  return nullopt;
}

SprinklerControllerSwitch *Sprinkler::control_switch(size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].controller_switch;
  }
  return nullptr;
}

SprinklerControllerSwitch *Sprinkler::enable_switch(size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].enable_switch;
  }
  return nullptr;
}

SprinklerSwitch *Sprinkler::valve_switch(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return &this->valve_[valve_number].valve_switch;
  }
  return nullptr;
}

SprinklerSwitch *Sprinkler::valve_pump_switch(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number) && this->valve_[valve_number].pump_switch_index.has_value()) {
    return &this->pump_[this->valve_[valve_number].pump_switch_index.value()];
  }
  return nullptr;
}

SprinklerSwitch *Sprinkler::valve_pump_switch_by_pump_index(size_t pump_index) {
  if (pump_index < this->pump_.size()) {
    return &this->pump_[pump_index];
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
    ESP_LOGD(TAG, "Marking valve %u complete", valve_number);
    this->valve_[valve_number].valve_cycle_complete = true;
  }
}

bool Sprinkler::valve_cycle_complete_(const size_t valve_number) {
  if (this->is_a_valid_valve(valve_number)) {
    return this->valve_[valve_number].valve_cycle_complete;
  }
  return false;
}

size_t Sprinkler::next_valve_number_(const size_t first_valve) {
  if (this->is_a_valid_valve(first_valve) && (first_valve + 1 < this->number_of_valves()))
    return first_valve + 1;

  return 0;
}

size_t Sprinkler::previous_valve_number_(const size_t first_valve) {
  if (this->is_a_valid_valve(first_valve) && (first_valve - 1 >= 0))
    return first_valve - 1;

  return this->number_of_valves() - 1;
}

optional<size_t> Sprinkler::next_valve_number_in_cycle_(const optional<size_t> first_valve) {
  if (this->reverse_sw_ != nullptr) {
    if (this->reverse_sw_->state) {
      return this->previous_enabled_incomplete_valve_number_(first_valve);
    }
  }
  return this->next_enabled_incomplete_valve_number_(first_valve);
}

void Sprinkler::load_next_valve_run_request_(optional<size_t> first_valve) {
  if (this->next_req_.has_request()) {
    if (!this->next_req_.run_duration()) {  // ensure the run duration is set correctly for consumption later on
      this->next_req_.set_run_duration(this->valve_run_duration_adjusted(this->next_req_.valve()));
    }
    return;  // there is already a request pending
  } else if (this->queue_enabled() && !this->queued_valves_.empty()) {
    this->next_req_.set_valve(this->queued_valves_.back().valve_number);
    if (this->queued_valves_.back().run_duration) {
      this->next_req_.set_run_duration(this->queued_valves_.back().run_duration);
    } else {
      this->next_req_.set_run_duration(this->valve_run_duration_adjusted(this->queued_valves_.back().valve_number));
    }
    this->queued_valves_.pop_back();
  } else if (this->auto_adv_sw_ != nullptr) {
    if (this->auto_adv_sw_->state) {
      if (this->next_valve_number_in_cycle_(first_valve).has_value()) {
        // if there is another valve to run as a part of a cycle, load that
        this->next_req_.set_valve(this->next_valve_number_in_cycle_(first_valve).value_or(0));
        this->next_req_.set_run_duration(
            this->valve_run_duration_adjusted(this->next_valve_number_in_cycle_(first_valve).value_or(0)));
      } else if ((this->repeat_count_++ < this->target_repeats_.value_or(0))) {
        ESP_LOGD(TAG, "Repeating - starting cycle %u of %u", this->repeat_count_ + 1,
                 this->target_repeats_.value_or(0) + 1);
        // if there are repeats remaining and no more valves were left in the cycle, start a new cycle
        this->prep_full_cycle_();
        this->next_req_.set_valve(this->next_valve_number_in_cycle_(first_valve).value_or(0));
        this->next_req_.set_run_duration(
            this->valve_run_duration_adjusted(this->next_valve_number_in_cycle_(first_valve).value_or(0)));
      }
    }
  }
}

optional<size_t> Sprinkler::next_enabled_incomplete_valve_number_(const optional<size_t> first_valve) {
  auto new_valve_number = this->next_valve_number_(first_valve.value_or(this->number_of_valves() - 1));

  while (new_valve_number != first_valve.value_or(this->number_of_valves() - 1)) {
    if (this->valve_is_enabled_(new_valve_number) && (!this->valve_cycle_complete_(new_valve_number))) {
      return new_valve_number;
    } else {
      new_valve_number = this->next_valve_number_(new_valve_number);
    }
  }
  return nullopt;
}

optional<size_t> Sprinkler::previous_enabled_incomplete_valve_number_(const optional<size_t> first_valve) {
  auto new_valve_number = this->previous_valve_number_(first_valve.value_or(0));

  while (new_valve_number != first_valve.value_or(0)) {
    if (this->valve_is_enabled_(new_valve_number) && (!this->valve_cycle_complete_(new_valve_number))) {
      return new_valve_number;
    } else {
      new_valve_number = this->previous_valve_number_(new_valve_number);
    }
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

void Sprinkler::start_valve_(SprinklerValveRunRequest *req) {
  if (!req->has_request()) {
    return;  // we can't do anything if the request contains nothing
  }
  if (!this->is_a_valid_valve(req->valve())) {
    return;  // we can't do anything if the valve number isn't valid
  }
  for (auto &vo : this->valve_op_) {  // find the first available SprinklerValveOperator, load it and start it up
    if (vo.state() == IDLE) {
      auto run_duration = req->run_duration() ? req->run_duration() : this->valve_run_duration_adjusted(req->valve());
      ESP_LOGD(TAG, "Starting valve %u for %u seconds, cycle %u of %u", req->valve(), run_duration,
               this->repeat_count_ + 1, this->target_repeats_.value_or(0) + 1);
      req->set_valve_operator(&vo);
      vo.set_controller(this);
      vo.set_valve(&this->valve_[req->valve()]);
      vo.set_run_duration(run_duration);
      vo.set_start_delay(this->start_delay_, this->start_delay_is_valve_delay_);
      vo.set_stop_delay(this->stop_delay_, this->stop_delay_is_valve_delay_);
      vo.start();
      return;
    }
  }
}

void Sprinkler::all_valves_off_(const bool include_pump) {
  for (size_t valve_index = 0; valve_index < this->number_of_valves(); valve_index++) {
    if (this->valve_[valve_index].valve_switch.state()) {
      this->valve_[valve_index].valve_switch.turn_off();
    }
    if (include_pump) {
      this->set_pump_state(this->valve_pump_switch(valve_index), false);
    }
  }
  ESP_LOGD(TAG, "All valves stopped%s", include_pump ? ", including pumps" : "");
}

void Sprinkler::prep_full_cycle_() {
  if (this->auto_adv_sw_ != nullptr) {
    if (!this->auto_adv_sw_->state) {
      this->auto_adv_sw_->publish_state(true);
    }
  }
  if (!this->any_valve_is_enabled_()) {
    for (auto &valve : this->valve_) {
      if (valve.enable_switch != nullptr) {
        valve.enable_switch->publish_state(true);
      }
    }
  }
  this->reset_cycle_states_();
}

void Sprinkler::reset_cycle_states_() {
  for (auto &valve : this->valve_) {
    valve.valve_cycle_complete = false;
  }
}

void Sprinkler::fsm_request_(size_t requested_valve, uint32_t requested_run_duration) {
  this->next_req_.set_valve(requested_valve);
  this->next_req_.set_run_duration(requested_run_duration);
  // if state is IDLE or ACTIVE, call fsm_transition_() to start it immediately;
  //  otherwise, fsm_transition() will pick up next_req_ at the next appropriate transition
  this->fsm_kick_();
}

void Sprinkler::fsm_kick_() {
  if ((this->state_ == IDLE) || (this->state_ == ACTIVE)) {
    this->fsm_transition_();
  }
}

void Sprinkler::fsm_transition_() {
  ESP_LOGVV(TAG, "fsm_transition_ called; state is %s", this->state_as_str_(this->state_).c_str());
  switch (this->state_) {
    case IDLE:  // the system was off -> start it up
      // advances to ACTIVE
      this->fsm_transition_from_shutdown_();
      break;

    case ACTIVE:
      // advances to STOPPING or ACTIVE (again)
      this->fsm_transition_from_valve_run_();
      break;

    case STARTING: {
      // follows valve open delay interval
      this->set_timer_duration_(sprinkler::TIMER_SM,
                                this->active_req_.run_duration() - this->switching_delay_.value_or(0));
      this->start_timer_(sprinkler::TIMER_SM);
      this->start_valve_(&this->active_req_);
      this->state_ = ACTIVE;
      if (this->next_req_.has_request()) {
        // another valve has been requested, so restart the timer so we pick it up quickly
        this->set_timer_duration_(sprinkler::TIMER_SM, this->manual_selection_delay_.value_or(1));
        this->start_timer_(sprinkler::TIMER_SM);
      }
      break;
    }

    case STOPPING:
      // stop_delay_ has elapsed so just shut everything off
      this->active_req_.reset();
      this->manual_valve_.reset();
      this->all_valves_off_(true);
      this->state_ = IDLE;
      break;

    default:
      break;
  }
  if (this->next_req_.has_request() && (this->state_ == IDLE)) {
    // another valve has been requested, so restart the timer so we pick it up quickly
    this->set_timer_duration_(sprinkler::TIMER_SM, this->manual_selection_delay_.value_or(1));
    this->start_timer_(sprinkler::TIMER_SM);
  }
  ESP_LOGVV(TAG, "fsm_transition_ complete; new state is %s", this->state_as_str_(this->state_).c_str());
}

void Sprinkler::fsm_transition_from_shutdown_() {
  this->load_next_valve_run_request_();
  this->active_req_.set_valve(this->next_req_.valve());
  this->active_req_.set_run_duration(this->next_req_.run_duration());
  this->next_req_.reset();

  this->set_timer_duration_(sprinkler::TIMER_SM, this->active_req_.run_duration() - this->switching_delay_.value_or(0));
  this->start_timer_(sprinkler::TIMER_SM);
  this->start_valve_(&this->active_req_);
  this->state_ = ACTIVE;
}

void Sprinkler::fsm_transition_from_valve_run_() {
  if (!this->active_req_.has_request()) {  // dummy check...
    this->fsm_transition_to_shutdown_();
    return;
  }

  if (!this->timer_active_(sprinkler::TIMER_SM)) {  // only flag the valve as "complete" if the timer finished
    this->mark_valve_cycle_complete_(this->active_req_.valve());
  } else {
    ESP_LOGD(TAG, "Valve cycle interrupted - NOT flagging valve as complete and stopping current valve");
    for (auto &vo : this->valve_op_) {
      vo.stop();
    }
  }

  this->load_next_valve_run_request_(this->active_req_.valve());

  if (this->next_req_.has_request()) {  // there is another valve to run...
    bool same_pump =
        this->valve_pump_switch(this->active_req_.valve()) == this->valve_pump_switch(this->next_req_.valve());

    this->active_req_.set_valve(this->next_req_.valve());
    this->active_req_.set_run_duration(this->next_req_.run_duration());
    this->next_req_.reset();

    // this->state_ = ACTIVE;  // state isn't changing
    if (this->valve_overlap_ || !this->switching_delay_.has_value()) {
      this->set_timer_duration_(sprinkler::TIMER_SM,
                                this->active_req_.run_duration() - this->switching_delay_.value_or(0));
      this->start_timer_(sprinkler::TIMER_SM);
      this->start_valve_(&this->active_req_);
    } else {
      this->set_timer_duration_(
          sprinkler::TIMER_SM,
          this->switching_delay_.value() * 2 +
              (this->pump_switch_off_during_valve_open_delay_ && same_pump ? this->stop_delay_ : 0));
      this->start_timer_(sprinkler::TIMER_SM);
      this->state_ = STARTING;
    }
  } else {  // there is NOT another valve to run...
    this->fsm_transition_to_shutdown_();
  }
}

void Sprinkler::fsm_transition_to_shutdown_() {
  this->state_ = STOPPING;
  this->set_timer_duration_(sprinkler::TIMER_SM,
                            this->start_delay_ + this->stop_delay_ + this->switching_delay_.value_or(0) + 1);
  this->start_timer_(sprinkler::TIMER_SM);
}

std::string Sprinkler::state_as_str_(SprinklerState state) {
  switch (state) {
    case IDLE:
      return "IDLE";

    case STARTING:
      return "STARTING";

    case ACTIVE:
      return "ACTIVE";

    case STOPPING:
      return "STOPPING";

    case BYPASS:
      return "BYPASS";

    default:
      return "UNKNOWN";
  }
}

void Sprinkler::start_timer_(const SprinklerTimerIndex timer_index) {
  if (this->timer_duration_(timer_index) > 0) {
    this->set_timeout(this->timer_[timer_index].name, this->timer_duration_(timer_index),
                      this->timer_cbf_(timer_index));
    this->timer_[timer_index].start_time = millis();
    this->timer_[timer_index].active = true;
  }
  ESP_LOGVV(TAG, "Timer %u started for %u sec", static_cast<size_t>(timer_index),
            this->timer_duration_(timer_index) / 1000);
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

void Sprinkler::valve_selection_callback_() {
  this->timer_[sprinkler::TIMER_VALVE_SELECTION].active = false;
  ESP_LOGVV(TAG, "Valve selection timer expired");
  if (this->manual_valve_.has_value()) {
    this->fsm_request_(this->manual_valve_.value());
    this->manual_valve_.reset();
  }
}

void Sprinkler::sm_timer_callback_() {
  this->timer_[sprinkler::TIMER_SM].active = false;
  ESP_LOGVV(TAG, "State machine timer expired");
  this->fsm_transition_();
}

void Sprinkler::dump_config() {
  ESP_LOGCONFIG(TAG, "Sprinkler Controller -- %s", this->name_.c_str());
  if (this->manual_selection_delay_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Manual Selection Delay: %u seconds", this->manual_selection_delay_.value_or(0));
  }
  if (this->target_repeats_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Repeat Cycles: %u times", this->target_repeats_.value_or(0));
  }
  if (this->start_delay_) {
    if (this->start_delay_is_valve_delay_) {
      ESP_LOGCONFIG(TAG, "  Pump Start Valve Delay: %u seconds", this->start_delay_);
    } else {
      ESP_LOGCONFIG(TAG, "  Pump Start Pump Delay: %u seconds", this->start_delay_);
    }
  }
  if (this->stop_delay_) {
    if (this->stop_delay_is_valve_delay_) {
      ESP_LOGCONFIG(TAG, "  Pump Stop Valve Delay: %u seconds", this->stop_delay_);
    } else {
      ESP_LOGCONFIG(TAG, "  Pump Stop Pump Delay: %u seconds", this->stop_delay_);
    }
  }
  if (this->switching_delay_.has_value()) {
    if (this->valve_overlap_) {
      ESP_LOGCONFIG(TAG, "  Valve Overlap: %u seconds", this->switching_delay_.value_or(0));
    } else {
      ESP_LOGCONFIG(TAG, "  Valve Open Delay: %u seconds", this->switching_delay_.value_or(0));
      ESP_LOGCONFIG(TAG, "  Pump Switch Off During Valve Open Delay: %s",
                    YESNO(this->pump_switch_off_during_valve_open_delay_));
    }
  }
  for (size_t valve_number = 0; valve_number < this->number_of_valves(); valve_number++) {
    ESP_LOGCONFIG(TAG, "  Valve %u:", valve_number);
    ESP_LOGCONFIG(TAG, "    Name: %s", this->valve_name(valve_number));
    ESP_LOGCONFIG(TAG, "    Run Duration: %u seconds", this->valve_[valve_number].run_duration);
    if (this->valve_[valve_number].valve_switch.pulse_duration()) {
      ESP_LOGCONFIG(TAG, "    Pulse Duration: %u milliseconds",
                    this->valve_[valve_number].valve_switch.pulse_duration());
    }
  }
  if (!this->pump_.empty()) {
    ESP_LOGCONFIG(TAG, "  Total number of pumps: %u", this->pump_.size());
  }
  if (!this->valve_.empty()) {
    ESP_LOGCONFIG(TAG, "  Total number of valves: %u", this->valve_.size());
  }
}

}  // namespace sprinkler
}  // namespace esphome
