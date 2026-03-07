#pragma once

#include <cstdint>
#include <span>
#include <string>
#include "string_ref.h"
#include "helpers.h"
#include "log.h"
#include "preferences.h"

#ifdef USE_DEVICES
#include "device.h"
#endif

// Forward declarations for friend access from codegen-generated setup()
void setup();           // NOLINT(readability-redundant-declaration) - may be declared in Arduino.h
void original_setup();  // NOLINT(readability-redundant-declaration) - used by cpp unit tests

namespace esphome {

// Extern lookup functions for entity string tables.
// Generated code provides strong definitions; weak defaults return "".
extern const char *entity_device_class_lookup(uint8_t index);
extern const char *entity_uom_lookup(uint8_t index);
extern const char *entity_icon_lookup(uint8_t index);

// Maximum device name length - keep in sync with validate_hostname() in esphome/core/config.py
static constexpr size_t ESPHOME_DEVICE_NAME_MAX_LEN = 31;

// Maximum friendly name length for entities and sub-devices - keep in sync with FRIENDLY_NAME_MAX_LEN in
// esphome/core/config.py
static constexpr size_t ESPHOME_FRIENDLY_NAME_MAX_LEN = 120;

// Maximum domain length (longest: "alarm_control_panel" = 19)
static constexpr size_t ESPHOME_DOMAIN_MAX_LEN = 20;

// Maximum size for object_id buffer (friendly_name + null + margin)
static constexpr size_t OBJECT_ID_MAX_LEN = 128;

// Maximum state length that Home Assistant will accept without raising ValueError
static constexpr size_t MAX_STATE_LEN = 255;

// Maximum device class string buffer size (47 chars + null terminator)
// Longest standard device class: "volatile_organic_compounds_parts" (32 chars)
// Device classes are stored in PROGMEM; on ESP8266 they must be copied to a stack buffer.
static constexpr size_t MAX_DEVICE_CLASS_LENGTH = 48;

// Maximum icon string buffer size (63 chars + null terminator)
// Icons are stored in PROGMEM; on ESP8266 they must be copied to a stack buffer.
static constexpr size_t MAX_ICON_LENGTH = 64;

enum EntityCategory : uint8_t {
  ENTITY_CATEGORY_NONE = 0,
  ENTITY_CATEGORY_CONFIG = 1,
  ENTITY_CATEGORY_DIAGNOSTIC = 2,
};

// The generic Entity base class that provides an interface common to all Entities.
class EntityBase {
 public:
  // Get the name of this Entity
  const StringRef &get_name() const;

  // Get whether this Entity has its own name or it should use the device friendly_name.
  bool has_own_name() const { return this->flags_.has_own_name; }

  // Get the sanitized name of this Entity as an ID.
  // Deprecated: object_id mangles names and all object_id methods are planned for removal.
  // See https://github.com/esphome/backlog/issues/76
  // Now is the time to stop using object_id entirely. If you still need it temporarily,
  // use get_object_id_to() which will remain available longer but will also eventually be removed.
  ESPDEPRECATED("object_id mangles names and all object_id methods are planned for removal "
                "(see https://github.com/esphome/backlog/issues/76). "
                "Now is the time to stop using object_id. If still needed, use get_object_id_to() "
                "which will remain available longer. get_object_id() will be removed in 2026.7.0",
                "2025.12.0")
  std::string get_object_id() const;

  // Get the unique Object ID of this Entity
  uint32_t get_object_id_hash();

  /// Get object_id with zero heap allocation
  /// For static case: returns StringRef to internal storage (buffer unused)
  /// For dynamic case: formats into buffer and returns StringRef to buffer
  StringRef get_object_id_to(std::span<char, OBJECT_ID_MAX_LEN> buf) const;

  /// Write object_id directly to buffer, returns length written (excluding null)
  /// Useful for building compound strings without intermediate buffer
  size_t write_object_id_to(char *buf, size_t buf_size) const;

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

  // Get this entity's device class into a stack buffer.
  // On non-ESP8266: returns pointer to PROGMEM string directly (buffer unused).
  // On ESP8266: copies from PROGMEM to buffer, returns buffer pointer.
  const char *get_device_class_to(std::span<char, MAX_DEVICE_CLASS_LENGTH> buffer) const;

#ifdef USE_ESP8266
  // On ESP8266, rodata is RAM. Device classes are in PROGMEM and cannot be accessed
  // directly as const char*. Use get_device_class_to() with a stack buffer instead.
  template<typename T = int> StringRef get_device_class_ref() const {
    static_assert(sizeof(T) == 0, "get_device_class_ref() unavailable on ESP8266 (rodata is RAM). "
                                  "Use get_device_class_to() with a stack buffer.");
    return StringRef("");
  }
  template<typename T = int> std::string get_device_class() const {
    static_assert(sizeof(T) == 0, "get_device_class() unavailable on ESP8266 (rodata is RAM). "
                                  "Use get_device_class_to() with a stack buffer.");
    return "";
  }
#else
  // Deprecated: use get_device_class_to() instead. Device classes are in PROGMEM.
  ESPDEPRECATED("Use get_device_class_to() instead. Will be removed in ESPHome 2026.9.0", "2026.3.0")
  StringRef get_device_class_ref() const;
  ESPDEPRECATED("Use get_device_class_to() instead. Will be removed in ESPHome 2026.9.0", "2026.3.0")
  std::string get_device_class() const;
#endif
  // Get unit of measurement as StringRef (from packed index)
  StringRef get_unit_of_measurement_ref() const;
  /// Get the unit of measurement as std::string (deprecated, prefer get_unit_of_measurement_ref())
  ESPDEPRECATED("Use get_unit_of_measurement_ref() instead for better performance (avoids string copy). Will be "
                "removed in ESPHome 2026.9.0",
                "2026.3.0")
  std::string get_unit_of_measurement() const;

  // Get this entity's icon into a stack buffer.
  // On ESP32: returns pointer to PROGMEM string directly (buffer unused).
  // On ESP8266: copies from PROGMEM to buffer, returns buffer pointer.
  const char *get_icon_to(std::span<char, MAX_ICON_LENGTH> buffer) const;

#ifdef USE_ESP8266
  // On ESP8266, rodata is RAM. Icons are in PROGMEM and cannot be accessed
  // directly as const char*. Use get_icon_to() with a stack buffer instead.
  template<typename T = int> StringRef get_icon_ref() const {
    static_assert(sizeof(T) == 0,
                  "get_icon_ref() unavailable on ESP8266 (rodata is RAM). Use get_icon_to() with a stack buffer.");
    return StringRef("");
  }
  template<typename T = int> std::string get_icon() const {
    static_assert(sizeof(T) == 0,
                  "get_icon() unavailable on ESP8266 (rodata is RAM). Use get_icon_to() with a stack buffer.");
    return "";
  }
#else
  // Deprecated: use get_icon_to() instead. Icons are in PROGMEM.
  ESPDEPRECATED("Use get_icon_to() instead. Will be removed in ESPHome 2026.9.0", "2026.3.0")
  StringRef get_icon_ref() const;
  ESPDEPRECATED("Use get_icon_to() instead. Will be removed in ESPHome 2026.9.0", "2026.3.0")
  std::string get_icon() const;
#endif

#ifdef USE_DEVICES
  // Get/set this entity's device id
  uint32_t get_device_id() const {
    if (this->device_ == nullptr) {
      return 0;  // No device set, return 0
    }
    return this->device_->get_device_id();
  }
  void set_device(Device *device) { this->device_ = device; }
  // Get the device this entity belongs to (nullptr if main device)
  Device *get_device() const { return this->device_; }
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
   * @deprecated Use make_entity_preference<T>() instead, or preferences won't be migrated.
   * See https://github.com/esphome/backlog/issues/85
   */
  ESPDEPRECATED("Use make_entity_preference<T>() instead, or preferences won't be migrated. "
                "See https://github.com/esphome/backlog/issues/85. Will be removed in 2027.1.0.",
                "2026.7.0")
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

  /// Create a preference object for storing this entity's state/settings.
  /// @tparam T The type of data to store (must be trivially copyable)
  /// @param version Optional version hash XORed with preference key (change when struct layout changes)
  template<typename T> ESPPreferenceObject make_entity_preference(uint32_t version = 0) {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    return this->make_entity_preference_(sizeof(T), version);
  }

 protected:
  friend void ::setup();
  friend void ::original_setup();

  /// Combined entity setup from codegen: set name, object_id hash, and entity string indices.
  void configure_entity_(const char *name, uint32_t object_id_hash, uint32_t entity_strings_packed);

  /// Non-template helper for make_entity_preference() to avoid code bloat.
  /// When preference hash algorithm changes, migration logic goes here.
  ESPPreferenceObject make_entity_preference_(size_t size, uint32_t version);

  void calc_object_id_();

  StringRef name_;
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
  // String table indices — packed into the 3 padding bytes after flags_
#ifdef USE_ENTITY_DEVICE_CLASS
  uint8_t device_class_idx_{};
#endif
#ifdef USE_ENTITY_UNIT_OF_MEASUREMENT
  uint8_t uom_idx_{};
#endif
#ifdef USE_ENTITY_ICON
  uint8_t icon_idx_{};
#endif
};

/// Log entity icon if set (for use in dump_config)
#ifdef USE_ENTITY_ICON
#define LOG_ENTITY_ICON(tag, prefix, obj) log_entity_icon(tag, prefix, obj)
void log_entity_icon(const char *tag, const char *prefix, const EntityBase &obj);
#else
#define LOG_ENTITY_ICON(tag, prefix, obj) ((void) 0)
inline void log_entity_icon(const char *, const char *, const EntityBase &) {}
#endif
/// Log entity device class if set (for use in dump_config)
#define LOG_ENTITY_DEVICE_CLASS(tag, prefix, obj) log_entity_device_class(tag, prefix, obj)
void log_entity_device_class(const char *tag, const char *prefix, const EntityBase &obj);
/// Log entity unit of measurement if set (for use in dump_config)
#define LOG_ENTITY_UNIT_OF_MEASUREMENT(tag, prefix, obj) log_entity_unit_of_measurement(tag, prefix, obj)
void log_entity_unit_of_measurement(const char *tag, const char *prefix, const EntityBase &obj);

/**
 * An entity that has a state.
 * @tparam T The type of the state
 */
template<typename T> class StatefulEntityBase : public EntityBase {
 public:
  virtual bool has_state() const { return this->state_.has_value(); }
  virtual const T &get_state() const { return this->state_.value(); }  // NOLINT(bugprone-unchecked-optional-access)
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
