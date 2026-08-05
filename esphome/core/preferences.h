#pragma once

#include "esphome/core/preference_backend.h"

// Include the concrete preferences manager for the active platform.
// Each header defines its manager class and provides the Preferences,
// ESPPreferences, and global_preferences declarations.
#ifdef USE_ESP32
#include "esphome/components/esp32/preferences.h"
#elif defined(USE_ESP8266)
#include "esphome/components/esp8266/preferences.h"
#elif defined(USE_RP2)
#include "esphome/components/rp2/preferences.h"
#elif defined(USE_LIBRETINY)
#include "esphome/components/libretiny/preferences.h"
#elif defined(USE_HOST)
#include "esphome/components/host/preferences.h"
#elif defined(USE_ZEPHYR) && defined(CONFIG_SETTINGS)
#include "esphome/components/zephyr/preferences.h"
#else
namespace esphome {
struct Preferences : public PreferencesMixin<Preferences> {
  using PreferencesMixin<Preferences>::make_preference;
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  ESPPreferenceObject make_preference(size_t, uint32_t) { return {}; }
  bool load_from_key(uint32_t, uint8_t *, size_t) { return false; }

  /**
   * Commit pending writes to flash.
   *
   * @return true if write is successful.
   */
  bool sync() { return false; }

  /**
   * Forget all unsaved changes and re-initialize the permanent preferences storage.
   * Usually followed by a restart which moves the system to "factory" conditions
   *
   * @return true if operation is successful.
   */
  bool reset() { return false; }
};
using ESPPreferences = Preferences;
extern ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome
#endif

#ifdef USE_PREFERENCE_KEY_LOOKUP
namespace esphome {
/// Copy preference data stored under old_key into new_pref (created for new_key) if the keys
/// differ and new_pref has no data yet. scratch must hold at least size bytes.
/// Returns true when scratch holds the entity's current data (loaded or just migrated).
/// The old entry is intentionally left in place so a firmware downgrade still finds its data.
/// If saving under the new key fails, callers that consume scratch (like TextSaver) still get
/// valid data for this boot, callers that reload from the preference fall back to their
/// defaults, and the migration simply runs again on the next boot.
/// Only available on key-lookup preference backends; slot-based backends keep their old
/// keys instead. See: https://github.com/esphome/backlog/issues/85
bool migrate_preference(ESPPreferenceObject &new_pref, uint8_t *scratch, size_t size, uint32_t old_key,
                        uint32_t new_key);
}  // namespace esphome
#endif  // USE_PREFERENCE_KEY_LOOKUP
