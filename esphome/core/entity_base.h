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

// Forward declaration for friend access
namespace api {
class APIConnection;
}  // namespace api

namespace web_server {
struct UrlMatch;
}  // namespace web_server

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

  // Set both name and object_id in one call (reduces generated code size)
  void set_name_and_object_id(const char *name, const char *object_id);

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
  ESPDEPRECATED(
      "Use get_icon_ref() instead for better performance (avoids string copy). Will be removed in ESPHome 2026.5.0",
      "2025.11.0")
  std::string get_icon() const;
  void set_icon(const char *icon);
  StringRef get_icon_ref() const {
    static constexpr auto EMPTY_STRING = StringRef::from_lit("");
#ifdef USE_ENTITY_ICON
    return this->icon_c_str_ == nullptr ? EMPTY_STRING : StringRef(this->icon_c_str_);
#else
    return EMPTY_STRING;
#endif
  }

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

  /**
   * @brief Get a unique hash for storing preferences/settings for this entity.
   *
   * This method returns a hash that uniquely identifies the entity for the purpose of
   * storing preferences (such as calibration, state, etc.). Unlike get_object_id_hash(),
   * this hash also incorporates the device_id (if devices are enabled), ensuring uniqueness
   * across multiple devices that may have entities with the same object_id.
   *
   * Use this method when storing or retrieving preferences/settings that should be unique
   * per device-entity pair. Use get_object_id_hash() when you need a hash that identifies
   * the entity regardless of the device it belongs to.
   *
   * For backward compatibility, if device_id is 0 (the main device), the hash is unchanged
   * from previous versions, so existing single-device configurations will continue to work.
   *
   * @return uint32_t The unique hash for preferences, including device_id if available.
   */
  uint32_t get_preference_hash() {
#ifdef USE_DEVICES
    // Combine object_id_hash with device_id to ensure uniqueness across devices
    // Note: device_id is 0 for the main device, so XORing with 0 preserves the original hash
    // This ensures backward compatibility for existing single-device configurations
    return this->get_object_id_hash() ^ this->get_device_id();
#else
    // Without devices, just use object_id_hash as before
    return this->get_object_id_hash();
#endif
  }

 protected:
  friend class api::APIConnection;
  friend struct web_server::UrlMatch;

  // Get object_id as StringRef when it's static (for API usage)
  // Returns empty StringRef if object_id is dynamic (needs allocation)
  StringRef get_object_id_ref_for_api_() const;

  void calc_object_id_();

  /// Check if the object_id is dynamic (changes with MAC suffix)
  bool is_object_id_dynamic_() const;

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
  ESPDEPRECATED("Use get_device_class_ref() instead for better performance (avoids string copy). Will be removed in "
                "ESPHome 2026.5.0",
                "2025.11.0")
  std::string get_device_class();
  /// Manually set the device class.
  void set_device_class(const char *device_class);
  /// Get the device class as StringRef
  StringRef get_device_class_ref() const {
    static constexpr auto EMPTY_STRING = StringRef::from_lit("");
    return this->device_class_ == nullptr ? EMPTY_STRING : StringRef(this->device_class_);
  }

 protected:
  const char *device_class_{nullptr};  ///< Device class override
};

class EntityBase_UnitOfMeasurement {  // NOLINT(readability-identifier-naming)
 public:
  /// Get the unit of measurement, using the manual override if set.
  ESPDEPRECATED("Use get_unit_of_measurement_ref() instead for better performance (avoids string copy). Will be "
                "removed in ESPHome 2026.5.0",
                "2025.11.0")
  std::string get_unit_of_measurement();
  /// Manually set the unit of measurement.
  void set_unit_of_measurement(const char *unit_of_measurement);
  /// Get the unit of measurement as StringRef
  StringRef get_unit_of_measurement_ref() const {
    static constexpr auto EMPTY_STRING = StringRef::from_lit("");
    return this->unit_of_measurement_ == nullptr ? EMPTY_STRING : StringRef(this->unit_of_measurement_);
  }

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
  void invalidate_state() { this->set_new_state({}); }

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
   * @param new_state The new state.
   * @return True if the state was changed, false if it was the same as before.
   */
  virtual bool set_new_state(const optional<T> &new_state) {
    if (this->state_ != new_state) {
      // call the full state callbacks with the previous and new state
      if (this->full_state_callbacks_ != nullptr)
        this->full_state_callbacks_->call(this->state_, new_state);
      // trigger legacy callbacks only if the new state is valid and either the trigger on initial state is enabled or
      // the previous state was valid
      auto had_state = this->has_state();
      this->state_ = new_state;
      if (this->state_callbacks_ != nullptr && new_state.has_value() && (this->trigger_on_initial_state_ || had_state))
        this->state_callbacks_->call(new_state.value());
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
