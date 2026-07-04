#ifdef USE_ESP32

#include "preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences_rtc.h"
#include <esp_attr.h>
#include <nvs_flash.h>
#include <soc/soc_caps.h>
#include <cstring>
#include <vector>

namespace esphome::esp32 {

static const char *const TAG = "preferences";

struct NVSData {
  uint32_t key;
  SmallInlineBuffer<8> data;  // Most prefs fit in 8 bytes (covers fan, cover, select, etc.)
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// RTC memory backend for preferences requested with in_flash=false. Survives deep sleep and
// software/CPU resets, but not power loss; integrity is guarded by a per-record checksum so
// power-on garbage is detected on load. Keep this small: RTC memory is scarce and shared.
//
// Only compiled in when USE_ESP32_RTC_PREFERENCES_STORAGE is set (see preferences.h): the storage
// buffer reserves RTC memory, so it exists only when some config option actually selected RTC
// storage AND the variant has RTC memory (the ESP32-C2 and -C61 have none, so RTC_NOINIT_ATTR would
// have no section to land in and fail to link). Otherwise in_flash=false transparently falls back
// to NVS (see make_preference below).
//
// On variants with only RTC fast memory (C3/C6/H2/P4/C5/...) RTC_NOINIT_ATTR lands in RTC fast memory.
// This is still safe: the linker reserves .rtc_noinit ahead of any RTC-fast-as-heap pool
// (CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP), and IDF keeps the RTC fast power domain on in deep
// sleep (forced on whether or not it is used as heap), so the data is retained across both resets and
// deep sleep -- only power loss clears it.
#ifdef USE_ESP32_RTC_PREFERENCES_STORAGE
static constexpr size_t RTC_PREF_SIZE_WORDS = 64;  // 256 bytes
static constexpr size_t RTC_PREF_MAX_WORDS = 255;  // length_words field is a uint8_t

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static RTC_NOINIT_ATTR uint32_t s_rtc_storage[RTC_PREF_SIZE_WORDS];

static bool save_to_rtc(uint16_t offset, uint32_t key, uint8_t length_words, const uint8_t *data, size_t len) {
  if (rtc_pref_bytes_to_words(len) != length_words)
    return false;
  const size_t buffer_size = static_cast<size_t>(length_words) + 1;
  if (static_cast<size_t>(offset) + buffer_size > RTC_PREF_SIZE_WORDS)
    return false;
  rtc_pref_encode(&s_rtc_storage[offset], key, length_words, data, len);
  return true;
}

static bool load_from_rtc(uint16_t offset, uint32_t key, uint8_t length_words, uint8_t *data, size_t len) {
  if (rtc_pref_bytes_to_words(len) != length_words)
    return false;
  const size_t buffer_size = static_cast<size_t>(length_words) + 1;
  if (static_cast<size_t>(offset) + buffer_size > RTC_PREF_SIZE_WORDS)
    return false;
  return rtc_pref_decode(&s_rtc_storage[offset], key, length_words, data, len);
}
#endif  // USE_ESP32_RTC_PREFERENCES_STORAGE

// open() runs from app_main() before the logger is initialized, so any failure
// must be deferred until after global_logger is set. This is emitted from the
// first make_preference() call, which runs from the generated setup() after
// log->pre_setup() has run at EARLY_INIT priority.
static esp_err_t s_open_err = ESP_OK;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool ESP32PreferenceBackend::save(const uint8_t *data, size_t len) {
#ifdef USE_ESP32_RTC_PREFERENCES_STORAGE
  if (!this->in_flash)
    return save_to_rtc(this->rtc_offset, this->key, this->length_words, data, len);
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

bool ESP32PreferenceBackend::load(uint8_t *data, size_t len) {
#ifdef USE_ESP32_RTC_PREFERENCES_STORAGE
  if (!this->in_flash)
    return load_from_rtc(this->rtc_offset, this->key, this->length_words, data, len);
#endif
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
  size_t actual_len;
  esp_err_t err = nvs_get_blob(this->nvs_handle, key_str, nullptr, &actual_len);
  if (err != 0) {
    ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", key_str, esp_err_to_name(err));
    return false;
  }
  if (actual_len != len) {
    ESP_LOGVV(TAG, "NVS length does not match (%zu!=%zu)", actual_len, len);
    return false;
  }
  err = nvs_get_blob(this->nvs_handle, key_str, data, &len);
  if (err != 0) {
    ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", key_str, esp_err_to_name(err));
    return false;
  } else {
    ESP_LOGVV(TAG, "nvs_get_blob: key: %s, len: %zu", key_str, len);
  }
  return true;
}

void ESP32Preferences::open() {
  // Runs from app_main() before the logger is initialized; any logging here
  // must be deferred. See s_open_err and make_preference() below.
  nvs_flash_init();
  esp_err_t err = nvs_open("esphome", NVS_READWRITE, &this->nvs_handle);
  if (err == 0)
    return;

  s_open_err = err;
  nvs_flash_deinit();
  nvs_flash_erase();
  nvs_flash_init();

  err = nvs_open("esphome", NVS_READWRITE, &this->nvs_handle);
  if (err != 0) {
    this->nvs_handle = 0;
  }
}

ESPPreferenceObject ESP32Preferences::make_preference(size_t length, uint32_t type, bool in_flash) {
#ifdef USE_ESP32_RTC_PREFERENCES_STORAGE
  if (!in_flash)
    return this->make_rtc_preference_(length, type);
#else
  if (!in_flash) {
    // RTC storage is not compiled in (no config option selected it), so this request
    // falls back to NVS -- the historic ESP32 behavior. Warn once so callers explicitly
    // asking for RTC storage can discover the fallback.
    static bool warned = false;
    if (!warned) {
      ESP_LOGW(TAG, "RTC preference storage not compiled in; using NVS (enable with 'preferences: rtc_storage: true')");
      warned = true;
    }
  }
#endif
  // in_flash, or RTC storage not compiled in: fall back to NVS.
  return this->make_preference(length, type);
}

ESPPreferenceObject ESP32Preferences::make_preference(size_t length, uint32_t type) {
  if (s_open_err != ESP_OK) {
    if (this->nvs_handle == 0) {
      ESP_LOGW(TAG, "nvs_open failed: %s - NVS unavailable", esp_err_to_name(s_open_err));
    } else {
      ESP_LOGW(TAG, "nvs_open failed: %s - erased NVS", esp_err_to_name(s_open_err));
    }
    s_open_err = ESP_OK;
  }
  auto *pref = new ESP32PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
  pref->nvs_handle = this->nvs_handle;
  pref->key = type;
  pref->in_flash = true;

  return ESPPreferenceObject(pref);
}

#ifdef USE_ESP32_RTC_PREFERENCES_STORAGE
ESPPreferenceObject ESP32Preferences::make_rtc_preference_(size_t length, uint32_t type) {
  const uint32_t length_words = rtc_pref_bytes_to_words(length);
  if (length_words > RTC_PREF_MAX_WORDS) {
    ESP_LOGE(TAG, "RTC preference too large: %" PRIu32 " words", length_words);
    return {};
  }
  const uint32_t total_words = length_words + 1;  // +1 for checksum
  if (static_cast<size_t>(this->current_rtc_offset_) + total_words > RTC_PREF_SIZE_WORDS) {
    ESP_LOGE(TAG, "RTC preference storage full, cannot allocate %" PRIu32 " words", total_words);
    return {};
  }
  auto *pref = new ESP32PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
  pref->key = type;
  pref->in_flash = false;
  pref->rtc_offset = this->current_rtc_offset_;
  pref->length_words = static_cast<uint8_t>(length_words);
  this->current_rtc_offset_ += static_cast<uint16_t>(total_words);

  return ESPPreferenceObject(pref);
}
#endif  // USE_ESP32_RTC_PREFERENCES_STORAGE

bool ESP32Preferences::sync() {
  if (s_pending_save.empty())
    return true;

  ESP_LOGV(TAG, "Saving %zu items...", s_pending_save.size());
  int cached = 0, written = 0, failed = 0;
  esp_err_t last_err = ESP_OK;
  uint32_t last_key = 0;

  for (const auto &save : s_pending_save) {
    char key_str[UINT32_MAX_STR_SIZE];
    uint32_to_str(key_str, save.key);
    ESP_LOGVV(TAG, "Checking if NVS data %s has changed", key_str);
    if (this->is_changed_(this->nvs_handle, save, key_str)) {
      esp_err_t err = nvs_set_blob(this->nvs_handle, key_str, save.data.data(), save.data.size());
      ESP_LOGV(TAG, "sync: key: %s, len: %zu", key_str, save.data.size());
      if (err != 0) {
        ESP_LOGV(TAG, "nvs_set_blob('%s', len=%zu) failed: %s", key_str, save.data.size(), esp_err_to_name(err));
        failed++;
        last_err = err;
        last_key = save.key;
        continue;
      }
      written++;
    } else {
      ESP_LOGV(TAG, "NVS data not changed skipping %" PRIu32 "  len=%zu", save.key, save.data.size());
      cached++;
    }
  }
  s_pending_save.clear();

  if (failed > 0) {
    ESP_LOGE(TAG, "Writing %d items: %d cached, %d written, %d failed. Last error=%s for key=%" PRIu32,
             cached + written + failed, cached, written, failed, esp_err_to_name(last_err), last_key);
  } else if (written > 0) {
    ESP_LOGD(TAG, "Writing %d items: %d cached, %d written, %d failed", cached + written + failed, cached, written,
             failed);
  } else {
    ESP_LOGV(TAG, "Writing %d items: %d cached, %d written, %d failed", cached + written + failed, cached, written,
             failed);
  }

  // note: commit on esp-idf currently is a no-op, nvs_set_blob always writes
  esp_err_t err = nvs_commit(this->nvs_handle);
  if (err != 0) {
    ESP_LOGV(TAG, "nvs_commit() failed: %s", esp_err_to_name(err));
    return false;
  }

  return failed == 0;
}

bool ESP32Preferences::is_changed_(uint32_t nvs_handle, const NVSData &to_save, const char *key_str) {
  size_t actual_len;
  esp_err_t err = nvs_get_blob(nvs_handle, key_str, nullptr, &actual_len);
  if (err != 0) {
    ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", key_str, esp_err_to_name(err));
    return true;
  }
  // Check size first before allocating memory
  if (actual_len != to_save.data.size()) {
    return true;
  }
  // Most preferences are small, use stack buffer with heap fallback for large ones
  SmallBufferWithHeapFallback<256> stored_data(actual_len);
  err = nvs_get_blob(nvs_handle, key_str, stored_data.get(), &actual_len);
  if (err != 0) {
    ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", key_str, esp_err_to_name(err));
    return true;
  }
  return memcmp(to_save.data.data(), stored_data.get(), to_save.data.size()) != 0;
}

bool ESP32Preferences::reset() {
  ESP_LOGD(TAG, "Erasing storage");
  s_pending_save.clear();
#ifdef USE_ESP32_RTC_PREFERENCES_STORAGE
  // Invalidate RTC-backed preferences too (checksum will no longer match). current_rtc_offset_ is
  // deliberately left alone: existing backends keep pointing at their allocated slots, and reset()
  // is always followed by a restart (same reason nvs_handle is zeroed below).
  memset(s_rtc_storage, 0, sizeof(s_rtc_storage));
#endif

  nvs_flash_deinit();
  nvs_flash_erase();
  // Make the handle invalid to prevent any saves until restart
  this->nvs_handle = 0;
  return true;
}

static ESP32Preferences s_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

ESP32Preferences *get_preferences() { return &s_preferences; }

void setup_preferences() {
  s_preferences.open();
  global_preferences = &s_preferences;
}

}  // namespace esphome::esp32

namespace esphome {
ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome

#endif  // USE_ESP32
