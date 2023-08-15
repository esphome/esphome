#include "esphome/core/entity_base.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {

static const char *const TAG = "entity_base";

// Entity Name
const StringRef &EntityBase::get_name() const { return this->name_; }
void EntityBase::set_name(const char *name) {
  this->name_ = StringRef(name);
  if (this->name_.empty()) {
    this->name_ = StringRef(App.get_friendly_name());
    this->has_own_name_ = false;
  } else {
    this->has_own_name_ = true;
  }
}

// Entity Internal
bool EntityBase::is_internal() const { return this->internal_; }
void EntityBase::set_internal(bool internal) { this->internal_ = internal; }

// Entity Disabled by Default
bool EntityBase::is_disabled_by_default() const { return this->disabled_by_default_; }
void EntityBase::set_disabled_by_default(bool disabled_by_default) { this->disabled_by_default_ = disabled_by_default; }

// Entity Icon
std::string EntityBase::get_icon() const {
  if (this->icon_c_str_ == nullptr) {
    return "";
  }
  return this->icon_c_str_;
}
void EntityBase::set_icon(const char *icon) { this->icon_c_str_ = icon; }

// Entity Category
EntityCategory EntityBase::get_entity_category() const { return this->entity_category_; }
void EntityBase::set_entity_category(EntityCategory entity_category) { this->entity_category_ = entity_category; }

// Entity Object ID
std::string EntityBase::get_object_id() const {
  // Check if `App.get_friendly_name()` is constant or dynamic.
  if (!this->has_own_name_ && App.is_name_add_mac_suffix_enabled()) {
    // `App.get_friendly_name()` is dynamic.
    return str_sanitize(str_snake_case(App.get_friendly_name()));
  } else {
    // `App.get_friendly_name()` is constant.
    if (this->object_id_c_str_ == nullptr) {
      return "";
    }
    return this->object_id_c_str_;
  }
}
void EntityBase::set_object_id(const char *object_id) {
  this->object_id_c_str_ = object_id;
  this->calc_object_id_();
}

// Calculate Object ID Hash from Entity Name
void EntityBase::calc_object_id_() {
  // Check if `App.get_friendly_name()` is constant or dynamic.
  if (!this->has_own_name_ && App.is_name_add_mac_suffix_enabled()) {
    // `App.get_friendly_name()` is dynamic.
    const auto object_id = str_sanitize(str_snake_case(App.get_friendly_name()));
    // FNV-1 hash
    this->object_id_hash_ = fnv1_hash(object_id);
  } else {
    // `App.get_friendly_name()` is constant.
    // FNV-1 hash
    this->object_id_hash_ = fnv1_hash(this->object_id_c_str_);
  }
}

uint32_t EntityBase::get_object_id_hash() { return this->object_id_hash_; }

std::string EntityBase_DeviceClass::get_device_class() {
  if (this->device_class_ == nullptr) {
    return "";
  }
  return this->device_class_;
}

void EntityBase_DeviceClass::set_device_class(const char *device_class) { this->device_class_ = device_class; }

std::string EntityBase_UnitOfMeasurement::get_unit_of_measurement() {
  if (this->unit_of_measurement_ == nullptr)
    return "";
  return this->unit_of_measurement_;
}
void EntityBase_UnitOfMeasurement::set_unit_of_measurement(const char *unit_of_measurement) {
  this->unit_of_measurement_ = unit_of_measurement;
}

}  // namespace esphome
