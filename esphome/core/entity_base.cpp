#include "esphome/core/entity_base.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

namespace esphome {

static const char *const TAG = "entity_base";

// Entity Name
const StringRef &EntityBase::get_name() const { return this->name_; }
void EntityBase::set_name(const char *name) { this->set_name(name, 0); }
void EntityBase::set_name(const char *name, uint32_t object_id_hash) {
  this->name_ = StringRef(name);
  if (this->name_.empty()) {
#ifdef USE_DEVICES
    if (this->device_ != nullptr) {
      this->name_ = StringRef(this->device_->get_name());
    } else
#endif
    {
      // Bug-for-bug compatibility with OLD behavior:
      // - With MAC suffix: OLD code used App.get_friendly_name() directly (no fallback)
      // - Without MAC suffix: OLD code used pre-computed object_id with fallback to device name
      const std::string &friendly = App.get_friendly_name();
      if (App.is_name_add_mac_suffix_enabled()) {
        // MAC suffix enabled - use friendly_name directly (even if empty) for compatibility
        this->name_ = StringRef(friendly);
      } else {
        // No MAC suffix - fallback to device name if friendly_name is empty
        this->name_ = StringRef(!friendly.empty() ? friendly : App.get_name());
      }
    }
    this->flags_.has_own_name = false;
    // Dynamic name - must calculate hash at runtime
    this->calc_object_id_();
  } else {
    this->flags_.has_own_name = true;
    // Static name - use pre-computed hash if provided
    if (object_id_hash != 0) {
      this->object_id_hash_ = object_id_hash;
    } else {
      this->calc_object_id_();
    }
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

// Entity Object ID - computed on-demand from name
std::string EntityBase::get_object_id() const {
  char buf[OBJECT_ID_MAX_LEN];
  size_t len = this->write_object_id_to(buf, sizeof(buf));
  return std::string(buf, len);
}

// Calculate Object ID Hash directly from name using snake_case + sanitize
void EntityBase::calc_object_id_() {
  this->object_id_hash_ = fnv1_hash_object_id(this->name_.c_str(), this->name_.size());
}

size_t EntityBase::write_object_id_to(char *buf, size_t buf_size) const {
  size_t len = std::min(this->name_.size(), buf_size - 1);
  for (size_t i = 0; i < len; i++) {
    buf[i] = to_sanitized_char(to_snake_case_char(this->name_[i]));
  }
  buf[len] = '\0';
  return len;
}

StringRef EntityBase::get_object_id_to(std::span<char, OBJECT_ID_MAX_LEN> buf) const {
  size_t len = this->write_object_id_to(buf.data(), buf.size());
  return StringRef(buf.data(), len);
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
