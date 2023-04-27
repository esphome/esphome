#ifdef USE_LIBRETINY

#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <flashdb.h>
#include <cstring>
#include <vector>
#include <string>

namespace esphome {
namespace libretiny {

static const char *const TAG = "lt.preferences";

struct NVSData {
  std::string key;
  std::vector<uint8_t> data;
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class LibreTinyPreferenceBackend : public ESPPreferenceBackend {
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
    ESP_LOGVV(TAG, "s_pending_save: key: %s, len: %d", key.c_str(), len);
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
    } else {
      ESP_LOGVV(TAG, "fdb_kv_get_blob: key: %s, len: %d", key.c_str(), len);
    }
    return true;
  }
};

class LibreTinyPreferences : public ESPPreferences {
 public:
  struct fdb_kvdb db;
  struct fdb_blob blob;

  void open() {
    //
    fdb_err_t err = fdb_kvdb_init(&db, "esphome", "kvs", NULL, NULL);
    if (err != FDB_NO_ERR) {
      LT_E("fdb_kvdb_init(...) failed: %d", err);
    } else {
      LT_I("Preferences initialized");
    }
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    auto *pref = new LibreTinyPreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    pref->db = &db;
    pref->blob = &blob;

    uint32_t keyval = type;
    pref->key = str_sprintf("%u", keyval);

    return ESPPreferenceObject(pref);
  }

  bool sync() override {
    if (s_pending_save.empty())
      return true;

    ESP_LOGD(TAG, "Saving %d preferences to flash...", s_pending_save.size());
    // goal try write all pending saves even if one fails
    int cached = 0, written = 0, failed = 0;
    fdb_err_t last_err = FDB_NO_ERR;
    std::string last_key{};

    // go through vector from back to front (makes erase easier/more efficient)
    for (ssize_t i = s_pending_save.size() - 1; i >= 0; i--) {
      const auto &save = s_pending_save[i];
      ESP_LOGVV(TAG, "Checking if FDB data %s has changed", save.key.c_str());
      if (is_changed(&db, save)) {
        ESP_LOGV(TAG, "sync: key: %s, len: %d", save.key.c_str(), save.data.size());
        fdb_blob_make(&blob, save.data.data(), save.data.size());
        fdb_err_t err = fdb_kv_set_blob(&db, save.key.c_str(), &blob);
        if (err != FDB_NO_ERR) {
          ESP_LOGV(TAG, "fdb_kv_set_blob('%s', len=%u) failed: %d", save.key.c_str(), save.data.size(), err);
          failed++;
          last_err = err;
          last_key = save.key;
          continue;
        }
        written++;
      } else {
        ESP_LOGD(TAG, "FDB data not changed; skipping %s  len=%u", save.key.c_str(), save.data.size());
        cached++;
      }
      s_pending_save.erase(s_pending_save.begin() + i);
    }
    ESP_LOGD(TAG, "Saving %d preferences to flash: %d cached, %d written, %d failed", cached + written + failed, cached,
             written, failed);
    if (failed > 0) {
      ESP_LOGE(TAG, "Error saving %d preferences to flash. Last error=%d for key=%s", failed, last_err,
               last_key.c_str());
    }

    return failed == 0;
  }

  bool is_changed(const fdb_kvdb_t db, const NVSData &to_save) {
    NVSData stored_data{};
    struct fdb_kv kv;
    fdb_kv_t kvp = fdb_kv_get_obj(db, to_save.key.c_str(), &kv);
    if (kvp == nullptr) {
      ESP_LOGV(TAG, "fdb_kv_get_obj('%s'): nullptr - the key might not be set yet", to_save.key.c_str());
      return true;
    }
    stored_data.data.reserve(kv.value_len);
    fdb_blob_make(&blob, stored_data.data.data(), kv.value_len);
    size_t actual_len = fdb_kv_get_blob(db, to_save.key.c_str(), &blob);
    if (actual_len != kv.value_len) {
      ESP_LOGV(TAG, "fdb_kv_get_blob('%s') len mismatch: %u != %u", to_save.key.c_str(), actual_len, kv.value_len);
      return true;
    }
    return to_save.data != stored_data.data;
  }

  bool reset() override {
    ESP_LOGD(TAG, "Cleaning up preferences in flash...");
    s_pending_save.clear();

    fdb_kv_set_default(&db);
    fdb_kvdb_deinit(&db);
    return true;
  }
};

void setup_preferences() {
  auto *prefs = new LibreTinyPreferences();  // NOLINT(cppcoreguidelines-owning-memory)
  prefs->open();
  global_preferences = prefs;
}

}  // namespace libretiny

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_LIBRETINY
