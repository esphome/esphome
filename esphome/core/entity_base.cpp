#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {

static const char *const TAG = "entity_base";

EntityBase::EntityBase(std::string name) : name_(std::move(name)) { this->calc_object_id_(); }

// Entity Name
const std::string &EntityBase::get_name() const { return this->name_; }
void EntityBase::set_name(const std::string &name) {
  this->name_ = name;
  this->calc_object_id_();
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
const std::string &EntityBase::get_object_id() { return this->object_id_; }

// Calculate Object ID Hash from Entity Name
void EntityBase::calc_object_id_() {
  this->object_id_ = str_sanitize(str_snake_case(this->name_));
  // FNV-1 hash
  this->object_id_hash_ = fnv1_hash(this->object_id_);
}
uint32_t EntityBase::get_object_id_hash() { return this->object_id_hash_; }

}  // namespace esphome
