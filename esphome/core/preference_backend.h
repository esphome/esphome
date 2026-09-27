#pragma once

#include <concepts>
#include <cstdint>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

// Include the concrete preference backend for the active platform.
// Each header defines its backend class, forward-declares its manager class,
// declares get_preferences(), and provides the PreferenceBackend alias.
#ifdef USE_ESP32
#include "esphome/components/esp32/preference_backend.h"
#elif defined(USE_ESP8266)
#include "esphome/components/esp8266/preference_backend.h"
#elif defined(USE_RP2)
#include "esphome/components/rp2/preference_backend.h"
#elif defined(USE_LIBRETINY)
#include "esphome/components/libretiny/preference_backend.h"
#elif defined(USE_HOST)
#include "esphome/components/host/preference_backend.h"
#elif defined(USE_ZEPHYR) && defined(CONFIG_SETTINGS)
#include "esphome/components/zephyr/preference_backend.h"
#endif

// Key-lookup preference backends find stored data by key; their platforms add the
// USE_PREFERENCE_KEY_LOOKUP define from Python codegen, which enables one-shot reads
// of stored data by key (the primitive preference key migrations need). Slot-based
// backends (ESP8266, RP2040) instead allocate a storage slot for every
// make_preference() call and use the key only as a validity tag on that slot;
// migration is not possible there, and key collisions cannot corrupt data.

namespace esphome {

// The PreferenceBackend method surface, asserted on the alias each platform
// header binds. save() persists len bytes; load() fills dest only when the
// stored data exists and matches len. Both report success as their return.
template<typename T>
concept PreferenceBackendContract = requires(T backend, const uint8_t *src, uint8_t *dest, size_t len) {
  { backend.save(src, len) } -> std::same_as<bool>;
  { backend.load(dest, len) } -> std::same_as<bool>;
};

#if !defined(USE_ESP32) && !defined(USE_ESP8266) && !defined(USE_RP2) && !defined(USE_LIBRETINY) && \
    !defined(USE_HOST) && !(defined(USE_ZEPHYR) && defined(CONFIG_SETTINGS))
// Stub for static analysis when no platform is defined.
struct PreferenceBackend {
  bool save(const uint8_t *, size_t) { return false; }
  bool load(uint8_t *, size_t) { return false; }
};
#endif

using ESPPreferenceBackend = PreferenceBackend;
static_assert(PreferenceBackendContract<PreferenceBackend>,
              "The platform's preference backend is missing part of the PreferenceBackend surface");

class ESPPreferenceObject {
 public:
  ESPPreferenceObject() = default;
  explicit ESPPreferenceObject(PreferenceBackend *backend) : backend_(backend) {}

  template<typename T> bool save(const T *src) { return this->save(reinterpret_cast<const uint8_t *>(src), sizeof(T)); }

  template<typename T> bool load(T *dest) { return this->load(reinterpret_cast<uint8_t *>(dest), sizeof(T)); }

  /// Raw save with explicit length, for callers that only know the size at runtime.
  bool save(const uint8_t *src, size_t len) {
    if (this->backend_ == nullptr)
      return false;
    return this->backend_->save(src, len);
  }

  /// Raw load with explicit length, for callers that only know the size at runtime.
  bool load(uint8_t *dest, size_t len) {
    if (this->backend_ == nullptr)
      return false;
    return this->backend_->load(dest, len);
  }

 protected:
  PreferenceBackend *backend_{nullptr};
};

// The preferences manager method surface, asserted in esphome/core/preferences.h
// on the ESPPreferences alias each platform's preferences.h binds through
// DECLARE_PREFERENCE_ALIASES. Semantics beyond the signatures:
// - make_preference: the two-argument form applies the platform's historic
//   default storage; in_flash=false may fall back to flash where the platform
//   has no faster storage.
// - sync: commit pending writes to flash, true on success.
// - reset: forget unsaved changes and re-initialize the permanent storage
//   (usually followed by a restart), true on success.
// The template forms are what component call sites use; PreferencesMixin
// supplies them, but the derived class's non-template overloads hide them
// unless it also declares `using PreferencesMixin<X>::make_preference;`, so
// the concept pins those too.
template<typename T>
concept PreferencesContract = requires(T prefs, size_t len, uint32_t type, bool in_flash) {
  { prefs.make_preference(len, type, in_flash) } -> std::same_as<ESPPreferenceObject>;
  { prefs.make_preference(len, type) } -> std::same_as<ESPPreferenceObject>;
  { prefs.template make_preference<uint32_t>(type, in_flash) } -> std::same_as<ESPPreferenceObject>;
  { prefs.template make_preference<uint32_t>(type) } -> std::same_as<ESPPreferenceObject>;
  { prefs.sync() } -> std::same_as<bool>;
  { prefs.reset() } -> std::same_as<bool>;
};

// Key-lookup platforms additionally provide load_from_key(), a one-shot read
// of a stored preference by key; see the key-lookup note at the top of this
// file. Not part of PreferencesContract, so it is asserted in preferences.h
// only where USE_PREFERENCE_KEY_LOOKUP is set.
template<typename T>
concept PreferencesKeyLookupContract = requires(T prefs, uint32_t type, uint8_t *data, size_t len) {
  { prefs.load_from_key(type, data, len) } -> std::same_as<bool>;
};

/// CRTP mixin providing type-safe template make_preference<T>() helpers.
/// Platform preferences classes inherit this to avoid duplicating these templates.
template<typename Derived> class PreferencesMixin {
 public:
  template<typename T, enable_if_t<is_trivially_copyable<T>::value, bool> = true>
  ESPPreferenceObject make_preference(uint32_t type, bool in_flash) {
    return static_cast<Derived *>(this)->make_preference(sizeof(T), type, in_flash);
  }

  template<typename T, enable_if_t<is_trivially_copyable<T>::value, bool> = true>
  ESPPreferenceObject make_preference(uint32_t type) {
    return static_cast<Derived *>(this)->make_preference(sizeof(T), type);
  }

 private:
  PreferencesMixin() = default;
  friend Derived;
};

// Macro for platform preferences.h headers to declare the standard aliases.
// Must be used at file scope (outside any namespace).
#define DECLARE_PREFERENCE_ALIASES(platform_class) \
  namespace esphome { \
  using Preferences = platform_class; \
  using ESPPreferences = Preferences; \
  extern ESPPreferences *global_preferences; /* NOLINT(cppcoreguidelines-avoid-non-const-global-variables) */ \
  }

}  // namespace esphome
