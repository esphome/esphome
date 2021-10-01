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

// Entity Object ID
const std::string &EntityBase::get_object_id() { return this->object_id_; }

// Calculate Object ID Hash from Entity Name
void EntityBase::calc_object_id_() {
  this->object_id_ = sanitize_string_allowlist(to_lowercase_underscore(this->name_), HOSTNAME_CHARACTER_ALLOWLIST);
  // FNV-1 hash
  this->object_id_hash_ = fnv1_hash(this->object_id_);
}
uint32_t EntityBase::get_object_id_hash() { return this->object_id_hash_; }

}  // namespace esphome
