#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/scheduler.h"

#ifdef USE_DEVICES
#include "esphome/core/device.h"
#endif
#ifdef USE_AREAS
#include "esphome/core/area.h"
#endif

#ifdef USE_SOCKET_SELECT_SUPPORT
#include <sys/select.h>
#endif

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_FAN
#include "esphome/components/fan/fan_state.h"
#endif
#ifdef USE_CLIMATE
#include "esphome/components/climate/climate.h"
#endif
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif
#ifdef USE_COVER
#include "esphome/components/cover/cover.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_DATETIME_DATE
#include "esphome/components/datetime/date_entity.h"
#endif
#ifdef USE_DATETIME_TIME
#include "esphome/components/datetime/time_entity.h"
#endif
#ifdef USE_DATETIME_DATETIME
#include "esphome/components/datetime/datetime_entity.h"
#endif
#ifdef USE_TEXT
#include "esphome/components/text/text.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_LOCK
#include "esphome/components/lock/lock.h"
#endif
#ifdef USE_VALVE
#include "esphome/components/valve/valve.h"
#endif
#ifdef USE_MEDIA_PLAYER
#include "esphome/components/media_player/media_player.h"
#endif
#ifdef USE_ALARM_CONTROL_PANEL
#include "esphome/components/alarm_control_panel/alarm_control_panel.h"
#endif
#ifdef USE_EVENT
#include "esphome/components/event/event.h"
#endif
#ifdef USE_UPDATE
#include "esphome/components/update/update_entity.h"
#endif

namespace esphome {

// Teardown timeout constant (in milliseconds)
// For reboots, it's more important to shut down quickly than disconnect cleanly
// since we're not entering deep sleep. The only consequence of not shutting down
// cleanly is a warning in the log.
static const uint32_t TEARDOWN_TIMEOUT_REBOOT_MS = 1000;  // 1 second for quick reboot

class Application {
 public:
  void pre_setup(const std::string &name, const std::string &friendly_name, const char *comment,
                 const char *compilation_time, bool name_add_mac_suffix) {
    arch_init();
    this->name_add_mac_suffix_ = name_add_mac_suffix;
    if (name_add_mac_suffix) {
      this->name_ = name + "-" + get_mac_address().substr(6);
      if (friendly_name.empty()) {
        this->friendly_name_ = "";
      } else {
        this->friendly_name_ = friendly_name + " " + get_mac_address().substr(6);
      }
    } else {
      this->name_ = name;
      this->friendly_name_ = friendly_name;
    }
    this->comment_ = comment;
    this->compilation_time_ = compilation_time;
  }

#ifdef USE_DEVICES
  void register_device(Device *device) { this->devices_.push_back(device); }
#endif
#ifdef USE_AREAS
  void register_area(Area *area) { this->areas_.push_back(area); }
#endif

  void set_current_component(Component *component) { this->current_component_ = component; }
  Component *get_current_component() { return this->current_component_; }

#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(binary_sensor::BinarySensor *binary_sensor) {
    this->binary_sensors_.push_back(binary_sensor);
  }
#endif

#ifdef USE_SENSOR
  void register_sensor(sensor::Sensor *sensor) { this->sensors_.push_back(sensor); }
#endif

#ifdef USE_SWITCH
  void register_switch(switch_::Switch *a_switch) { this->switches_.push_back(a_switch); }
#endif

#ifdef USE_BUTTON
  void register_button(button::Button *button) { this->buttons_.push_back(button); }
#endif

#ifdef USE_TEXT_SENSOR
  void register_text_sensor(text_sensor::TextSensor *sensor) { this->text_sensors_.push_back(sensor); }
#endif

#ifdef USE_FAN
  void register_fan(fan::Fan *state) { this->fans_.push_back(state); }
#endif

#ifdef USE_COVER
  void register_cover(cover::Cover *cover) { this->covers_.push_back(cover); }
#endif

#ifdef USE_CLIMATE
  void register_climate(climate::Climate *climate) { this->climates_.push_back(climate); }
#endif

#ifdef USE_LIGHT
  void register_light(light::LightState *light) { this->lights_.push_back(light); }
#endif

#ifdef USE_NUMBER
  void register_number(number::Number *number) { this->numbers_.push_back(number); }
#endif

#ifdef USE_DATETIME_DATE
  void register_date(datetime::DateEntity *date) { this->dates_.push_back(date); }
#endif

#ifdef USE_DATETIME_TIME
  void register_time(datetime::TimeEntity *time) { this->times_.push_back(time); }
#endif

#ifdef USE_DATETIME_DATETIME
  void register_datetime(datetime::DateTimeEntity *datetime) { this->datetimes_.push_back(datetime); }
#endif

#ifdef USE_TEXT
  void register_text(text::Text *text) { this->texts_.push_back(text); }
#endif

#ifdef USE_SELECT
  void register_select(select::Select *select) { this->selects_.push_back(select); }
#endif

#ifdef USE_LOCK
  void register_lock(lock::Lock *a_lock) { this->locks_.push_back(a_lock); }
#endif

#ifdef USE_VALVE
  void register_valve(valve::Valve *valve) { this->valves_.push_back(valve); }
#endif

#ifdef USE_MEDIA_PLAYER
  void register_media_player(media_player::MediaPlayer *media_player) { this->media_players_.push_back(media_player); }
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  void register_alarm_control_panel(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
    this->alarm_control_panels_.push_back(a_alarm_control_panel);
  }
#endif

#ifdef USE_EVENT
  void register_event(event::Event *event) { this->events_.push_back(event); }
#endif

#ifdef USE_UPDATE
  void register_update(update::UpdateEntity *update) { this->updates_.push_back(update); }
#endif

  /// Reserve space for components to avoid memory fragmentation
  void reserve_components(size_t count) { this->components_.reserve(count); }

#ifdef USE_BINARY_SENSOR
  void reserve_binary_sensor(size_t count) { this->binary_sensors_.reserve(count); }
#endif
#ifdef USE_SWITCH
  void reserve_switch(size_t count) { this->switches_.reserve(count); }
#endif
#ifdef USE_BUTTON
  void reserve_button(size_t count) { this->buttons_.reserve(count); }
#endif
#ifdef USE_SENSOR
  void reserve_sensor(size_t count) { this->sensors_.reserve(count); }
#endif
#ifdef USE_TEXT_SENSOR
  void reserve_text_sensor(size_t count) { this->text_sensors_.reserve(count); }
#endif
#ifdef USE_FAN
  void reserve_fan(size_t count) { this->fans_.reserve(count); }
#endif
#ifdef USE_COVER
  void reserve_cover(size_t count) { this->covers_.reserve(count); }
#endif
#ifdef USE_CLIMATE
  void reserve_climate(size_t count) { this->climates_.reserve(count); }
#endif
#ifdef USE_LIGHT
  void reserve_light(size_t count) { this->lights_.reserve(count); }
#endif
#ifdef USE_NUMBER
  void reserve_number(size_t count) { this->numbers_.reserve(count); }
#endif
#ifdef USE_DATETIME_DATE
  void reserve_date(size_t count) { this->dates_.reserve(count); }
#endif
#ifdef USE_DATETIME_TIME
  void reserve_time(size_t count) { this->times_.reserve(count); }
#endif
#ifdef USE_DATETIME_DATETIME
  void reserve_datetime(size_t count) { this->datetimes_.reserve(count); }
#endif
#ifdef USE_SELECT
  void reserve_select(size_t count) { this->selects_.reserve(count); }
#endif
#ifdef USE_TEXT
  void reserve_text(size_t count) { this->texts_.reserve(count); }
#endif
#ifdef USE_LOCK
  void reserve_lock(size_t count) { this->locks_.reserve(count); }
#endif
#ifdef USE_VALVE
  void reserve_valve(size_t count) { this->valves_.reserve(count); }
#endif
#ifdef USE_MEDIA_PLAYER
  void reserve_media_player(size_t count) { this->media_players_.reserve(count); }
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  void reserve_alarm_control_panel(size_t count) { this->alarm_control_panels_.reserve(count); }
#endif
#ifdef USE_EVENT
  void reserve_event(size_t count) { this->events_.reserve(count); }
#endif
#ifdef USE_UPDATE
  void reserve_update(size_t count) { this->updates_.reserve(count); }
#endif
#ifdef USE_AREAS
  void reserve_area(size_t count) { this->areas_.reserve(count); }
#endif
#ifdef USE_DEVICES
  void reserve_device(size_t count) { this->devices_.reserve(count); }
#endif

  /// Register the component in this Application instance.
  template<class C> C *register_component(C *c) {
    static_assert(std::is_base_of<Component, C>::value, "Only Component subclasses can be registered");
    this->register_component_((Component *) c);
    return c;
  }

  /// Set up all the registered components. Call this at the end of your setup() function.
  void setup();

  /// Make a loop iteration. Call this in your loop() function.
  void loop();

  /// Get the name of this Application set by pre_setup().
  const std::string &get_name() const { return this->name_; }

  /// Get the friendly name of this Application set by pre_setup().
  const std::string &get_friendly_name() const { return this->friendly_name_; }

  /// Get the area of this Application set by pre_setup().
  const char *get_area() const {
#ifdef USE_AREAS
    // If we have areas registered, return the name of the first one (which is the top-level area)
    if (!this->areas_.empty() && this->areas_[0] != nullptr) {
      return this->areas_[0]->get_name();
    }
#endif
    return "";
  }

  /// Get the comment of this Application set by pre_setup().
  std::string get_comment() const { return this->comment_; }

  bool is_name_add_mac_suffix_enabled() const { return this->name_add_mac_suffix_; }

  std::string get_compilation_time() const { return this->compilation_time_; }

  /// Get the cached time in milliseconds from when the current component started its loop execution
  inline uint32_t IRAM_ATTR HOT get_loop_component_start_time() const { return this->loop_component_start_time_; }

  /** Set the target interval with which to run the loop() calls.
   * If the loop() method takes longer than the target interval, ESPHome won't
   * sleep in loop(), but if the time spent in loop() is small than the target, ESPHome
   * will delay at the end of the App.loop() method.
   *
   * This is done to conserve power: In most use-cases, high-speed loop() calls are not required
   * and degrade power consumption.
   *
   * Each component can request a high frequency loop execution by using the HighFrequencyLoopRequester
   * helper in helpers.h
   *
   * Note: This method is not called by ESPHome core code. It is only used by lambda functions
   * in YAML configurations or by external components.
   *
   * @param loop_interval The interval in milliseconds to run the core loop at. Defaults to 16 milliseconds.
   */
  void set_loop_interval(uint32_t loop_interval) {
    this->loop_interval_ = std::min(loop_interval, static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()));
  }

  uint32_t get_loop_interval() const { return static_cast<uint32_t>(this->loop_interval_); }

  void schedule_dump_config() { this->dump_config_at_ = 0; }

  void feed_wdt(uint32_t time = 0);

  void reboot();

  void safe_reboot();

  void run_safe_shutdown_hooks();

  void run_powerdown_hooks();

  /** Teardown all components with a timeout.
   *
   * @param timeout_ms Maximum time to wait for teardown in milliseconds
   */
  void teardown_components(uint32_t timeout_ms);

  uint8_t get_app_state() const { return this->app_state_; }

// Helper macro for entity getter method declarations - reduces code duplication
// When USE_DEVICE_ID is enabled in the future, this can be conditionally compiled to add device_id parameter
#define GET_ENTITY_METHOD(entity_type, entity_name, entities_member) \
  entity_type *get_##entity_name##_by_key(uint32_t key, bool include_internal = false) { \
    for (auto *obj : this->entities_member##_) { \
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal())) \
        return obj; \
    } \
    return nullptr; \
  }

#ifdef USE_DEVICES
  const std::vector<Device *> &get_devices() { return this->devices_; }
#endif
#ifdef USE_AREAS
  const std::vector<Area *> &get_areas() { return this->areas_; }
#endif
#ifdef USE_BINARY_SENSOR
  const std::vector<binary_sensor::BinarySensor *> &get_binary_sensors() { return this->binary_sensors_; }
  GET_ENTITY_METHOD(binary_sensor::BinarySensor, binary_sensor, binary_sensors)
#endif
#ifdef USE_SWITCH
  const std::vector<switch_::Switch *> &get_switches() { return this->switches_; }
  GET_ENTITY_METHOD(switch_::Switch, switch, switches)
#endif
#ifdef USE_BUTTON
  const std::vector<button::Button *> &get_buttons() { return this->buttons_; }
  GET_ENTITY_METHOD(button::Button, button, buttons)
#endif
#ifdef USE_SENSOR
  const std::vector<sensor::Sensor *> &get_sensors() { return this->sensors_; }
  GET_ENTITY_METHOD(sensor::Sensor, sensor, sensors)
#endif
#ifdef USE_TEXT_SENSOR
  const std::vector<text_sensor::TextSensor *> &get_text_sensors() { return this->text_sensors_; }
  GET_ENTITY_METHOD(text_sensor::TextSensor, text_sensor, text_sensors)
#endif
#ifdef USE_FAN
  const std::vector<fan::Fan *> &get_fans() { return this->fans_; }
  GET_ENTITY_METHOD(fan::Fan, fan, fans)
#endif
#ifdef USE_COVER
  const std::vector<cover::Cover *> &get_covers() { return this->covers_; }
  GET_ENTITY_METHOD(cover::Cover, cover, covers)
#endif
#ifdef USE_LIGHT
  const std::vector<light::LightState *> &get_lights() { return this->lights_; }
  GET_ENTITY_METHOD(light::LightState, light, lights)
#endif
#ifdef USE_CLIMATE
  const std::vector<climate::Climate *> &get_climates() { return this->climates_; }
  GET_ENTITY_METHOD(climate::Climate, climate, climates)
#endif
#ifdef USE_NUMBER
  const std::vector<number::Number *> &get_numbers() { return this->numbers_; }
  GET_ENTITY_METHOD(number::Number, number, numbers)
#endif
#ifdef USE_DATETIME_DATE
  const std::vector<datetime::DateEntity *> &get_dates() { return this->dates_; }
  GET_ENTITY_METHOD(datetime::DateEntity, date, dates)
#endif
#ifdef USE_DATETIME_TIME
  const std::vector<datetime::TimeEntity *> &get_times() { return this->times_; }
  GET_ENTITY_METHOD(datetime::TimeEntity, time, times)
#endif
#ifdef USE_DATETIME_DATETIME
  const std::vector<datetime::DateTimeEntity *> &get_datetimes() { return this->datetimes_; }
  GET_ENTITY_METHOD(datetime::DateTimeEntity, datetime, datetimes)
#endif
#ifdef USE_TEXT
  const std::vector<text::Text *> &get_texts() { return this->texts_; }
  GET_ENTITY_METHOD(text::Text, text, texts)
#endif
#ifdef USE_SELECT
  const std::vector<select::Select *> &get_selects() { return this->selects_; }
  GET_ENTITY_METHOD(select::Select, select, selects)
#endif
#ifdef USE_LOCK
  const std::vector<lock::Lock *> &get_locks() { return this->locks_; }
  GET_ENTITY_METHOD(lock::Lock, lock, locks)
#endif
#ifdef USE_VALVE
  const std::vector<valve::Valve *> &get_valves() { return this->valves_; }
  GET_ENTITY_METHOD(valve::Valve, valve, valves)
#endif
#ifdef USE_MEDIA_PLAYER
  const std::vector<media_player::MediaPlayer *> &get_media_players() { return this->media_players_; }
  GET_ENTITY_METHOD(media_player::MediaPlayer, media_player, media_players)
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  const std::vector<alarm_control_panel::AlarmControlPanel *> &get_alarm_control_panels() {
    return this->alarm_control_panels_;
  }
  GET_ENTITY_METHOD(alarm_control_panel::AlarmControlPanel, alarm_control_panel, alarm_control_panels)
#endif

#ifdef USE_EVENT
  const std::vector<event::Event *> &get_events() { return this->events_; }
  GET_ENTITY_METHOD(event::Event, event, events)
#endif

#ifdef USE_UPDATE
  const std::vector<update::UpdateEntity *> &get_updates() { return this->updates_; }
  GET_ENTITY_METHOD(update::UpdateEntity, update, updates)
#endif

  Scheduler scheduler;

  /// Register/unregister a socket file descriptor to be monitored for read events.
#ifdef USE_SOCKET_SELECT_SUPPORT
  /// These functions update the fd_set used by select() in the main loop.
  /// WARNING: These functions are NOT thread-safe. They must only be called from the main loop.
  /// NOTE: File descriptors >= FD_SETSIZE (typically 10 on ESP) will be rejected with an error.
  /// @return true if registration was successful, false if fd exceeds limits
  bool register_socket_fd(int fd);
  void unregister_socket_fd(int fd);
  /// Check if there's data available on a socket without blocking
  /// This function is thread-safe for reading, but should be called after select() has run
  bool is_socket_ready(int fd) const;
#endif

 protected:
  friend Component;

  void register_component_(Component *comp);

  void calculate_looping_components_();

  // These methods are called by Component::disable_loop() and Component::enable_loop()
  // Components should not call these directly - use this->disable_loop() or this->enable_loop()
  // to ensure component state is properly updated along with the loop partition
  void disable_component_loop_(Component *component);
  void enable_component_loop_(Component *component);
  void enable_pending_loops_();
  void activate_looping_component_(uint16_t index);

  void feed_wdt_arch_();

  /// Perform a delay while also monitoring socket file descriptors for readiness
  void yield_with_select_(uint32_t delay_ms);

  // === Member variables ordered by size to minimize padding ===

  // Pointer-sized members first
  Component *current_component_{nullptr};
  const char *comment_{nullptr};
  const char *compilation_time_{nullptr};

  // size_t members
  size_t dump_config_at_{SIZE_MAX};

  // Vectors (largest members)
  std::vector<Component *> components_{};

  // Partitioned vector design for looping components
  // =================================================
  // Components are partitioned into [active | inactive] sections:
  //
  // looping_components_: [A, B, C, D | E, F]
  //                                  ^
  //                      looping_components_active_end_ (4)
  //
  // - Components A,B,C,D are active and will be called in loop()
  // - Components E,F are inactive (disabled/failed) and won't be called
  // - No flag checking needed during iteration - just loop 0 to active_end_
  // - When a component is disabled, it's swapped with the last active component
  //   and active_end_ is decremented
  // - When a component is enabled, it's swapped with the first inactive component
  //   and active_end_ is incremented
  // - This eliminates branch mispredictions from flag checking in the hot loop
  std::vector<Component *> looping_components_{};

#ifdef USE_DEVICES
  std::vector<Device *> devices_{};
#endif
#ifdef USE_AREAS
  std::vector<Area *> areas_{};
#endif
#ifdef USE_BINARY_SENSOR
  std::vector<binary_sensor::BinarySensor *> binary_sensors_{};
#endif
#ifdef USE_SWITCH
  std::vector<switch_::Switch *> switches_{};
#endif
#ifdef USE_BUTTON
  std::vector<button::Button *> buttons_{};
#endif
#ifdef USE_EVENT
  std::vector<event::Event *> events_{};
#endif
#ifdef USE_SENSOR
  std::vector<sensor::Sensor *> sensors_{};
#endif
#ifdef USE_TEXT_SENSOR
  std::vector<text_sensor::TextSensor *> text_sensors_{};
#endif
#ifdef USE_FAN
  std::vector<fan::Fan *> fans_{};
#endif
#ifdef USE_COVER
  std::vector<cover::Cover *> covers_{};
#endif
#ifdef USE_CLIMATE
  std::vector<climate::Climate *> climates_{};
#endif
#ifdef USE_LIGHT
  std::vector<light::LightState *> lights_{};
#endif
#ifdef USE_NUMBER
  std::vector<number::Number *> numbers_{};
#endif
#ifdef USE_DATETIME_DATE
  std::vector<datetime::DateEntity *> dates_{};
#endif
#ifdef USE_DATETIME_TIME
  std::vector<datetime::TimeEntity *> times_{};
#endif
#ifdef USE_DATETIME_DATETIME
  std::vector<datetime::DateTimeEntity *> datetimes_{};
#endif
#ifdef USE_SELECT
  std::vector<select::Select *> selects_{};
#endif
#ifdef USE_TEXT
  std::vector<text::Text *> texts_{};
#endif
#ifdef USE_LOCK
  std::vector<lock::Lock *> locks_{};
#endif
#ifdef USE_VALVE
  std::vector<valve::Valve *> valves_{};
#endif
#ifdef USE_MEDIA_PLAYER
  std::vector<media_player::MediaPlayer *> media_players_{};
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  std::vector<alarm_control_panel::AlarmControlPanel *> alarm_control_panels_{};
#endif
#ifdef USE_UPDATE
  std::vector<update::UpdateEntity *> updates_{};
#endif

#ifdef USE_SOCKET_SELECT_SUPPORT
  std::vector<int> socket_fds_;  // Vector of all monitored socket file descriptors
#endif

  // String members
  std::string name_;
  std::string friendly_name_;

  // 4-byte members
  uint32_t last_loop_{0};
  uint32_t loop_component_start_time_{0};

#ifdef USE_SOCKET_SELECT_SUPPORT
  int max_fd_{-1};  // Highest file descriptor number for select()
#endif

  // 2-byte members (grouped together for alignment)
  uint16_t loop_interval_{16};  // Loop interval in ms (max 65535ms = 65.5 seconds)
  uint16_t looping_components_active_end_{0};
  uint16_t current_loop_index_{0};  // For safe reentrant modifications during iteration

  // 1-byte members (grouped together to minimize padding)
  uint8_t app_state_{0};
  bool name_add_mac_suffix_;
  bool in_loop_{false};
  volatile bool has_pending_enable_loop_requests_{false};

#ifdef USE_SOCKET_SELECT_SUPPORT
  bool socket_fds_changed_{false};  // Flag to rebuild base_read_fds_ when socket_fds_ changes

  // Variable-sized members at end
  fd_set base_read_fds_{};  // Cached fd_set rebuilt only when socket_fds_ changes
  fd_set read_fds_{};       // Working fd_set for select(), copied from base_read_fds_
#endif
};

/// Global storage of Application pointer - only one Application can exist.
extern Application App;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome
