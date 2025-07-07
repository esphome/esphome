#pragma once

#include <string>
#include <cstdint>
#include "string_ref.h"
#include "helpers.h"
#include "log.h"

#ifdef USE_DEVICES
#include "device.h"
#endif

namespace esphome {

enum EntityCategory : uint8_t {
  ENTITY_CATEGORY_NONE = 0,
  ENTITY_CATEGORY_CONFIG = 1,
  ENTITY_CATEGORY_DIAGNOSTIC = 2,
};

// The generic Entity base class that provides an interface common to all Entities.
class EntityBase {
 public:
  // Get/set the name of this Entity
  const StringRef &get_name() const;
  void set_name(const char *name);

  // Get whether this Entity has its own name or it should use the device friendly_name.
  bool has_own_name() const { return this->flags_.has_own_name; }

  // Get the sanitized name of this Entity as an ID.
  std::string get_object_id() const;
  void set_object_id(const char *object_id);

  // Get the unique Object ID of this Entity
  uint32_t get_object_id_hash();

  // Get/set whether this Entity should be hidden outside ESPHome
  bool is_internal() const { return this->flags_.internal; }
  void set_internal(bool internal) { this->flags_.internal = internal; }

  // Check if this object is declared to be disabled by default.
  // That means that when the device gets added to Home Assistant (or other clients) it should
  // not be added to the default view by default, and a user action is necessary to manually add it.
  bool is_disabled_by_default() const { return this->flags_.disabled_by_default; }
  void set_disabled_by_default(bool disabled_by_default) { this->flags_.disabled_by_default = disabled_by_default; }

  // Get/set the entity category.
  EntityCategory get_entity_category() const { return static_cast<EntityCategory>(this->flags_.entity_category); }
  void set_entity_category(EntityCategory entity_category) {
    this->flags_.entity_category = static_cast<uint8_t>(entity_category);
  }

  // Get/set this entity's icon
  std::string get_icon() const;
  void set_icon(const char *icon);

#ifdef USE_DEVICES
  // Get/set this entity's device id
  uint32_t get_device_id() const {
    if (this->device_ == nullptr) {
      return 0;  // No device set, return 0
    }
    return this->device_->get_device_id();
  }
  void set_device(Device *device) { this->device_ = device; }
#endif

  // Check if this entity has state
  bool has_state() const { return this->flags_.has_state; }

  // Set has_state - for components that need to manually set this
  void set_has_state(bool state) { this->flags_.has_state = state; }

 protected:
  /// The hash_base() function has been deprecated. It is kept in this
  /// class for now, to prevent external components from not compiling.
  virtual uint32_t hash_base() { return 0L; }
  void calc_object_id_();

  StringRef name_;
  const char *object_id_c_str_{nullptr};
#ifdef USE_ENTITY_ICON
  const char *icon_c_str_{nullptr};
#endif
  uint32_t object_id_hash_{};
#ifdef USE_DEVICES
  Device *device_{};
#endif

  // Bit-packed flags to save memory (1 byte instead of 5)
  struct EntityFlags {
    uint8_t has_own_name : 1;
    uint8_t internal : 1;
    uint8_t disabled_by_default : 1;
    uint8_t has_state : 1;
    uint8_t entity_category : 2;  // Supports up to 4 categories
    uint8_t reserved : 2;         // Reserved for future use
  } flags_{};
};

class EntityBase_DeviceClass {  // NOLINT(readability-identifier-naming)
 public:
  /// Get the device class, using the manual override if set.
  std::string get_device_class();
  /// Manually set the device class.
  void set_device_class(const char *device_class);

 protected:
  const char *device_class_{nullptr};  ///< Device class override
};

class EntityBase_UnitOfMeasurement {  // NOLINT(readability-identifier-naming)
 public:
  /// Get the unit of measurement, using the manual override if set.
  std::string get_unit_of_measurement();
  /// Manually set the unit of measurement.
  void set_unit_of_measurement(const char *unit_of_measurement);

 protected:
  const char *unit_of_measurement_{nullptr};  ///< Unit of measurement override
};

/**
 * An entity that has a state.
 * @tparam T The type of the state
 */
template<typename T> class StatefulEntityBase : public EntityBase {
 public:
  virtual bool has_state() const { return this->state_.has_value(); }
  virtual const T &get_state() const { return this->state_.value(); }
  virtual T get_state_default(T default_value) const { return this->state_.value_or(default_value); }
  void invalidate_state() { this->set_state_({}); }

  void add_full_state_callback(std::function<void(optional<T> previous, optional<T> current)> &&callback) {
    if (this->full_state_callbacks_ == nullptr)
      this->full_state_callbacks_ = new CallbackManager<void(optional<T> previous, optional<T> current)>();  // NOLINT
    this->full_state_callbacks_->add(std::move(callback));
  }
  void add_on_state_callback(std::function<void(T)> &&callback) {
    if (this->state_callbacks_ == nullptr)
      this->state_callbacks_ = new CallbackManager<void(T)>();  // NOLINT
    this->state_callbacks_->add(std::move(callback));
  }

  void set_trigger_on_initial_state(bool trigger_on_initial_state) {
    this->trigger_on_initial_state_ = trigger_on_initial_state;
  }

 protected:
  optional<T> state_{};
  /**
   * Set a new state for this entity. This will trigger callbacks only if the new state is different from the previous.
   *
   * @param state The new state.
   * @return True if the state was changed, false if it was the same as before.
   */
  bool set_state_(const optional<T> &state) {
    if (this->state_ != state) {
      // call the full state callbacks with the previous and new state
      if (this->full_state_callbacks_ != nullptr)
        this->full_state_callbacks_->call(this->state_, state);
      // trigger legacy callbacks only if the new state is valid and either the trigger on initial state is enabled or
      // the previous state was valid
      auto had_state = this->has_state();
      this->state_ = state;
      if (this->state_callbacks_ != nullptr && state.has_value() && (this->trigger_on_initial_state_ || had_state))
        this->state_callbacks_->call(state.value());
      return true;
    }
    return false;
  }
  bool trigger_on_initial_state_{true};
  // callbacks with full state and previous state
  CallbackManager<void(optional<T> previous, optional<T> current)> *full_state_callbacks_{};
  CallbackManager<void(T)> *state_callbacks_{};
};
}  // namespace esphome
