#pragma once

#include <string>
#include <cstdint>
#include "WString.h"

namespace esphome {

enum EntityCategory : uint8_t {
  ENTITY_CATEGORY_NONE = 0,
  ENTITY_CATEGORY_CONFIG = 1,
  ENTITY_CATEGORY_DIAGNOSTIC = 2,
};

// The generic Entity base class that provides an interface common to all Entities.
class EntityBase {
 public:
  EntityBase() : EntityBase(F("")) {}
  explicit EntityBase(const __FlashStringHelper *name);

  // Get/set the name of this Entity
  std::string get_name() const;
  void set_name(const __FlashStringHelper *name);

  // Get/set the sanitized name of this Entity as an ID.
  std::string get_object_id() const;
  void set_object_id(const __FlashStringHelper *object_id);

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
  const std::string &get_icon() const;
  void set_icon(const std::string &name);

 protected:
  virtual uint32_t hash_base() = 0;

  const __FlashStringHelper * name_;
  const __FlashStringHelper * object_id_;
  std::string icon_;
  uint32_t object_id_hash_;
  bool internal_{false};
  bool disabled_by_default_{false};
  EntityCategory entity_category_{ENTITY_CATEGORY_NONE};
};

}  // namespace esphome
