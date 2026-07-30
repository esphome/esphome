#include "esphome/core/entity_base.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome {

static const char *const TAG = "entity_base";

void EntityBase::configure_entity_(const char *name, uint32_t entity_key, uint32_t entity_fields) {
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
      const auto &friendly = App.get_friendly_name();
      if (App.is_name_add_mac_suffix_enabled()) {
        // MAC suffix enabled - use friendly_name directly (even if empty) for compatibility
        this->name_ = friendly;
      } else {
        // No MAC suffix - fallback to device name if friendly_name is empty
        this->name_ = !friendly.empty() ? friendly : App.get_name();
      }
    }
    this->flags_.has_own_name = false;
    // Dynamic name - must calculate key at runtime
    this->calc_entity_key_();
  } else {
    this->flags_.has_own_name = true;
    // Static name - use pre-computed key if provided
    if (entity_key != 0) {
      this->entity_key_ = entity_key;
    } else {
      this->calc_entity_key_();
    }
  }
  // Unpack entity string table indices and flags from entity_fields.
#ifdef USE_ENTITY_DEVICE_CLASS
  this->device_class_idx_ = (entity_fields >> ENTITY_FIELD_DC_SHIFT) & 0xFF;
#endif
#ifdef USE_ENTITY_UNIT_OF_MEASUREMENT
  this->uom_idx_ = (entity_fields >> ENTITY_FIELD_UOM_SHIFT) & 0xFF;
#endif
#ifdef USE_ENTITY_ICON
  this->icon_idx_ = (entity_fields >> ENTITY_FIELD_ICON_SHIFT) & 0xFF;
#endif
  this->flags_.internal = (entity_fields >> ENTITY_FIELD_INTERNAL_SHIFT) & 1;
  this->flags_.disabled_by_default = (entity_fields >> ENTITY_FIELD_DISABLED_BY_DEFAULT_SHIFT) & 1;
  this->flags_.entity_category = (entity_fields >> ENTITY_FIELD_ENTITY_CATEGORY_SHIFT) & 0x3;
}

// Weak default lookup functions — overridden by generated code in main.cpp
__attribute__((weak)) const char *entity_device_class_lookup(uint8_t) { return ""; }
__attribute__((weak)) const char *entity_uom_lookup(uint8_t) { return ""; }
__attribute__((weak)) const char *entity_icon_lookup(uint8_t) { return ""; }

// Entity device class — buffer-based API for PROGMEM safety on ESP8266
const char *EntityBase::get_device_class_to([[maybe_unused]] std::span<char, MAX_DEVICE_CLASS_LENGTH> buffer) const {
#ifdef USE_ENTITY_DEVICE_CLASS
  const uint8_t idx = this->device_class_idx_;
#else
  const uint8_t idx = 0;
#endif
#ifdef USE_ESP8266
  if (idx == 0)
    return "";
  const char *dc = entity_device_class_lookup(idx);
  ESPHOME_strncpy_P(buffer.data(), dc, buffer.size() - 1);
  buffer[buffer.size() - 1] = '\0';
  return buffer.data();
#else
  return entity_device_class_lookup(idx);
#endif
}

#ifndef USE_ESP8266
// Deprecated device class accessors — not available on ESP8266 (rodata is RAM)
StringRef EntityBase::get_device_class_ref() const {
#ifdef USE_ENTITY_DEVICE_CLASS
  return StringRef(entity_device_class_lookup(this->device_class_idx_));
#else
  return StringRef(entity_device_class_lookup(0));
#endif
}
std::string EntityBase::get_device_class() const {
#ifdef USE_ENTITY_DEVICE_CLASS
  return std::string(entity_device_class_lookup(this->device_class_idx_));
#else
  return std::string(entity_device_class_lookup(0));
#endif
}
#endif  // !USE_ESP8266

// Entity unit of measurement (from index)
StringRef EntityBase::get_unit_of_measurement_ref() const {
#ifdef USE_ENTITY_UNIT_OF_MEASUREMENT
  return StringRef(entity_uom_lookup(this->uom_idx_));
#else
  return StringRef(entity_uom_lookup(0));
#endif
}
std::string EntityBase::get_unit_of_measurement() const {
  return std::string(this->get_unit_of_measurement_ref().c_str());
}

// Entity icon — buffer-based API for PROGMEM safety on ESP8266
const char *EntityBase::get_icon_to([[maybe_unused]] std::span<char, MAX_ICON_LENGTH> buffer) const {
#ifdef USE_ENTITY_ICON
  const uint8_t idx = this->icon_idx_;
#else
  const uint8_t idx = 0;
#endif
#ifdef USE_ESP8266
  if (idx == 0)
    return "";
  const char *icon = entity_icon_lookup(idx);
  ESPHOME_strncpy_P(buffer.data(), icon, buffer.size() - 1);
  buffer[buffer.size() - 1] = '\0';
  return buffer.data();
#else
  return entity_icon_lookup(idx);
#endif
}

#ifndef USE_ESP8266
// Deprecated icon accessors — not available on ESP8266 (rodata is RAM)
StringRef EntityBase::get_icon_ref() const {
#ifdef USE_ENTITY_ICON
  return StringRef(entity_icon_lookup(this->icon_idx_));
#else
  return StringRef(entity_icon_lookup(0));
#endif
}
std::string EntityBase::get_icon() const {
#ifdef USE_ENTITY_ICON
  return std::string(entity_icon_lookup(this->icon_idx_));
#else
  return std::string(entity_icon_lookup(0));
#endif
}
#endif  // !USE_ESP8266

// Calculate the entity key directly from the raw name (no transformations)
void EntityBase::calc_entity_key_() { this->entity_key_ = fnv1_hash_bytes(this->name_.c_str(), this->name_.size()); }

// Reconstruct the OLD (pre-2026.8.0) object_id-based hash for preference key compatibility.
// Named entities historically used the hash pre-computed by Python code generation, which
// sanitized per UTF-8 code point; entities without their own name computed the hash at
// runtime per byte. See https://github.com/esphome/backlog/issues/85
uint32_t EntityBase::calc_old_object_id_hash_() const {
  return fnv1_hash_object_id(this->name_.c_str(), this->name_.size(), this->flags_.has_own_name);
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

ESPPreferenceObject EntityBase::make_entity_preference_(size_t size, uint32_t version) {
  // The old key hashed the sanitized object_id, so multiple entity names could collide on
  // one key and overwrite each other's stored preferences; the new key hashes the raw name.
  // See: https://github.com/esphome/backlog/issues/85
  uint32_t old_key = this->old_preference_key_base_() ^ version;
#ifdef USE_PREFERENCE_KEY_LOOKUP
  uint32_t new_key = this->preference_key_base_() ^ version;
  auto pref = global_preferences->make_preference(size, new_key);
  // All in-tree entity preferences fit the stack buffer, so migration never hits the heap
  SmallBufferWithHeapFallback<64> buffer(size);
  migrate_preference(pref, buffer.get(), size, old_key, new_key);
  return pref;
#else
  // Slot-based backends keep the old key: it is only a validity tag on a positional slot,
  // so collisions cannot corrupt data there and keeping it preserves stored state.
  return global_preferences->make_preference(size, old_key);
#endif
}

#ifdef USE_ENTITY_ICON
void log_entity_icon(const char *tag, const char *prefix, const EntityBase &obj) {
  char icon_buf[MAX_ICON_LENGTH];
  const char *icon = obj.get_icon_to(icon_buf);
  if (icon[0] != '\0') {
    ESP_LOGCONFIG(tag, "%s  Icon: '%s'", prefix, icon);
  }
}
#endif

void log_entity_device_class(const char *tag, const char *prefix, const EntityBase &obj) {
  char dc_buf[MAX_DEVICE_CLASS_LENGTH];
  const char *dc = obj.get_device_class_to(dc_buf);
  if (dc[0] != '\0') {
    ESP_LOGCONFIG(tag, "%s  Device Class: '%s'", prefix, dc);
  }
}

void log_entity_unit_of_measurement(const char *tag, const char *prefix, const EntityBase &obj) {
  if (!obj.get_unit_of_measurement_ref().empty()) {
    ESP_LOGCONFIG(tag, "%s  Unit of Measurement: '%s'", prefix, obj.get_unit_of_measurement_ref().c_str());
  }
}

}  // namespace esphome
