#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace sprinkler {

enum SprinklerState : size_t {
  // NOTE: these states are used by both SprinklerValveOperator and Sprinkler (the controller)!
  IDLE,      // system/valve is off
  STARTING,  // system/valve is starting/"half open" -- either pump or valve is on, but the remaining pump/valve is not
  ACTIVE,    // system/valve is running its cycle
  STOPPING   // system/valve is stopping/"half open" -- either pump or valve is on, but the remaining pump/valve is not
};

enum SprinklerTimerIndex : size_t {
  TIMER_SM = 0,
  TIMER_VALVE_SELECTION = 1,
};

class Sprinkler;
class SprinklerSwitch;
class SprinklerValveOperator;
template<typename... Ts> class StartSingleValveAction;
template<typename... Ts> class ShutdownAction;
template<typename... Ts> class ResumeOrStartAction;

struct SprinklerTimer {
  const std::string name;
  bool active;
  uint32_t time;
  uint32_t start_time;
  std::function<void()> func;
};

struct SprinklerValve {
  std::unique_ptr<SprinklerSwitch> controller_switch;
  std::unique_ptr<SprinklerSwitch> enable_switch;
  switch_::Switch *pump_switch;
  switch_::Switch *valve_switch;
  uint32_t valve_run_duration;
  bool valve_cycle_complete;
  std::unique_ptr<ShutdownAction<>> valve_shutdown_action;
  std::unique_ptr<StartSingleValveAction<>> valve_resumeorstart_action;
  std::unique_ptr<Automation<>> valve_turn_off_automation;
  std::unique_ptr<Automation<>> valve_turn_on_automation;
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
  switch_::Switch *pump_switch();  // returns this SprinklerValveOperator's valve's pump switch

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

class SprinklerSwitch : public switch_::Switch, public Component {
 public:
  SprinklerSwitch();

  void setup() override;
  void dump_config() override;

  void set_state_lambda(std::function<optional<bool>()> &&f);
  void set_restore_state(bool restore_state);
  Trigger<> *get_turn_on_trigger() const;
  Trigger<> *get_turn_off_trigger() const;
  void set_optimistic(bool optimistic);
  void set_assumed_state(bool assumed_state);
  void loop() override;

  float get_setup_priority() const override;

 protected:
  bool assumed_state() override;

  void write_state(bool state) override;

  optional<std::function<optional<bool>()>> f_;
  bool optimistic_{false};
  bool assumed_state_{false};
  Trigger<> *turn_on_trigger_;
  Trigger<> *turn_off_trigger_;
  Trigger<> *prev_trigger_{nullptr};
  bool restore_state_{false};
};

class Sprinkler : public Component, public EntityBase {
 public:
  Sprinkler();
  Sprinkler(const std::string &name);

  void setup() override;
  void loop() override;
  void dump_config() override;

  /// add a valve to the controller
  void add_valve(const std::string &valve_sw_name, const std::string &enable_sw_name = "");

  /// add another controller to the controller so it can check if pumps/main valves are in use
  void add_controller(Sprinkler *other_controller);

  /// configure important controller switches
  void set_controller_main_switch(SprinklerSwitch *controller_switch);
  void set_controller_auto_adv_switch(SprinklerSwitch *auto_adv_switch);
  void set_controller_reverse_switch(SprinklerSwitch *reverse_switch);

  /// configure a valve's switch object, run duration and pump switch (if provided).
  ///  valve_run_duration is time in seconds.
  void configure_valve_switch(size_t valve_number, switch_::Switch *valve_switch, uint32_t valve_run_duration,
                              switch_::Switch *pump_switch = nullptr);

  /// value multiplied by configured run times -- used to extend or shorten the cycle
  void set_multiplier(optional<float> multiplier);

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

  /// set how long the valve should remain on/open (time in seconds)
  void set_valve_run_duration(optional<size_t> valve_number, optional<uint32_t> valve_run_duration);

  /// if auto_advance is true, controller will iterate through all enabled valves
  void set_auto_advance(bool auto_advance);

  /// set the number of times to repeat a full cycle
  void set_repeat(optional<uint32_t> repeat);

  /// if reverse is true, controller will iterate through all enabled valves in reverse (descending) order
  void set_reverse(bool reverse);

  /// returns valve_number's run duration. note that these times are stored in seconds
  uint32_t valve_run_duration(size_t valve_number);

  /// returns valve_number's run duration adjusted by multiplier_. note that these times are stored in seconds
  uint32_t valve_run_duration_adjusted(size_t valve_number);

  /// returns true if auto_advance is enabled
  bool auto_advance();

  /// returns the number of times the controller is set to repeat cycles, if at all. check with 'has_value()'
  optional<uint32_t> repeat();

  /// if a cycle is active, returns the number of times the controller has repeated the cycle. check with 'has_value()'
  optional<uint32_t> repeat_count();

  /// returns true if reverse is enabled
  bool reverse();

  /// starts the controller from the first valve in the queue and disables auto_advance.
  /// if the queue is empty, does nothing.
  void start_from_queue();

  /// starts a full cycle of all enabled valves and enables auto_advance.
  /// if no valves are enabled, all valves will be enabled.
  void start_full_cycle();

  /// activates a single valve and disables auto_advance.
  void start_single_valve(optional<size_t> valve_number);

  /// sets a valve to be run after the active valve, regardless of auto-advance
  void queue_valve(optional<size_t> valve_number);

  /// clears/removes all valves from the queue
  void clear_queued_valves();

  /// advances to the next valve (numerically)
  void next_valve();

  /// advances to the previous valve (numerically)
  void previous_valve();

  /// turns off all valves, effectively shutting down the system.
  void shutdown(bool clear_queue = true);

  /// same as shutdown(), but also stores active_valve() and time_remaining() allowing resume() to continue the cycle
  void pause();

  /// resumes a cycle that was suspended using pause()
  void resume();

  /// if a cycle was suspended using pause(), resumes it. otherwise calls start_full_cycle()
  void resume_or_start_full_cycle();

  /// returns a pointer to a valve's name string object; returns nullptr if valve_number is invalid
  const char *valve_name(size_t valve_number);

  /// returns the number of the valve that is currently active, if any. check with 'has_value()'
  optional<uint8_t> active_valve();

  /// returns the number of the valve that is paused, if any. check with 'has_value()'
  optional<uint8_t> paused_valve();

  /// returns the number of the next valve in the queue, if any. check with 'has_value()'
  optional<uint8_t> queued_valve();

  /// returns the number of the valve that is manually selected, if any. check with 'has_value()'
  ///  this is set by next_valve() and previous_valve() when manual_selection_delay_ > 0
  optional<uint8_t> manual_valve();

  /// returns the number of valves the controller is configured with
  size_t number_of_valves();

  /// returns true if valve number is valid
  bool is_a_valid_valve(size_t valve_number);

  /// returns true if the pump the pointer points to is in use
  bool pump_in_use(switch_::Switch *pump_switch);

  /// turns off a pump "safely" by checking that no other valve or controller is using it before doing so
  void pump_off(switch_::Switch *pump_switch);

  /// returns the amount of time remaining in seconds for the active valve, if any. check with 'has_value()'
  optional<uint32_t> time_remaining();

  /// returns a pointer to a valve's control switch object
  SprinklerSwitch *control_switch(size_t valve_number);

  /// returns a pointer to a valve's enable switch object
  SprinklerSwitch *enable_switch(size_t valve_number);

 protected:
  uint32_t hash_base() override;

  /// returns true if valve number is enabled
  bool valve_is_enabled_(size_t valve_number);

  /// marks a valve's cycle as complete
  void mark_valve_cycle_complete_(size_t valve_number);

  /// returns true if valve's cycle is flagged as complete
  bool valve_cycle_complete_(size_t valve_number);

  /// returns a pointer to a valve's switch object
  switch_::Switch *valve_switch_(size_t valve_number);

  /// returns a pointer to a valve's pump switch object
  switch_::Switch *valve_pump_switch_(size_t valve_number);

  /// returns the number of the next/previous valve in the vector
  uint8_t next_valve_number_(uint8_t first_valve);
  uint8_t previous_valve_number_(uint8_t first_valve);

  /// returns the number of the next valve that should be activated in a full cycle.
  ///  if no valve is next (cycle is complete), returns no value (check with 'has_value()')
  optional<uint8_t> next_valve_number_in_cycle_(optional<uint8_t> first_valve = nullopt);

  /// returns the number of the next valve that should be activated, regardless of cycle or auto advance.
  ///  if no valve is next, returns no value (check with 'has_value()')
  optional<uint8_t> next_valve_number_to_run_(optional<uint8_t> first_valve = nullopt);

  /// returns the number of the next/previous valve that should be activated.
  ///  if no valve is next (cycle is complete), returns no value (check with 'has_value()')
  optional<uint8_t> next_enabled_incomplete_valve_number_(optional<uint8_t> first_valve);
  optional<uint8_t> previous_enabled_incomplete_valve_number_(optional<uint8_t> first_valve);

  /// returns true if any valve is enabled
  bool any_valve_is_enabled_();

  /// if available, turns on the pump if pump_state is true, otherwise turns off the pump
  void set_pump_state_(size_t valve_number, bool pump_state);

  /// turns on the valve if valve_state is true, otherwise turns off the valve
  void set_valve_state_(size_t valve_number, bool valve_state);

  /// loads a valve into an available SprinklerValveOperator valve_op_ and starts it (switches it on)
  /// NOTE: if run_duration is not specified, the valve's run_duration will be set based on the valve's configuration
  void start_valve_(optional<size_t> valve_number, optional<uint32_t> run_duration = nullopt);

  /// turns off/closes all valves, including pump if include_pump is true
  void all_valves_off_(bool include_pump = false);

  /// prepares for a full cycle by verifying auto-advance is on as well as one or more valve enable switches.
  void prep_full_cycle_();

  /// resets the cycle state for all valves
  void reset_cycle_states_();

  /// resets resume state
  void reset_resume_();

  /// make a request of the state machine
  void fsm_request_(optional<uint8_t> target_valve = nullopt);

  /// advance controller state, advancing to target_valve if provided
  void fsm_transition_();

  /// starts up the system from IDLE state
  void fsm_transition_from_shutdown_();

  /// transitions from ACTIVE state to ACTIVE (as in, next valve) or to a SHUTDOWN or IDLE state
  void fsm_transition_from_valve_run_();

  /// starts up the system from IDLE state
  void fsm_transition_to_shutdown_();

  /// return the current FSM state as a string
  std::string state_as_str();

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
  const uint8_t max_queue_size{100};

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

  /// The number of the valve that is currently active
  optional<uint8_t> active_valve_;

  /// The number of the manually selected valve currently selected
  optional<uint8_t> manual_valve_;

  /// The number of the valve to resume from (if paused)
  optional<uint8_t> paused_valve_;

  /// The number of the requested valve for the FSM to transition to next
  optional<uint8_t> requested_valve_;

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
  std::vector<size_t> queued_valves_;

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
  SprinklerSwitch *auto_adv_sw_{nullptr};
  SprinklerSwitch *controller_sw_{nullptr};
  SprinklerSwitch *reverse_sw_{nullptr};

  std::unique_ptr<ShutdownAction<>> sprinkler_shutdown_action_;
  std::unique_ptr<ResumeOrStartAction<>> sprinkler_resumeorstart_action_;

  std::unique_ptr<Automation<>> sprinkler_turn_off_automation_;
  std::unique_ptr<Automation<>> sprinkler_turn_on_automation_;
};

}  // namespace sprinkler
}  // namespace esphome
