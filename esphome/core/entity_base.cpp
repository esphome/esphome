#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {

static const char *const TAG = "entity_base";

EntityBase::EntityBase(const __FlashStringHelper *name) : name_(name) { }

// Entity Name
std::string EntityBase::get_name() const {
  return String(this->name_).c_str();
}
void EntityBase::set_name(const __FlashStringHelper *name) {
  this->name_ = name;
}

// Entity Internal
bool EntityBase::is_internal() const { return this->internal_; }
void EntityBase::set_internal(bool internal) { this->internal_ = internal; }

// Entity Disabled by Default
bool EntityBase::is_disabled_by_default() const { return this->disabled_by_default_; }
void EntityBase::set_disabled_by_default(bool disabled_by_default) { this->disabled_by_default_ = disabled_by_default; }

// Entity Icon
const std::string &EntityBase::get_icon() const { return this->icon_; }
void EntityBase::set_icon(const std::string &name) { this->icon_ = name; }

// Entity Category
EntityCategory EntityBase::get_entity_category() const { return this->entity_category_; }
void EntityBase::set_entity_category(EntityCategory entity_category) { this->entity_category_ = entity_category; }

// Entity Object ID
std::string EntityBase::get_object_id() const {
  return String(this->object_id_).c_str();
}
void EntityBase::set_object_id(const __FlashStringHelper *object_id) {
  this->object_id_ = object_id;
  this->object_id_hash_ = fnv1_hash(this->get_object_id());
}
uint32_t EntityBase::get_object_id_hash() { return this->object_id_hash_; }

}  // namespace esphome
