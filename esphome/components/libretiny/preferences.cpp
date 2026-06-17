#ifdef USE_LIBRETINY

#include "preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstring>
#include <vector>

namespace esphome::libretiny {

static const char *const TAG = "preferences";

struct NVSData {
  uint32_t key;
  SmallInlineBuffer<8> data;  // Most prefs fit in 8 bytes (covers fan, cover, select, etc.)
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool LibreTinyPreferenceBackend::save(const uint8_t *data, size_t len) {
  // try find in pending saves and update that
  for (auto &obj : s_pending_save) {
    if (obj.key == this->key) {
      obj.data.set(data, len);
      return true;
    }
  }
  NVSData save{};
  save.key = this->key;
  save.data.set(data, len);
  s_pending_save.push_back(std::move(save));
  ESP_LOGVV(TAG, "s_pending_save: key: %" PRIu32 ", len: %zu", this->key, len);
  return true;
}

bool LibreTinyPreferenceBackend::load(uint8_t *data, size_t len) {
  // try find in pending saves and load from that
  for (auto &obj : s_pending_save) {
    if (obj.key == this->key) {
      if (obj.data.size() != len) {
        // size mismatch
        return false;
      }
      memcpy(data, obj.data.data(), len);
      return true;
    }
  }

  char key_str[UINT32_MAX_STR_SIZE];
  uint32_to_str(key_str, this->key);
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

void LibreTinyPreferences::open() {
  //
  fdb_err_t err = fdb_kvdb_init(&this->db, "esphome", "kvs", NULL, NULL);
  if (err != FDB_NO_ERR) {
    LT_E("fdb_kvdb_init(...) failed: %d", err);
  } else {
    LT_I("Preferences initialized");
  }
}

ESPPreferenceObject LibreTinyPreferences::make_preference(size_t length, uint32_t type) {
  auto *pref = new LibreTinyPreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
  pref->db = &this->db;
  pref->blob = &this->blob;
  pref->key = type;

  return ESPPreferenceObject(pref);
}

bool LibreTinyPreferences::sync() {
  if (s_pending_save.empty())
    return true;

  ESP_LOGV(TAG, "Saving %zu items...", s_pending_save.size());
  int cached = 0, written = 0, failed = 0;
  fdb_err_t last_err = FDB_NO_ERR;
  uint32_t last_key = 0;

  for (const auto &save : s_pending_save) {
    char key_str[UINT32_MAX_STR_SIZE];
    uint32_to_str(key_str, save.key);
    ESP_LOGVV(TAG, "Checking if FDB data %s has changed", key_str);
    if (this->is_changed_(&this->db, save, key_str)) {
      ESP_LOGV(TAG, "sync: key: %s, len: %zu", key_str, save.data.size());
      fdb_blob_make(&this->blob, save.data.data(), save.data.size());
      fdb_err_t err = fdb_kv_set_blob(&this->db, key_str, &this->blob);
      if (err != FDB_NO_ERR) {
        ESP_LOGV(TAG, "fdb_kv_set_blob('%s', len=%zu) failed: %d", key_str, save.data.size(), err);
        failed++;
        last_err = err;
        last_key = save.key;
        continue;
      }
      written++;
    } else {
      ESP_LOGV(TAG, "FDB data not changed; skipping %" PRIu32 "  len=%zu", save.key, save.data.size());
      cached++;
    }
  }
  s_pending_save.clear();

  if (failed > 0) {
    ESP_LOGE(TAG, "Writing %d items: %d cached, %d written, %d failed. Last error=%d for key=%" PRIu32,
             cached + written + failed, cached, written, failed, last_err, last_key);
  } else if (written > 0) {
    ESP_LOGD(TAG, "Writing %d items: %d cached, %d written, %d failed", cached + written + failed, cached, written,
             failed);
  } else {
    ESP_LOGV(TAG, "Writing %d items: %d cached, %d written, %d failed", cached + written + failed, cached, written,
             failed);
  }

  return failed == 0;
}

bool LibreTinyPreferences::is_changed_(fdb_kvdb_t db, const NVSData &to_save, const char *key_str) {
  struct fdb_kv kv;
  fdb_kv_t kvp = fdb_kv_get_obj(db, key_str, &kv);
  if (kvp == nullptr) {
    ESP_LOGV(TAG, "fdb_kv_get_obj('%s'): nullptr - the key might not be set yet", key_str);
    return true;
  }

  // Check size first - if different, data has changed
  if (kv.value_len != to_save.data.size()) {
    return true;
  }

  // Most preferences are small, use stack buffer with heap fallback for large ones
  SmallBufferWithHeapFallback<256> stored_data(kv.value_len);
  fdb_blob_make(&this->blob, stored_data.get(), kv.value_len);
  size_t actual_len = fdb_kv_get_blob(db, key_str, &this->blob);
  if (actual_len != kv.value_len) {
    ESP_LOGV(TAG, "fdb_kv_get_blob('%s') len mismatch: %zu != %zu", key_str, actual_len, (size_t) kv.value_len);
    return true;
  }

  // Compare the actual data
  return memcmp(to_save.data.data(), stored_data.get(), kv.value_len) != 0;
}

bool LibreTinyPreferences::reset() {
  ESP_LOGD(TAG, "Erasing storage");
  s_pending_save.clear();

  fdb_kv_set_default(&this->db);
  fdb_kvdb_deinit(&this->db);
  return true;
}

static LibreTinyPreferences s_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

LibreTinyPreferences *get_preferences() { return &s_preferences; }

void setup_preferences() {
  s_preferences.open();
  global_preferences = &s_preferences;
}

}  // namespace esphome::libretiny

namespace esphome {
ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome

#endif  // USE_LIBRETINY
