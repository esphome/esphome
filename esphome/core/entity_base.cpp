#include "esphome/core/entity_base.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

namespace esphome {

static const char *const TAG = "entity_base";

// Entity Name
const StringRef &EntityBase::get_name() const { return this->name_; }
void EntityBase::set_name(const char *name) {
  this->name_ = StringRef(name);
  if (this->name_.empty()) {
#ifdef USE_DEVICES
    if (this->device_ != nullptr) {
      this->name_ = StringRef(this->device_->get_name());
    } else
#endif
    {
      this->name_ = StringRef(App.get_friendly_name());
    }
    this->flags_.has_own_name = false;
  } else {
    this->flags_.has_own_name = true;
  }
}

// Entity Icon
std::string EntityBase::get_icon() const {
#ifdef USE_ENTITY_ICON
  if (this->icon_c_str_ == nullptr) {
    return "";
  }
  return this->icon_c_str_;
#else
  return "";
#endif
}
void EntityBase::set_icon(const char *icon) {
#ifdef USE_ENTITY_ICON
  this->icon_c_str_ = icon;
#else
  // No-op when USE_ENTITY_ICON is not defined
#endif
}

// Check if the object_id is dynamic (changes with MAC suffix)
bool EntityBase::is_object_id_dynamic_() const {
  return !this->flags_.has_own_name && App.is_name_add_mac_suffix_enabled();
}

// Entity Object ID
std::string EntityBase::get_object_id() const {
  // Check if `App.get_friendly_name()` is constant or dynamic.
  if (this->is_object_id_dynamic_()) {
    // `App.get_friendly_name()` is dynamic.
    return str_sanitize(str_snake_case(App.get_friendly_name()));
  }
  // `App.get_friendly_name()` is constant.
  return this->object_id_c_str_ == nullptr ? "" : this->object_id_c_str_;
}
StringRef EntityBase::get_object_id_ref_for_api_() const {
  static constexpr auto EMPTY_STRING = StringRef::from_lit("");
  // Return empty for dynamic case (MAC suffix)
  if (this->is_object_id_dynamic_()) {
    return EMPTY_STRING;
  }
  // For static case, return the string or empty if null
  return this->object_id_c_str_ == nullptr ? EMPTY_STRING : StringRef(this->object_id_c_str_);
}
void EntityBase::set_object_id(const char *object_id) {
  this->object_id_c_str_ = object_id;
  this->calc_object_id_();
}

void EntityBase::set_name_and_object_id(const char *name, const char *object_id) {
  this->set_name(name);
  this->object_id_c_str_ = object_id;
  this->calc_object_id_();
}

// Calculate Object ID Hash from Entity Name
void EntityBase::calc_object_id_() {
  this->object_id_hash_ =
      fnv1_hash(this->is_object_id_dynamic_() ? this->get_object_id().c_str() : this->object_id_c_str_);
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
