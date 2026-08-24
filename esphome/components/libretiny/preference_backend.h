#pragma once
#ifdef USE_LIBRETINY

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
};

class LibreTinyPreferences;
LibreTinyPreferences *get_preferences();

}  // namespace esphome::libretiny

namespace esphome {
using PreferenceBackend = libretiny::LibreTinyPreferenceBackend;
}  // namespace esphome

#endif  // USE_LIBRETINY
