#include "esphome/core/entity_base.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

#include <new>

namespace esphome {

ESPHOME_LOG_TAG(TAG, "entity_base");

#ifdef USE_ESP8266
namespace {
// RAM copies made by get_name(), only for entities whose name is asked for
struct NameCopy {
  NameCopy *next;
  const char *key;
  StringRef ref;
};
NameCopy *name_copies = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
const StringRef EMPTY_NAME;
}  // namespace

const StringRef &EntityBase::get_name() const {
  const char *key = this->name_.progmem_ptr();
  for (NameCopy *copy = name_copies; copy != nullptr; copy = copy->next) {
    if (copy->key == key)
      return copy->ref;
  }
  size_t len = this->name_.size();
  void *mem = malloc(sizeof(NameCopy) + len + 1);  // NOLINT(cppcoreguidelines-no-malloc)
  if (mem == nullptr)
    return EMPTY_NAME;
  char *buf = static_cast<char *>(mem) + sizeof(NameCopy);
  this->name_.write_to(buf, len + 1);
  name_copies = new (mem) NameCopy{name_copies, key, StringRef(buf, len)};
  return name_copies->ref;
}
#endif

void EntityBase::configure_entity_(const char *name, uint32_t object_id_hash, uint32_t entity_fields) {
  this->name_ = ProgmemStringRef(name, ESPHOME_strlen_P(name));
  if (this->name_.empty()) {
    StringRef fallback;
#ifdef USE_DEVICES
    if (this->device_ != nullptr) {
      fallback = StringRef(this->device_->get_name());
    } else
#endif
    {
      // Bug-for-bug compatibility with OLD behavior:
      // - With MAC suffix: OLD code used App.get_friendly_name() directly (no fallback)
      // - Without MAC suffix: OLD code used pre-computed object_id with fallback to device name
      const auto &friendly = App.get_friendly_name();
      if (App.is_name_add_mac_suffix_enabled()) {
        // MAC suffix enabled - use friendly_name directly (even if empty) for compatibility
        fallback = friendly;
      } else {
        // No MAC suffix - fallback to device name if friendly_name is empty
        fallback = !friendly.empty() ? friendly : App.get_name();
      }
    }
    this->name_ = ProgmemStringRef(fallback);
    this->flags_.has_own_name = false;
    // Dynamic name in RAM - hash it at runtime
    this->object_id_hash_ = fnv1_hash_object_id(fallback.c_str(), fallback.size());
  } else {
    this->flags_.has_own_name = true;
    // Static name - codegen precomputes the hash; external register_* callers may pass 0
    if (object_id_hash == 0) {
      // The name is in flash on ESP8266, so read it byte by byte through progmem_read_byte()
      const char *name_p = this->name_.progmem_ptr();
      object_id_hash = fnv1_hash_object_id(this->name_.size(), [name_p](size_t i) {
        return static_cast<char>(progmem_read_byte(reinterpret_cast<const uint8_t *>(name_p + i)));
      });
    }
    this->object_id_hash_ = object_id_hash;
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

void EntityBase::set_internal(bool internal) {
  // Remove the after-setup path in 2027.3.0 and ignore the call instead.
  if (App.is_setup_complete()) {
    ESP_LOGE(TAG, "'%s': set_internal() after setup is undefined behavior, stops working in 2027.3.0",
             LOG_STR_ARG(this->get_log_name()));
  }
  this->flags_.internal = internal;
}

// Weak default lookup functions — overridden by generated code in main.cpp
__attribute__((weak)) const char *entity_device_class_lookup(uint8_t) { return ""; }
__attribute__((weak)) const char *entity_uom_lookup(uint8_t) { return ""; }
__attribute__((weak)) const char *entity_icon_lookup(uint8_t) { return ""; }

static ProgmemStringRef table_ref(const char *s) { return {s, ESPHOME_strlen_P(s)}; }

ProgmemStringRef EntityBase::get_device_class_progmem() const {
#ifdef USE_ENTITY_DEVICE_CLASS
  return table_ref(entity_device_class_lookup(this->device_class_idx_));
#else
  return {};
#endif
}

const char *EntityBase::get_device_class_to([[maybe_unused]] std::span<char, MAX_DEVICE_CLASS_LENGTH> buffer) const {
#ifdef USE_ESP8266
  this->get_device_class_progmem().write_to(buffer.data(), buffer.size());
  return buffer.data();
#else
  return this->get_device_class_progmem().progmem_ptr();
#endif
}

// Entity unit of measurement (from index)
StringRef EntityBase::get_unit_of_measurement_ref() const {
#ifdef USE_ENTITY_UNIT_OF_MEASUREMENT
  return StringRef(entity_uom_lookup(this->uom_idx_));
#else
  return StringRef(entity_uom_lookup(0));
#endif
}
ProgmemStringRef EntityBase::get_icon_progmem() const {
#ifdef USE_ENTITY_ICON
  return table_ref(entity_icon_lookup(this->icon_idx_));
#else
  return {};
#endif
}

const char *EntityBase::get_icon_to([[maybe_unused]] std::span<char, MAX_ICON_LENGTH> buffer) const {
#ifdef USE_ESP8266
  this->get_icon_progmem().write_to(buffer.data(), buffer.size());
  return buffer.data();
#else
  return this->get_icon_progmem().progmem_ptr();
#endif
}

size_t EntityBase::write_object_id_to(char *buf, size_t buf_size) const {
  size_t len = this->write_name_to(buf, buf_size);
  for (size_t i = 0; i < len; i++) {
    buf[i] = to_sanitized_char(to_snake_case_char(buf[i]));
  }
  return len;
}

StringRef EntityBase::get_object_id_to(std::span<char, OBJECT_ID_MAX_LEN> buf) const {
  size_t len = this->write_object_id_to(buf.data(), buf.size());
  return StringRef(buf.data(), len);
}

uint32_t EntityBase::entity_preference_key_(uint32_t version) {
  // The key hashes the sanitized object_id, so multiple entity names can collide on one
  // key and overwrite each other's stored preferences ("Living Room" and "living_room",
  // or two UTF-8 names that both sanitize to underscores). Keys hashed from the raw name
  // fix this, but they change the entity key API clients track, which the Home Assistant
  // esphome integration cannot handle yet. See: https://github.com/esphome/backlog/issues/85
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return this->get_preference_hash() ^ version;
#pragma GCC diagnostic pop
}

ESPPreferenceObject EntityBase::make_entity_preference_(size_t size, uint32_t version) {
  return global_preferences->make_preference(size, this->entity_preference_key_(version));
}

ESPPreferenceObject EntityBase::make_entity_preference_(size_t size, uint32_t version, bool in_flash) {
  return global_preferences->make_preference(size, this->entity_preference_key_(version), in_flash);
}

bool log_entity_header(const char *tag, const char *prefix, const char *type, const EntityBase *obj) {
  if (obj == nullptr)
    return false;
  ESP_LOGCONFIG(tag, "%s%s '%s'", prefix, type, obj->get_name().c_str());
  return true;
}

#ifdef USE_ENTITY_ICON
void log_entity_icon(const char *tag, const char *prefix, const EntityBase &obj) {
  ProgmemStringRef icon = obj.get_icon_progmem();
  if (!icon.empty()) {
    ESP_LOGCONFIG(tag, "%s  Icon: '%s'", prefix, icon.progmem_ptr());
  }
}
#endif

void log_entity_device_class(const char *tag, const char *prefix, const EntityBase &obj) {
  ProgmemStringRef dc = obj.get_device_class_progmem();
  if (!dc.empty()) {
    ESP_LOGCONFIG(tag, "%s  Device Class: '%s'", prefix, dc.progmem_ptr());
  }
}

void log_entity_unit_of_measurement(const char *tag, const char *prefix, const EntityBase &obj) {
  if (!obj.get_unit_of_measurement_ref().empty()) {
    ESP_LOGCONFIG(tag, "%s  Unit of Measurement: '%s'", prefix, obj.get_unit_of_measurement_ref().c_str());
  }
}

}  // namespace esphome
