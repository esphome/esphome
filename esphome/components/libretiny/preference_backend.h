#pragma once
#ifdef USE_LIBRETINY

#include "esphome/core/defines.h"

#include <cstddef>
#include <cstdint>

// Forward declare FlashDB types to avoid pulling in flashdb.h
// NOLINTBEGIN(readability-identifier-naming)
struct fdb_kvdb;
struct fdb_blob;
// NOLINTEND(readability-identifier-naming)

namespace esphome::libretiny {

class LibreTinyPreferenceBackend final {
 public:
  bool save(const uint8_t *data, size_t len);
  bool load(uint8_t *data, size_t len);

  uint32_t key;
  struct fdb_kvdb *db;
  struct fdb_blob *blob;
#ifdef USE_LIBRETINY_RTC_PREFERENCES
  // storage: rtc. Kept in FlashDB, but the value saved before a boot that RTC memory would not
  // survive (power-on, brown-out, reset pin, unknown) is discarded by its first save or load.
  bool discard_after_hard_reset{false};

 protected:
  bool discard_if_stale_();
#endif
};

class LibreTinyPreferences;
LibreTinyPreferences *get_preferences();

}  // namespace esphome::libretiny

namespace esphome {
using PreferenceBackend = libretiny::LibreTinyPreferenceBackend;
}  // namespace esphome

#endif  // USE_LIBRETINY
