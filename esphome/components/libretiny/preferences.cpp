#ifdef USE_LIBRETINY

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include <flashdb.h>
#include <cinttypes>
#include <cstring>
#include <memory>

namespace esphome {
namespace libretiny {

static const char *const TAG = "lt.preferences";

// Buffer size for converting uint32_t to string: max "4294967295" (10 chars) + null terminator + 1 padding
static constexpr size_t KEY_BUFFER_SIZE = 12;

struct NVSData {
  uint32_t key;
  std::unique_ptr<uint8_t[]> data;
  size_t len;

  void set_data(const uint8_t *src, size_t size) {
    if (!this->data || this->len != size) {
      this->data = std::make_unique<uint8_t[]>(size);
      this->len = size;
    }
    memcpy(this->data.get(), src, size);
  }
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class LibreTinyPreferenceBackend : public ESPPreferenceBackend {
 public:
  uint32_t key;
  fdb_kvdb_t db;
  fdb_blob_t blob;

  bool save(const uint8_t *data, size_t len) override {
    // try find in pending saves and update that
    for (auto &obj : s_pending_save) {
      if (obj.key == this->key) {
        obj.set_data(data, len);
        return true;
      }
    }
    NVSData save{};
    save.key = this->key;
    save.set_data(data, len);
    s_pending_save.emplace_back(std::move(save));
    ESP_LOGVV(TAG, "s_pending_save: key: %" PRIu32 ", len: %zu", this->key, len);
    return true;
  }

  bool load(uint8_t *data, size_t len) override {
    // try find in pending saves and load from that
    for (auto &obj : s_pending_save) {
      if (obj.key == this->key) {
        if (obj.len != len) {
          // size mismatch
          return false;
        }
        memcpy(data, obj.data.get(), len);
        return true;
      }
    }

    char key_str[KEY_BUFFER_SIZE];
    snprintf(key_str, sizeof(key_str), "%" PRIu32, this->key);
    fdb_blob_make(this->blob, data, len);
    size_t actual_len = fdb_kv_get_blob(this->db, key_str, this->blob);
    if (actual_len != len) {
      ESP_LOGVV(TAG, "NVS length does not match (%zu!=%zu)", actual_len, len);
      return false;
    } else {
      ESP_LOGVV(TAG, "fdb_kv_get_blob: key: %s, len: %zu", key_str, len);
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
    return this->make_preference(length, type);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    auto *pref = new LibreTinyPreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    pref->db = &this->db;
    pref->blob = &this->blob;
    pref->key = type;

    return ESPPreferenceObject(pref);
  }

  bool sync() override {
    if (s_pending_save.empty())
      return true;

    ESP_LOGV(TAG, "Saving %zu items...", s_pending_save.size());
    // goal try write all pending saves even if one fails
    int cached = 0, written = 0, failed = 0;
    fdb_err_t last_err = FDB_NO_ERR;
    uint32_t last_key = 0;

    // go through vector from back to front (makes erase easier/more efficient)
    for (ssize_t i = s_pending_save.size() - 1; i >= 0; i--) {
      const auto &save = s_pending_save[i];
      char key_str[KEY_BUFFER_SIZE];
      snprintf(key_str, sizeof(key_str), "%" PRIu32, save.key);
      ESP_LOGVV(TAG, "Checking if FDB data %s has changed", key_str);
      if (this->is_changed_(&this->db, save, key_str)) {
        ESP_LOGV(TAG, "sync: key: %s, len: %zu", key_str, save.len);
        fdb_blob_make(&this->blob, save.data.get(), save.len);
        fdb_err_t err = fdb_kv_set_blob(&this->db, key_str, &this->blob);
        if (err != FDB_NO_ERR) {
          ESP_LOGV(TAG, "fdb_kv_set_blob('%s', len=%zu) failed: %d", key_str, save.len, err);
          failed++;
          last_err = err;
          last_key = save.key;
          continue;
        }
        written++;
      } else {
        ESP_LOGD(TAG, "FDB data not changed; skipping %" PRIu32 "  len=%zu", save.key, save.len);
        cached++;
      }
      s_pending_save.erase(s_pending_save.begin() + i);
    }
    ESP_LOGD(TAG, "Writing %d items: %d cached, %d written, %d failed", cached + written + failed, cached, written,
             failed);
    if (failed > 0) {
      ESP_LOGE(TAG, "Writing %d items failed. Last error=%d for key=%" PRIu32, failed, last_err, last_key);
    }

    return failed == 0;
  }

 protected:
  bool is_changed_(fdb_kvdb_t db, const NVSData &to_save, const char *key_str) {
    struct fdb_kv kv;
    fdb_kv_t kvp = fdb_kv_get_obj(db, key_str, &kv);
    if (kvp == nullptr) {
      ESP_LOGV(TAG, "fdb_kv_get_obj('%s'): nullptr - the key might not be set yet", key_str);
      return true;
    }

    // Check size first - if different, data has changed
    if (kv.value_len != to_save.len) {
      return true;
    }

    // Allocate buffer on heap to avoid stack allocation for large data
    auto stored_data = std::make_unique<uint8_t[]>(kv.value_len);
    fdb_blob_make(&this->blob, stored_data.get(), kv.value_len);
    size_t actual_len = fdb_kv_get_blob(db, key_str, &this->blob);
    if (actual_len != kv.value_len) {
      ESP_LOGV(TAG, "fdb_kv_get_blob('%s') len mismatch: %u != %u", key_str, actual_len, kv.value_len);
      return true;
    }

    // Compare the actual data
    return memcmp(to_save.data.get(), stored_data.get(), kv.value_len) != 0;
  }

  bool reset() override {
    ESP_LOGD(TAG, "Erasing storage");
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
