#pragma once
#ifdef USE_LIBRETINY

#include "esphome/core/preference_backend.h"
#include <flashdb.h>

namespace esphome::libretiny {

struct NVSData;

class LibreTinyPreferences final : public PreferencesMixin<LibreTinyPreferences> {
 public:
  using PreferencesMixin<LibreTinyPreferences>::make_preference;
  void open();
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) {
    return this->make_preference(length, type);
  }
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
};

void setup_preferences();

}  // namespace esphome::libretiny

DECLARE_PREFERENCE_ALIASES(esphome::libretiny::LibreTinyPreferences)

#endif  // USE_LIBRETINY
