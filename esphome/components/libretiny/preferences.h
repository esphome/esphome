#pragma once
#ifdef USE_LIBRETINY

#include "esphome/core/defines.h"
#include "esphome/core/preference_backend.h"
#include <flashdb.h>

namespace esphome::libretiny {

struct NVSData;

class LibreTinyPreferences final : public PreferencesMixin<LibreTinyPreferences> {
 public:
  using PreferencesMixin<LibreTinyPreferences>::make_preference;
  void open();
#ifdef USE_LIBRETINY_RTC_PREFERENCES
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash);
  /// Whether an rtc entry could not be dropped at open; rtc loads then report nothing stored this boot
  bool rtc_discard_failed() const { return this->rtc_discard_failed_; }
#else
  // Without rtc storage every preference lives in flash; inline so builds that don't use rtc pay nothing
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) {
    return this->make_preference(length, type);
  }
#endif
  ESPPreferenceObject make_preference(size_t length, uint32_t type);
  /// One-shot read of a stored preference by key, without allocating a backend
  bool load_from_key(uint32_t type, uint8_t *data, size_t len);
  bool sync();
  bool reset();

  struct fdb_kvdb db;
  struct fdb_blob blob;

 protected:
  LibreTinyPreferenceBackend make_backend_(uint32_t type);
  bool is_changed_(fdb_kvdb_t db, const NVSData &to_save, const char *key_str);
#ifdef USE_LIBRETINY_RTC_PREFERENCES
  void discard_rtc_entries_();
  bool rtc_discard_failed_{false};
#endif
};

void setup_preferences();

}  // namespace esphome::libretiny

DECLARE_PREFERENCE_ALIASES(esphome::libretiny::LibreTinyPreferences)

#endif  // USE_LIBRETINY
