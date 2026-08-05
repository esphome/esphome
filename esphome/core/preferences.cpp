#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {

#ifdef USE_PREFERENCE_KEY_LOOKUP
static const char *const TAG = "preferences";

bool migrate_preference(ESPPreferenceObject &new_pref, uint8_t *scratch, size_t size, uint32_t old_key,
                        uint32_t new_key) {
  if (new_pref.load(scratch, size))
    return true;  // Current data present - never overwrite newer data with the old copy
  // One-shot read by key: no backend is allocated for the old key, so boots with
  // nothing to migrate (for example fresh installs) cost no heap
  if (old_key == new_key || !global_preferences->load_from_key(old_key, scratch, size))
    return false;  // No data stored under the old key, nothing to migrate
  if (!new_pref.save(scratch, size)) {
    ESP_LOGW(TAG, "Pref migration %" PRIx32 " -> %" PRIx32 " failed", old_key, new_key);
  }
  return true;
}
#endif  // USE_PREFERENCE_KEY_LOOKUP

}  // namespace esphome
