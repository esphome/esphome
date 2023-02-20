#pragma once

#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/scheduler.h"
#include "esphome/core/entity_base.h"

namespace esphome {

class Application {
 public:
  void pre_setup(const std::string &name, const std::string &friendly_name, const std::string &comment,
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

  template<class Entity> const std::vector<EntityBase *> &get_entities() {
    static_assert(std::is_base_of<EntityBase, Entity>::value, "Only EntityBase subclasses can be used");
    assert(this->entities_.size() > Entity::ENTITY_TYPE);
    return this->entities_[Entity::ENTITY_TYPE];
  }

  template<class Entity> Entity *get_entity_by_key(uint32_t key, bool include_internal = false) {
    static_assert(std::is_base_of<EntityBase, Entity>::value, "Only EntityBase subclasses can be used");
    for (auto &entity : this->entities_[Entity::ENTITY_TYPE])
      if (entity->get_object_id_hash() == key && (include_internal || !entity->is_internal()))
        return static_cast<Entity *>(entity);
    return nullptr;
  }

  template<class Entity> void register_entity(Entity *entity) {
    static_assert(std::is_base_of<EntityBase, Entity>::value, "Only EntityBase subclasses can be registered");
    // it is done that way to allow adding custom components without changing core
    if (entities_.size() < Entity::ENTITY_TYPE + 1) {
      entities_.resize(Entity::ENTITY_TYPE + 1);
    }
    this->entities_[Entity::ENTITY_TYPE].push_back(entity);
  }

  const std::vector<std::vector<EntityBase *>> &get_entities_all_types() { return this->entities_; }

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
  /// Get the comment of this Application set by pre_setup().
  const std::string &get_comment() const { return this->comment_; }

  bool is_name_add_mac_suffix_enabled() const { return this->name_add_mac_suffix_; }

  const std::string &get_compilation_time() const { return this->compilation_time_; }

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

  void schedule_dump_config() { this->dump_config_at_ = 0; }

  void feed_wdt();

  void reboot();

  void safe_reboot();

  void run_safe_shutdown_hooks();

  uint32_t get_app_state() const { return this->app_state_; }

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
  std::string comment_;
  std::string compilation_time_;
  bool name_add_mac_suffix_;
  uint32_t last_loop_{0};
  uint32_t loop_interval_{16};
  size_t dump_config_at_{SIZE_MAX};
  uint32_t app_state_{0};
  std::vector<std::vector<EntityBase *>> entities_;
};

/// Global storage of Application pointer - only one Application can exist.
extern Application App;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome
