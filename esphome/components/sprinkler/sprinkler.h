#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"

#include <vector>

namespace esphome {
namespace sprinkler {

const std::string min_str = "min";

enum SprinklerState : uint8_t {
  // NOTE: these states are used by both SprinklerValveOperator and Sprinkler (the controller)!
  IDLE,      // system/valve is off
  STARTING,  // system/valve is starting/"half open" -- either pump or valve is on, but the remaining pump/valve is not
  ACTIVE,    // system/valve is running its cycle
  STOPPING,  // system/valve is stopping/"half open" -- either pump or valve is on, but the remaining pump/valve is not
  BYPASS     // used by SprinklerValveOperator to ignore the instance checking pump status
};

enum SprinklerTimerIndex : uint8_t {
  TIMER_SM = 0,
  TIMER_VALVE_SELECTION = 1,
};

enum SprinklerValveRunRequestOrigin : uint8_t {
  USER,
  CYCLE,
  QUEUE,
};

class Sprinkler;                  // this component
class SprinklerControllerNumber;  // number components that appear in the front end; based on number core
class SprinklerControllerSwitch;  // switches that appear in the front end; based on switch core
class SprinklerSwitch;            // switches representing any valve or pump; provides abstraction for latching valves
class SprinklerValveOperator;     // manages all switching on/off of valves and associated pumps
class SprinklerValveRunRequest;   // tells the sprinkler controller what valve to run and for how long as well as what
                                  //  SprinklerValveOperator is handling it
template<typename... Ts> class StartSingleValveAction;
template<typename... Ts> class ShutdownAction;
template<typename... Ts> class ResumeOrStartAction;

class SprinklerSwitch {
 public:
  SprinklerSwitch();
  SprinklerSwitch(switch_::Switch *sprinkler_switch);
  SprinklerSwitch(switch_::Switch *off_switch, switch_::Switch *on_switch, uint32_t pulse_duration);

  bool is_latching_valve();  // returns true if configured as a latching valve
  void loop();               // called as a part of loop(), used for latching valve pulses
  uint32_t pulse_duration() { return this->pulse_duration_; }
  bool state();  // returns the switch's current state
  void set_off_switch(switch_::Switch *off_switch) { this->off_switch_ = off_switch; }
  void set_on_switch(switch_::Switch *on_switch) { this->on_switch_ = on_switch; }
  void set_pulse_duration(uint32_t pulse_duration) { this->pulse_duration_ = pulse_duration; }
  void sync_valve_state(
      bool latch_state);  // syncs internal state to switch; if latching valve, sets state to latch_state
  void turn_off();        // sets internal flag and actuates the switch
  void turn_on();         // sets internal flag and actuates the switch
  switch_::Switch *off_switch() { return this->off_switch_; }
  switch_::Switch *on_switch() { return this->on_switch_; }

 protected:
  bool state_{false};
  uint32_t pulse_duration_{0};
  uint64_t pinned_millis_{0};
  switch_::Switch *off_switch_{nullptr};  // only used for latching valves
  switch_::Switch *on_switch_{nullptr};   // used for both latching and non-latching valves
};

struct SprinklerQueueItem {
  size_t valve_number;
  uint32_t run_duration;
};

struct SprinklerTimer {
  const std::string name;
  bool active;
  uint32_t time;
  uint32_t start_time;
  std::function<void()> func;
};

struct SprinklerValve {
  SprinklerControllerNumber *run_duration_number;
  SprinklerControllerSwitch *controller_switch;
  SprinklerControllerSwitch *enable_switch;
  SprinklerSwitch valve_switch;
  uint32_t run_duration;
  optional<size_t> pump_switch_index;
  bool valve_cycle_complete;
  std::unique_ptr<ShutdownAction<>> valve_shutdown_action;
  std::unique_ptr<StartSingleValveAction<>> valve_resumeorstart_action;
  std::unique_ptr<Automation<>> valve_turn_off_automation;
  std::unique_ptr<Automation<>> valve_turn_on_automation;
};

class SprinklerControllerNumber : public number::Number, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  Trigger<float> *get_set_trigger() const { return set_trigger_; }
  void set_initial_value(float initial_value) { initial_value_ = initial_value; }
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  void control(float value) override;
  float initial_value_{NAN};
  bool restore_value_{true};
  Trigger<float> *set_trigger_ = new Trigger<float>();

  ESPPreferenceObject pref_;
};

class SprinklerControllerSwitch : public switch_::Switch, public Component {
 public:
  SprinklerControllerSwitch();

  void setup() override;
  void dump_config() override;

  void set_state_lambda(std::function<optional<bool>()> &&f);
  Trigger<> *get_turn_on_trigger() const;
  Trigger<> *get_turn_off_trigger() const;
  void loop() override;

  float get_setup_priority() const override;

 protected:
  void write_state(bool state) override;

  optional<std::function<optional<bool>()>> f_;
  Trigger<> *turn_on_trigger_;
  Trigger<> *turn_off_trigger_;
  Trigger<> *prev_trigger_{nullptr};
};

class SprinklerValveOperator {
 public:
  SprinklerValveOperator();
  SprinklerValveOperator(SprinklerValve *valve, Sprinkler *controller);
  void loop();
  void set_controller(Sprinkler *controller);
  void set_valve(SprinklerValve *valve);
  void set_run_duration(uint32_t run_duration);  // set the desired run duration in seconds
  void set_start_delay(uint32_t start_delay, bool start_delay_is_valve_delay);
  void set_stop_delay(uint32_t stop_delay, bool stop_delay_is_valve_delay);
  void start();
  void stop();
  uint32_t run_duration();         // returns the desired run duration in seconds
  uint32_t time_remaining();       // returns seconds remaining (does not include stop_delay_)
  SprinklerState state();          // returns the valve's state/status
  SprinklerSwitch *pump_switch();  // returns this SprinklerValveOperator's pump's SprinklerSwitch

 protected:
  void pump_off_();
  void pump_on_();
  void valve_off_();
  void valve_on_();
  void kill_();
  void run_();
  bool start_delay_is_valve_delay_{false};
  bool stop_delay_is_valve_delay_{false};
  uint32_t start_delay_{0};
  uint32_t stop_delay_{0};
  uint32_t run_duration_{0};
  uint64_t pinned_millis_{0};
  Sprinkler *controller_{nullptr};
  SprinklerValve *valve_{nullptr};
  SprinklerState state_{IDLE};
};

class SprinklerValveRunRequest {
 public:
  SprinklerValveRunRequest();
  SprinklerValveRunRequest(size_t valve_number, uint32_t run_duration, SprinklerValveOperator *valve_op);
  bool has_request();
  bool has_valve_operator();
  void set_request_from(SprinklerValveRunRequestOrigin origin);
  void set_run_duration(uint32_t run_duration);
  void set_valve(size_t valve_number);
  void set_valve_operator(SprinklerValveOperator *valve_op);
  void reset();
  uint32_t run_duration();
  size_t valve();
  optional<size_t> valve_as_opt();
  SprinklerValveOperator *valve_operator();
  SprinklerValveRunRequestOrigin request_is_from();

 protected:
  bool has_valve_{false};
  size_t valve_number_{0};
  uint32_t run_duration_{0};
  SprinklerValveOperator *valve_op_{nullptr};
  SprinklerValveRunRequestOrigin origin_{USER};
};

class Sprinkler : public Component, public EntityBase {
 public:
  Sprinkler();
  Sprinkler(const std::string &name);

  void setup() override;
  void loop() override;
  void dump_config() override;

  /// add a valve to the controller
  void add_valve(SprinklerControllerSwitch *valve_sw, SprinklerControllerSwitch *enable_sw = nullptr);

  /// add another controller to the controller so it can check if pumps/main valves are in use
  void add_controller(Sprinkler *other_controller);

  /// configure important controller switches
  void set_controller_main_switch(SprinklerControllerSwitch *controller_switch);
  void set_controller_auto_adv_switch(SprinklerControllerSwitch *auto_adv_switch);
  void set_controller_queue_enable_switch(SprinklerControllerSwitch *queue_enable_switch);
  void set_controller_reverse_switch(SprinklerControllerSwitch *reverse_switch);
  void set_controller_standby_switch(SprinklerControllerSwitch *standby_switch);

  /// configure important controller number components
  void set_controller_multiplier_number(SprinklerControllerNumber *multiplier_number);
  void set_controller_repeat_number(SprinklerControllerNumber *repeat_number);

  /// configure a valve's switch object and run duration. run_duration is time in seconds.
  void configure_valve_switch(size_t valve_number, switch_::Switch *valve_switch, uint32_t run_duration);
  void configure_valve_switch_pulsed(size_t valve_number, switch_::Switch *valve_switch_off,
                                     switch_::Switch *valve_switch_on, uint32_t pulse_duration, uint32_t run_duration);

  /// configure a valve's associated pump switch object
  void configure_valve_pump_switch(size_t valve_number, switch_::Switch *pump_switch);
  void configure_valve_pump_switch_pulsed(size_t valve_number, switch_::Switch *pump_switch_off,
                                          switch_::Switch *pump_switch_on, uint32_t pulse_duration);

  /// configure a valve's run duration number component
  void configure_valve_run_duration_number(size_t valve_number, SprinklerControllerNumber *run_duration_number);

  /// sets the multiplier value to '1 / divider' and sets repeat value to divider
  void set_divider(optional<uint32_t> divider);

  /// value multiplied by configured run times -- used to extend or shorten the cycle
  void set_multiplier(optional<float> multiplier);

  /// enable/disable skipping of disabled valves by the next and previous actions
  void set_next_prev_ignore_disabled_valves(bool ignore_disabled);

  /// set how long the pump should start after the valve (when the pump is starting)
  void set_pump_start_delay(uint32_t start_delay);

  /// set how long the pump should stop after the valve (when the pump is starting)
  void set_pump_stop_delay(uint32_t stop_delay);

  /// set how long the valve should start after the pump (when the pump is stopping)
  void set_valve_start_delay(uint32_t start_delay);

  /// set how long the valve should stop after the pump (when the pump is stopping)
  void set_valve_stop_delay(uint32_t stop_delay);

  /// if pump_switch_off_during_valve_open_delay is true, the controller will switch off the pump during the
  ///  valve_open_delay interval
  void set_pump_switch_off_during_valve_open_delay(bool pump_switch_off_during_valve_open_delay);

  /// set how long the controller should wait to open/switch on the valve after it becomes active
  void set_valve_open_delay(uint32_t valve_open_delay);

  /// set how long the controller should wait after opening a valve before closing the previous valve
  void set_valve_overlap(uint32_t valve_overlap);

  /// set how long the controller should wait to activate a valve after next_valve() or previous_valve() is called
  void set_manual_selection_delay(uint32_t manual_selection_delay);

  /// set how long the valve should remain on/open. run_duration is time in seconds
  void set_valve_run_duration(optional<size_t> valve_number, optional<uint32_t> run_duration);

  /// if auto_advance is true, controller will iterate through all enabled valves
  void set_auto_advance(bool auto_advance);

  /// set the number of times to repeat a full cycle
  void set_repeat(optional<uint32_t> repeat);

  /// if queue_enable is true, controller will iterate through valves in the queue
  void set_queue_enable(bool queue_enable);

  /// if reverse is true, controller will iterate through all enabled valves in reverse (descending) order
  void set_reverse(bool reverse);

  /// if standby is true, controller will refuse to activate any valves
  void set_standby(bool standby);

  /// returns valve_number's run duration in seconds
  uint32_t valve_run_duration(size_t valve_number);

  /// returns valve_number's run duration (in seconds) adjusted by multiplier_
  uint32_t valve_run_duration_adjusted(size_t valve_number);

  /// returns true if auto_advance is enabled
  bool auto_advance();

  /// returns the current value of the multiplier
  float multiplier();

  /// returns the number of times the controller is set to repeat cycles, if at all. check with 'has_value()'
  optional<uint32_t> repeat();

  /// if a cycle is active, returns the number of times the controller has repeated the cycle. check with 'has_value()'
  optional<uint32_t> repeat_count();

  /// returns true if the queue is enabled to run
  bool queue_enabled();

  /// returns true if reverse is enabled
  bool reverse();

  /// returns true if standby is enabled
  bool standby();

  /// starts the controller from the first valve in the queue and disables auto_advance.
  /// if the queue is empty, does nothing.
  void start_from_queue();

  /// starts a full cycle of all enabled valves and enables auto_advance.
  /// if no valves are enabled, all valves will be enabled.
  void start_full_cycle();

  /// activates a single valve and disables auto_advance.
  void start_single_valve(optional<size_t> valve_number, optional<uint32_t> run_duration = nullopt);

  /// adds a valve into the queue. queued valves have priority over valves to be run as a part of a full cycle.
  /// NOTE: queued valves will always run, regardless of auto-advance and/or valve enable switches.
  void queue_valve(optional<size_t> valve_number, optional<uint32_t> run_duration);

  /// clears/removes all valves from the queue
  void clear_queued_valves();

  /// advances to the next valve (numerically)
  void next_valve();

  /// advances to the previous valve (numerically)
  void previous_valve();

  /// turns off all valves, effectively shutting down the system.
  void shutdown(bool clear_queue = false);

  /// same as shutdown(), but also stores active_valve() and time_remaining() allowing resume() to continue the cycle
  void pause();

  /// resumes a cycle that was suspended using pause()
  void resume();

  /// if a cycle was suspended using pause(), resumes it. otherwise calls start_full_cycle()
  void resume_or_start_full_cycle();

  /// resets resume state
  void reset_resume();

  /// returns a pointer to a valve's name string object; returns nullptr if valve_number is invalid
  const char *valve_name(size_t valve_number);

  /// returns what invoked the valve that is currently active, if any. check with 'has_value()'
  optional<SprinklerValveRunRequestOrigin> active_valve_request_is_from();

  /// returns the number of the valve that is currently active, if any. check with 'has_value()'
  optional<size_t> active_valve();

  /// returns the number of the valve that is paused, if any. check with 'has_value()'
  optional<size_t> paused_valve();

  /// returns the number of the next valve in the queue, if any. check with 'has_value()'
  optional<size_t> queued_valve();

  /// returns the number of the valve that is manually selected, if any. check with 'has_value()'
  ///  this is set by next_valve() and previous_valve() when manual_selection_delay_ > 0
  optional<size_t> manual_valve();

  /// returns the number of valves the controller is configured with
  size_t number_of_valves();

  /// returns true if valve number is valid
  bool is_a_valid_valve(size_t valve_number);

  /// returns true if the pump the pointer points to is in use
  bool pump_in_use(SprinklerSwitch *pump_switch);

  /// switches on/off a pump "safely" by checking that the new state will not conflict with another controller
  void set_pump_state(SprinklerSwitch *pump_switch, bool state);

  /// returns the amount of time in seconds required for all valves
  uint32_t total_cycle_time_all_valves();

  /// returns the amount of time in seconds required for all enabled valves
  uint32_t total_cycle_time_enabled_valves();

  /// returns the amount of time in seconds required for all enabled & incomplete valves, not including the active valve
  uint32_t total_cycle_time_enabled_incomplete_valves();

  /// returns the amount of time in seconds required for all valves in the queue
  uint32_t total_queue_time();

  /// returns the amount of time remaining in seconds for the active valve, if any
  optional<uint32_t> time_remaining_active_valve();

  /// returns the amount of time remaining in seconds for all valves remaining, including the active valve, if any
  optional<uint32_t> time_remaining_current_operation();

  /// returns a pointer to a valve's control switch object
  SprinklerControllerSwitch *control_switch(size_t valve_number);

  /// returns a pointer to a valve's enable switch object
  SprinklerControllerSwitch *enable_switch(size_t valve_number);

  /// returns a pointer to a valve's switch object
  SprinklerSwitch *valve_switch(size_t valve_number);

  /// returns a pointer to a valve's pump switch object
  SprinklerSwitch *valve_pump_switch(size_t valve_number);

  /// returns a pointer to a valve's pump switch object
  SprinklerSwitch *valve_pump_switch_by_pump_index(size_t pump_index);

 protected:
  uint32_t hash_base() override;

  /// returns true if valve number is enabled
  bool valve_is_enabled_(size_t valve_number);

  /// marks a valve's cycle as complete
  void mark_valve_cycle_complete_(size_t valve_number);

  /// returns true if valve's cycle is flagged as complete
  bool valve_cycle_complete_(size_t valve_number);

  /// returns the number of the next valve in the vector or nullopt if no valves match criteria
  optional<size_t> next_valve_number_(optional<size_t> first_valve = nullopt, bool include_disabled = true,
                                      bool include_complete = true);

  /// returns the number of the previous valve in the vector or nullopt if no valves match criteria
  optional<size_t> previous_valve_number_(optional<size_t> first_valve = nullopt, bool include_disabled = true,
                                          bool include_complete = true);

  /// returns the number of the next valve that should be activated in a full cycle.
  ///  if no valve is next (cycle is complete), returns no value (check with 'has_value()')
  optional<size_t> next_valve_number_in_cycle_(optional<size_t> first_valve = nullopt);

  /// loads next_req_ with the next valve that should be activated, including its run duration.
  ///  if next_req_ already contains a request, nothing is done. after next_req_,
  ///  queued valves have priority, followed by enabled valves if auto-advance is enabled.
  ///  if no valve is next (for example, a full cycle is complete), next_req_ is reset via reset().
  void load_next_valve_run_request_(optional<size_t> first_valve = nullopt);

  /// returns true if any valve is enabled
  bool any_valve_is_enabled_();

  /// loads an available SprinklerValveOperator (valve_op_) based on req and starts it (switches it on).
  /// NOTE: if run_duration is zero, the valve's run_duration will be set based on the valve's configuration.
  void start_valve_(SprinklerValveRunRequest *req);

  /// turns off/closes all valves, including pump if include_pump is true
  void all_valves_off_(bool include_pump = false);

  /// prepares for a full cycle by verifying auto-advance is on as well as one or more valve enable switches.
  void prep_full_cycle_();

  /// resets the cycle state for all valves
  void reset_cycle_states_();

  /// make a request of the state machine
  void fsm_request_(size_t requested_valve, uint32_t requested_run_duration = 0);

  /// kicks the state machine to advance, starting it if it is not already active
  void fsm_kick_();

  /// advance controller state, advancing to target_valve if provided
  void fsm_transition_();

  /// starts up the system from IDLE state
  void fsm_transition_from_shutdown_();

  /// transitions from ACTIVE state to ACTIVE (as in, next valve) or to a SHUTDOWN or IDLE state
  void fsm_transition_from_valve_run_();

  /// starts up the system from IDLE state
  void fsm_transition_to_shutdown_();

  /// return the specified SprinklerValveRunRequestOrigin as a string
  std::string req_as_str_(SprinklerValveRunRequestOrigin origin);

  /// return the specified SprinklerState state as a string
  std::string state_as_str_(SprinklerState state);

  /// Start/cancel/get status of valve timers
  void start_timer_(SprinklerTimerIndex timer_index);
  bool cancel_timer_(SprinklerTimerIndex timer_index);
  /// returns true if the specified timer is active/running
  bool timer_active_(SprinklerTimerIndex timer_index);
  /// time is converted to milliseconds (ms) for set_timeout()
  void set_timer_duration_(SprinklerTimerIndex timer_index, uint32_t time);
  /// returns time in milliseconds (ms)
  uint32_t timer_duration_(SprinklerTimerIndex timer_index);
  std::function<void()> timer_cbf_(SprinklerTimerIndex timer_index);

  /// callback functions for timers
  void valve_selection_callback_();
  void sm_timer_callback_();
  void pump_stop_delay_callback_();

  /// Maximum allowed queue size
  const uint8_t max_queue_size_{100};

  /// When set to true, the next and previous actions will skip disabled valves
  bool next_prev_ignore_disabled_{false};

  /// Pump should be off during valve_open_delay interval
  bool pump_switch_off_during_valve_open_delay_{false};

  /// Sprinkler valve cycle should overlap
  bool valve_overlap_{false};

  /// Pump start/stop delay interval types
  bool start_delay_is_valve_delay_{false};
  bool stop_delay_is_valve_delay_{false};

  /// Pump start/stop delay intervals
  uint32_t start_delay_{0};
  uint32_t stop_delay_{0};

  /// Sprinkler controller state
  SprinklerState state_{IDLE};

  /// The valve run request that is currently active
  SprinklerValveRunRequest active_req_;

  /// The number of the manually selected valve currently selected
  optional<size_t> manual_valve_;

  /// The number of the valve to resume from (if paused)
  optional<size_t> paused_valve_;

  /// The next run request for the controller to consume after active_req_ is complete
  SprinklerValveRunRequest next_req_;

  /// Set the number of times to repeat a full cycle
  optional<uint32_t> target_repeats_;

  /// Set from time_remaining() when paused
  optional<uint32_t> resume_duration_;

  /// Manual switching delay
  optional<uint32_t> manual_selection_delay_;

  /// Valve switching delay
  optional<uint32_t> switching_delay_;

  /// Number of times the full cycle has been repeated
  uint32_t repeat_count_{0};

  /// Sprinkler valve run time multiplier value
  float multiplier_{1.0};

  /// Queue of valves to activate next, regardless of auto-advance
  std::vector<SprinklerQueueItem> queued_valves_;

  /// Sprinkler valve pump objects
  std::vector<SprinklerSwitch> pump_;

  /// Sprinkler valve objects
  std::vector<SprinklerValve> valve_;

  /// Sprinkler valve operator objects
  std::vector<SprinklerValveOperator> valve_op_{2};

  /// Valve control timers
  std::vector<SprinklerTimer> timer_{
      {this->name_ + "sm", false, 0, 0, std::bind(&Sprinkler::sm_timer_callback_, this)},
      {this->name_ + "vs", false, 0, 0, std::bind(&Sprinkler::valve_selection_callback_, this)}};

  /// Other Sprinkler instances we should be aware of (used to check if pumps are in use)
  std::vector<Sprinkler *> other_controllers_;

  /// Switches we'll present to the front end
  SprinklerControllerSwitch *auto_adv_sw_{nullptr};
  SprinklerControllerSwitch *controller_sw_{nullptr};
  SprinklerControllerSwitch *queue_enable_sw_{nullptr};
  SprinklerControllerSwitch *reverse_sw_{nullptr};
  SprinklerControllerSwitch *standby_sw_{nullptr};

  /// Number components we'll present to the front end
  SprinklerControllerNumber *multiplier_number_{nullptr};
  SprinklerControllerNumber *repeat_number_{nullptr};

  std::unique_ptr<ShutdownAction<>> sprinkler_shutdown_action_;
  std::unique_ptr<ShutdownAction<>> sprinkler_standby_shutdown_action_;
  std::unique_ptr<ResumeOrStartAction<>> sprinkler_resumeorstart_action_;

  std::unique_ptr<Automation<>> sprinkler_turn_off_automation_;
  std::unique_ptr<Automation<>> sprinkler_turn_on_automation_;
  std::unique_ptr<Automation<>> sprinkler_standby_turn_on_automation_;
};

}  // namespace sprinkler
}  // namespace esphome
