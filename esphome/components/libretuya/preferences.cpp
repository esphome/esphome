#ifdef USE_LIBRETUYA

#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <flashdb.h>
#include <cstring>
#include <vector>
#include <string>

namespace esphome {
namespace libretuya {

static const char *const TAG = "libretuya.preferences";

struct NVSData {
  std::string key;
  std::vector<uint8_t> data;
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class LibreTuyaPreferenceBackend : public ESPPreferenceBackend {
 public:
  std::string key;
  fdb_kvdb_t db;
  fdb_blob_t blob;

  bool save(const uint8_t *data, size_t len) override {
    // try find in pending saves and update that
    for (auto &obj : s_pending_save) {
      if (obj.key == key) {
        obj.data.assign(data, data + len);
        return true;
      }
    }
    NVSData save{};
    save.key = key;
    save.data.assign(data, data + len);
    s_pending_save.emplace_back(save);
    return true;
  }

  bool load(uint8_t *data, size_t len) override {
    // try find in pending saves and load from that
    for (auto &obj : s_pending_save) {
      if (obj.key == key) {
        if (obj.data.size() != len) {
          // size mismatch
          return false;
        }
        memcpy(data, obj.data.data(), len);
        return true;
      }
    }

    fdb_blob_make(blob, data, len);
    size_t actual_len = fdb_kv_get_blob(db, key.c_str(), blob);
    if (actual_len != len) {
      ESP_LOGVV(TAG, "NVS length does not match (%u!=%u)", actual_len, len);
      return false;
    }
    return true;
  }
};

class LibreTuyaPreferences : public ESPPreferences {
 public:
  struct fdb_kvdb db;
  struct fdb_blob blob;
  uint32_t current_offset = 0;

  void open() {
    //
    fdb_kvdb_init(&db, "esphome", "userdata", NULL, NULL);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    auto *pref = new LibreTuyaPreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    pref->db = &db;
    pref->blob = &blob;
    current_offset += length;

    uint32_t keyval = current_offset ^ type;
    char keybuf[16];
    snprintf(keybuf, sizeof(keybuf), "%d", keyval);
    pref->key = keybuf;  // copied to std::string

    return ESPPreferenceObject(pref);
  }

  bool sync() override {
    if (s_pending_save.empty())
      return true;

    ESP_LOGD(TAG, "Saving preferences to flash...");
    // goal try write all pending saves even if one fails
    bool any_failed = false;

    // go through vector from back to front (makes erase easier/more efficient)
    for (ssize_t i = s_pending_save.size() - 1; i >= 0; i--) {
      const auto &save = s_pending_save[i];
      fdb_blob_make(&blob, save.data.data(), save.data.size());
      fdb_err_t err = fdb_kv_set_blob(&db, save.key.c_str(), &blob);
      if (err != FDB_NO_ERR) {
        ESP_LOGV(TAG, "fdb_kv_set_blob('%s', len=%u) failed: %d", save.key.c_str(), save.data.size(), err);
        any_failed = true;
        continue;
      }
      s_pending_save.erase(s_pending_save.begin() + i);
    }

    return !any_failed;
  }
};

void setup_preferences() {
  auto *prefs = new LibreTuyaPreferences();  // NOLINT(cppcoreguidelines-owning-memory)
  prefs->open();
  global_preferences = prefs;
}

}  // namespace libretuya

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_LIBRETUYA
