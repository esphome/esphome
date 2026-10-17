#ifdef USE_LIBRETINY

#include "preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstring>
#include <vector>

namespace esphome::libretiny {

ESPHOME_LOG_TAG(TAG, "preferences");

struct NVSData {
  uint32_t key;
  SmallInlineBuffer<8> data;  // Most prefs fit in 8 bytes (covers fan, cover, select, etc.)
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_LIBRETINY_RTC_PREFERENCES
// Whether this boot followed a reset that RTC memory survives on ESP32 and ESP8266. LibreTiny chips
// have no such memory, so storage: rtc keeps preferences in flash and uses this to forget them.
static bool boot_kept_rtc_preferences() {
  switch (lt_get_reboot_reason()) {
    case REBOOT_REASON_SOFTWARE:
    case REBOOT_REASON_WATCHDOG:
    case REBOOT_REASON_CRASH:
    case REBOOT_REASON_SLEEP_GPIO:
    case REBOOT_REASON_SLEEP_RTC:
    case REBOOT_REASON_SLEEP_USB:
      return true;
    default:  // power-on, brown-out, reset pin, debugger, unknown
      return false;
  }
}

bool LibreTinyPreferenceBackend::discard_if_stale_() {
  // Runs on a preference's first save or load until the value from before this boot is gone; save()
  // then clears the flag, so a value this boot saved is never discarded.
  if (!this->discard_after_hard_reset)
    return false;
  if (boot_kept_rtc_preferences()) {
    this->discard_after_hard_reset = false;
    return false;
  }
  char key_str[UINT32_MAX_STR_SIZE];
  uint32_to_str(key_str, this->key);
  fdb_err_t err = fdb_kv_del(this->db, key_str);
  if (err == FDB_NO_ERR || err == FDB_KV_NAME_ERR) {  // FDB_KV_NAME_ERR: nothing was stored
    this->discard_after_hard_reset = false;
    if (err == FDB_NO_ERR) {
      ESP_LOGD(TAG, "Discarded rtc preference %s after a hard reset", key_str);
    }
  } else {
    // Keep the flag, so the next save or load tries again
    ESP_LOGW(TAG, "Failed to discard rtc preference %s: %d", key_str, err);
  }
  // Report nothing stored either way: a value from before the hard reset must not be restored
  return true;
}
#endif  // USE_LIBRETINY_RTC_PREFERENCES

bool LibreTinyPreferenceBackend::save(const uint8_t *data, size_t len) {
#ifdef USE_LIBRETINY_RTC_PREFERENCES
  this->discard_if_stale_();
  this->discard_after_hard_reset = false;  // this save replaces any value from before the boot
#endif
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
#ifdef USE_LIBRETINY_RTC_PREFERENCES
  if (this->discard_if_stale_())
    return false;
#endif
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

#ifdef USE_LIBRETINY_RTC_PREFERENCES
ESPPreferenceObject LibreTinyPreferences::make_preference(size_t length, uint32_t type, bool in_flash) {
  if (!in_flash) {
    auto *pref = new LibreTinyPreferenceBackend(this->make_backend_(type));  // NOLINT(cppcoreguidelines-owning-memory)
    pref->discard_after_hard_reset = true;
    return ESPPreferenceObject(pref);
  }
  return this->make_preference(length, type);
}
#endif

ESPPreferenceObject LibreTinyPreferences::make_preference(size_t length, uint32_t type) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return ESPPreferenceObject(new LibreTinyPreferenceBackend(this->make_backend_(type)));
}

LibreTinyPreferenceBackend LibreTinyPreferences::make_backend_(uint32_t type) {
  LibreTinyPreferenceBackend backend;
  backend.key = type;
  backend.db = &this->db;
  backend.blob = &this->blob;
  return backend;
}

bool LibreTinyPreferences::load_from_key(uint32_t type, uint8_t *data, size_t len) {
  LibreTinyPreferenceBackend backend = this->make_backend_(type);
  return backend.load(data, len);
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
