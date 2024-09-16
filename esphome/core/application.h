#pragma once

#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/scheduler.h"

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

class Application {
 public:
  void pre_setup(const std::string &name, const std::string &friendly_name, const std::string &area,
                 const char *comment, const char *compilation_time, bool name_add_mac_suffix) {
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
    this->area_ = area;
    this->comment_ = comment;
    this->compilation_time_ = compilation_time;
  }

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
  const std::string &get_area() const { return this->area_; }

  /// Get the comment of this Application set by pre_setup().
  std::string get_comment() const { return this->comment_; }

  bool is_name_add_mac_suffix_enabled() const { return this->name_add_mac_suffix_; }

  std::string get_compilation_time() const { return this->compilation_time_; }

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
   * @param loop_interval The interval in milliseconds to run the core loop at. Defaults to 16 milliseconds.
   */
  void set_loop_interval(uint32_t loop_interval) { this->loop_interval_ = loop_interval; }

  uint32_t get_loop_interval() const { return this->loop_interval_; }

  void schedule_dump_config() { this->dump_config_at_ = 0; }

  void feed_wdt();

  void reboot();

  void safe_reboot();

  void run_safe_shutdown_hooks();

  uint32_t get_app_state() const { return this->app_state_; }

#ifdef USE_BINARY_SENSOR
  const std::vector<binary_sensor::BinarySensor *> &get_binary_sensors() { return this->binary_sensors_; }
  binary_sensor::BinarySensor *get_binary_sensor_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->binary_sensors_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_SWITCH
  const std::vector<switch_::Switch *> &get_switches() { return this->switches_; }
  switch_::Switch *get_switch_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->switches_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_BUTTON
  const std::vector<button::Button *> &get_buttons() { return this->buttons_; }
  button::Button *get_button_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->buttons_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_SENSOR
  const std::vector<sensor::Sensor *> &get_sensors() { return this->sensors_; }
  sensor::Sensor *get_sensor_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->sensors_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_TEXT_SENSOR
  const std::vector<text_sensor::TextSensor *> &get_text_sensors() { return this->text_sensors_; }
  text_sensor::TextSensor *get_text_sensor_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->text_sensors_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_FAN
  const std::vector<fan::Fan *> &get_fans() { return this->fans_; }
  fan::Fan *get_fan_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->fans_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_COVER
  const std::vector<cover::Cover *> &get_covers() { return this->covers_; }
  cover::Cover *get_cover_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->covers_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_LIGHT
  const std::vector<light::LightState *> &get_lights() { return this->lights_; }
  light::LightState *get_light_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->lights_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_CLIMATE
  const std::vector<climate::Climate *> &get_climates() { return this->climates_; }
  climate::Climate *get_climate_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->climates_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_NUMBER
  const std::vector<number::Number *> &get_numbers() { return this->numbers_; }
  number::Number *get_number_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->numbers_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_DATETIME_DATE
  const std::vector<datetime::DateEntity *> &get_dates() { return this->dates_; }
  datetime::DateEntity *get_date_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->dates_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_DATETIME_TIME
  const std::vector<datetime::TimeEntity *> &get_times() { return this->times_; }
  datetime::TimeEntity *get_time_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->times_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_DATETIME_DATETIME
  const std::vector<datetime::DateTimeEntity *> &get_datetimes() { return this->datetimes_; }
  datetime::DateTimeEntity *get_datetime_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->datetimes_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_TEXT
  const std::vector<text::Text *> &get_texts() { return this->texts_; }
  text::Text *get_text_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->texts_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_SELECT
  const std::vector<select::Select *> &get_selects() { return this->selects_; }
  select::Select *get_select_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->selects_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_LOCK
  const std::vector<lock::Lock *> &get_locks() { return this->locks_; }
  lock::Lock *get_lock_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->locks_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_VALVE
  const std::vector<valve::Valve *> &get_valves() { return this->valves_; }
  valve::Valve *get_valve_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->valves_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif
#ifdef USE_MEDIA_PLAYER
  const std::vector<media_player::MediaPlayer *> &get_media_players() { return this->media_players_; }
  media_player::MediaPlayer *get_media_player_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->media_players_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  const std::vector<alarm_control_panel::AlarmControlPanel *> &get_alarm_control_panels() {
    return this->alarm_control_panels_;
  }
  alarm_control_panel::AlarmControlPanel *get_alarm_control_panel_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->alarm_control_panels_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif

#ifdef USE_EVENT
  const std::vector<event::Event *> &get_events() { return this->events_; }
  event::Event *get_event_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->events_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif

#ifdef USE_UPDATE
  const std::vector<update::UpdateEntity *> &get_updates() { return this->updates_; }
  update::UpdateEntity *get_update_by_key(uint32_t key, bool include_internal = false) {
    for (auto *obj : this->updates_) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal()))
        return obj;
    }
    return nullptr;
  }
#endif

  Scheduler scheduler;

 protected:
  friend Component;

  void register_component_(Component *comp);

  void calculate_looping_components_();

  void feed_wdt_arch_();

  std::vector<Component *> components_{};
  std::vector<Component *> looping_components_{};

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

  std::string name_;
  std::string friendly_name_;
  std::string area_;
  const char *comment_{nullptr};
  const char *compilation_time_{nullptr};
  bool name_add_mac_suffix_;
  uint32_t last_loop_{0};
  uint32_t loop_interval_{16};
  size_t dump_config_at_{SIZE_MAX};
  uint32_t app_state_{0};
};

/// Global storage of Application pointer - only one Application can exist.
extern Application App;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome
