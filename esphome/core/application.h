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
#include "esphome/core/entities.h"

namespace esphome {

class Application {
  template<typename Tuple> struct entityRegistry;
  template<typename... Args> struct entityRegistry<std::tuple<Args...>> {
    using type = std::tuple<std::vector<Args>...>;
  };

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

  template<typename Entity> void register_entity(Entity *entity) {
    get_by_type<std::vector<Entity *>>(entities_).push_back(entity);
  }

  template<typename Entity> const std::vector<Entity *> &get_entities() {
    return get_by_type<std::vector<Entity *>>(entities_);
  }

  template<typename Entity> Entity *get_entity_by_key(uint32_t key, bool include_internal) {
    for (auto *obj : this->get_entities<Entity>()) {
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal())) {
        return obj;
      }
    }
    return nullptr;
  }

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
  const std::vector<binary_sensor::BinarySensor *> &get_binary_sensors() {
    return this->get_entities<binary_sensor::BinarySensor>();
  }
  binary_sensor::BinarySensor *get_binary_sensor_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<binary_sensor::BinarySensor>(key, include_internal);
  }
#endif
#ifdef USE_SWITCH
  const std::vector<switch_::Switch *> &get_switches() { return this->get_entities<switch_::Switch>(); }
  switch_::Switch *get_switch_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<switch_::Switch>(key, include_internal);
  }
#endif
#ifdef USE_BUTTON
  const std::vector<button::Button *> &get_buttons() { return this->get_entities<button::Button>(); }
  button::Button *get_button_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<button::Button>(key, include_internal);
  }
#endif
#ifdef USE_SENSOR
  const std::vector<sensor::Sensor *> &get_sensors() { return this->get_entities<sensor::Sensor>(); }
  sensor::Sensor *get_sensor_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<sensor::Sensor>(key, include_internal);
  }
#endif
#ifdef USE_TEXT_SENSOR
  const std::vector<text_sensor::TextSensor *> &get_text_sensors() {
    return this->get_entities<text_sensor::TextSensor>();
  }
  text_sensor::TextSensor *get_text_sensor_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<text_sensor::TextSensor>(key, include_internal);
  }
#endif
#ifdef USE_FAN
  const std::vector<fan::Fan *> &get_fans() { return this->get_entities<fan::Fan>(); }
  fan::Fan *get_fan_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<fan::Fan>(key, include_internal);
  }
#endif
#ifdef USE_COVER
  const std::vector<cover::Cover *> &get_covers() { return this->get_entities<cover::Cover>(); }
  cover::Cover *get_cover_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<cover::Cover>(key, include_internal);
  }
#endif
#ifdef USE_LIGHT
  const std::vector<light::LightState *> &get_lights() { return this->get_entities<light::LightState>(); }
  light::LightState *get_light_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<light::LightState>(key, include_internal);
  }
#endif
#ifdef USE_CLIMATE
  const std::vector<climate::Climate *> &get_climates() { return this->get_entities<climate::Climate>(); }
  climate::Climate *get_climate_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<climate::Climate>(key, include_internal);
  }
#endif
#ifdef USE_NUMBER
  const std::vector<number::Number *> &get_numbers() { return this->get_entities<number::Number>(); }
  number::Number *get_number_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<number::Number>(key, include_internal);
  }
#endif
#ifdef USE_DATETIME_DATE
  const std::vector<datetime::DateEntity *> &get_dates() { return this->get_entities<datetime::DateEntity>(); }
  datetime::DateEntity *get_date_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<datetime::DateEntity>(key, include_internal);
  }
#endif
#ifdef USE_DATETIME_TIME
  const std::vector<datetime::TimeEntity *> &get_times() { return this->get_entities<datetime::TimeEntity>(); }
  datetime::TimeEntity *get_time_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<datetime::TimeEntity>(key, include_internal);
  }
#endif
#ifdef USE_DATETIME_DATETIME
  const std::vector<datetime::DateTimeEntity *> &get_datetimes() {
    return this->get_entities<datetime::DateTimeEntity>();
  }
  datetime::DateTimeEntity *get_datetime_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<datetime::DateTimeEntity>(key, include_internal);
  }
#endif
#ifdef USE_TEXT
  const std::vector<text::Text *> &get_texts() { return this->get_entities<text::Text>(); }
  text::Text *get_text_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<text::Text>(key, include_internal);
  }
#endif
#ifdef USE_SELECT
  const std::vector<select::Select *> &get_selects() { return this->get_entities<select::Select>(); }
  select::Select *get_select_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<select::Select>(key, include_internal);
  }
#endif
#ifdef USE_LOCK
  const std::vector<lock::Lock *> &get_locks() { return this->get_entities<lock::Lock>(); }
  lock::Lock *get_lock_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<lock::Lock>(key, include_internal);
  }
#endif
#ifdef USE_VALVE
  const std::vector<valve::Valve *> &get_valves() { return this->get_entities<valve::Valve>(); }
  valve::Valve *get_valve_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<valve::Valve>(key, include_internal);
  }
#endif
#ifdef USE_MEDIA_PLAYER
  const std::vector<media_player::MediaPlayer *> &get_media_players() {
    return this->get_entities<media_player::MediaPlayer>();
  }
  media_player::MediaPlayer *get_media_player_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<media_player::MediaPlayer>(key, include_internal);
  }
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  const std::vector<alarm_control_panel::AlarmControlPanel *> &get_alarm_control_panels() {
    return this->get_entities<alarm_control_panel::AlarmControlPanel>();
  }
  alarm_control_panel::AlarmControlPanel *get_alarm_control_panel_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<alarm_control_panel::AlarmControlPanel>(key, include_internal);
  }
#endif

#ifdef USE_EVENT
  const std::vector<event::Event *> &get_events() { return this->get_entities<event::Event>(); }
  event::Event *get_event_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<event::Event>(key, include_internal);
  }
#endif

#ifdef USE_UPDATE
  const std::vector<update::UpdateEntity *> &get_updates() { return this->get_entities<update::UpdateEntity>(); }
  update::UpdateEntity *get_update_by_key(uint32_t key, bool include_internal = false) {
    return this->get_entity_by_key<update::UpdateEntity>(key, include_internal);
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
  entityRegistry<entities_t>::type entities_;
};

/// Global storage of Application pointer - only one Application can exist.
extern Application App;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome
