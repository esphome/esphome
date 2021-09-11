#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace sprinkler {

enum SprinklerTimerIndex : size_t {
  TIMER_VALVE_OPEN_DELAY = 0,
  TIMER_VALVE_RUN_DURATION = 1,
};

struct SprinklerTimer {
  const std::string name;
  bool active;
  uint32_t time;
  uint32_t start_time;
  std::function<void()> func;
};

struct SprinklerValve {
  std::string valve_name;
  switch_::Switch *valve_switch;
  switch_::Switch *enable_switch;
  uint32_t valve_run_duration;
  bool valve_cycle_complete;
};

class Sprinkler : public Component {
 public:
  Sprinkler();
  void setup() override;
  void dump_config() override;

  /// add a valve to the controller. valve_run_duration is time in seconds.
  void add_valve(switch_::Switch *valve_switch, uint32_t valve_run_duration, std::string valve_name = "");
  void add_valve(switch_::Switch *valve_switch, switch_::Switch *enable_switch, uint32_t valve_run_duration,
                 std::string valve_name = "");

  /// set the pointer to the pump/master valve switch object
  void set_pump_switch(switch_::Switch *pump_switch);

  /// value multiplied by configured run times -- used to extend or shorten the cycle
  void set_multiplier(float multiplier);

  /// how long the controller should wait to open/switch on the valve after it becomes active
  void set_valve_open_delay(uint32_t valve_open_delay);

  /// how long the valve should remain on/open (time in seconds)
  void set_valve_run_duration(uint8_t valve_number, uint32_t valve_run_duration);

  /// if auto_advance is true, controller will iterate through all enabled valves
  void set_auto_advance(bool auto_advance);

  /// if reverse is true, controller will iterate through all enabled valves in reverse (descending) order
  void set_reverse(bool reverse);

  /// returns valve_number's run duration. note that these times are stored in seconds
  uint32_t valve_run_duration(uint8_t valve_number);

  /// returns valve_number's run duration adjusted by multiplier_. note that these times are stored in seconds
  uint32_t valve_run_duration_adjusted(uint8_t valve_number);

  /// returns true if auto_advance is enabled
  bool auto_advance();

  /// returns true if reverse is enabled
  bool reverse();

  /// starts a full cycle of all enabled valves and enables auto_advance.
  /// if no valves are enabled, all valves will be enabled.
  void start_full_cycle();

  /// activates a single valve and disables auto_advance.
  void start_single_valve(uint8_t valve_number);

  /// advances to the next valve (numerically)
  void next_valve();

  /// advances to the previous valve (numerically)
  void previous_valve();

  /// turns off all valves, effectively shutting down the system.
  void shutdown();

  /// returns a pointer to a valve's name string object; returns nullptr if valve_number is invalid
  const char *valve_name_(uint8_t valve_number);

  /// returns the number of the valve that is currently active or 'none_active' if no valve is active
  int8_t active_valve();

  /// returns true if valve number is valid
  bool is_a_valid_valve(int8_t valve_number);

  /// returns the amount of time remaining in seconds for the active valve
  uint32_t time_remaining();

  /// value indicating that no valve is currently active
  const int8_t none_active{-1};

 protected:
  /// returns true if any valve is active/turned on
  bool there_is_an_active_valve_();

  /// returns true if valve number is enabled
  bool valve_is_enabled_(uint8_t valve_number);

  /// marks a valve's cycle as complete
  void mark_valve_cycle_complete_(uint8_t valve_number);

  /// returns true if valve's cycle is flagged as complete
  bool valve_cycle_complete_(uint8_t valve_number);

  /// returns a pointer to a valve's switch object
  switch_::Switch *valve_switch_(uint8_t valve_number);

  /// returns the number of the next/previous valve in the vector
  int8_t next_valve_number_(int8_t first_valve);
  int8_t previous_valve_number_(int8_t first_valve);

  /// returns the number of the next valve that should be activated in a full cycle.
  ///  if no valve is next (cycle is complete), returns this->none_active
  int8_t next_valve_number_in_cycle_(int8_t first_valve);

  /// returns the number of the next/previous valve that should be activated.
  ///  if no valve is next (cycle is complete), returns none_active
  int8_t next_enabled_incomplete_valve_number_(int8_t first_valve);
  int8_t previous_enabled_incomplete_valve_number_(int8_t first_valve);

  /// returns true if any valve is enabled
  bool any_valve_is_enabled_();

  /// if available, turns on the pump if pump_state is true, otherwise turns off the pump
  void set_pump_state_(bool pump_state);

  /// called after valve_switch_delay completes
  void activate_active_valve_();

  /// starts a valve with related timers, etc.
  void start_valve_(uint8_t valve_number);

  /// turns off/closes all valves, including pump if include_pump is true
  void all_valves_off_(bool include_pump = false);

  /// resets the cycle state for all valves
  void reset_cycle_states_();

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
  void valve_open_delay_callback_();
  void valve_cycle_complete_callback_();

  /// Automatically advance to the next valve after run duration elapses
  bool auto_advance_{false};

  /// Iterate through valves in reverse (descending) order when true
  bool reverse_{false};

  /// The number of the valve that is currently active
  int8_t active_valve_{this->none_active};

  /// Sprinkler valve run time multiplier value
  float multiplier_{1.0};

  /// Sprinkler master valve/pump start switch
  switch_::Switch *pump_switch_{nullptr};

  /// Sprinkler valve objects
  std::vector<SprinklerValve> valve_;

  /// Valve control timers
  std::vector<SprinklerTimer> timer_{
      {"open_delay", false, 0, 0, std::bind(&Sprinkler::valve_open_delay_callback_, this)},
      {"cycle_complete", false, 0, 0, std::bind(&Sprinkler::valve_cycle_complete_callback_, this)}};
};

}  // namespace sprinkler
}  // namespace esphome
