#pragma once

#include <string>
#include <cstdint>
#include "string_ref.h"

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
  bool has_own_name() const { return this->has_own_name_; }

  // Get the sanitized name of this Entity as an ID.
  std::string get_object_id() const;
  void set_object_id(const char *object_id);

  // Get the unique Object ID of this Entity
  uint32_t get_object_id_hash();

  // Get/set whether this Entity should be hidden from outside of ESPHome
  bool is_internal() const;
  void set_internal(bool internal);

  // Check if this object is declared to be disabled by default.
  // That means that when the device gets added to Home Assistant (or other clients) it should
  // not be added to the default view by default, and a user action is necessary to manually add it.
  bool is_disabled_by_default() const;
  void set_disabled_by_default(bool disabled_by_default);

  // Get/set the entity category.
  EntityCategory get_entity_category() const;
  void set_entity_category(EntityCategory entity_category);

  // Get/set this entity's icon
  std::string get_icon() const;
  void set_icon(const char *icon);

 protected:
  /// The hash_base() function has been deprecated. It is kept in this
  /// class for now, to prevent external components from not compiling.
  virtual uint32_t hash_base() { return 0L; }
  void calc_object_id_();

  StringRef name_;
  const char *object_id_c_str_{nullptr};
  const char *icon_c_str_{nullptr};
  uint32_t object_id_hash_;
  bool has_own_name_{false};
  bool internal_{false};
  bool disabled_by_default_{false};
  EntityCategory entity_category_{ENTITY_CATEGORY_NONE};
};

class EntityBase_DeviceClass {
 public:
  /// Get the device class, using the manual override if set.
  std::string get_device_class();
  /// Manually set the device class.
  void set_device_class(const char *device_class);

 protected:
  const char *device_class_{nullptr};  ///< Device class override
};

class EntityBase_UnitOfMeasurement {
 public:
  /// Get the unit of measurement, using the manual override if set.
  std::string get_unit_of_measurement();
  /// Manually set the unit of measurement.
  void set_unit_of_measurement(const char *unit_of_measurement);

 protected:
  const char *unit_of_measurement_{nullptr};  ///< Unit of measurement override
};

}  // namespace esphome
