#ifdef USE_ESP32

#include "preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <nvs_flash.h>
#include <cstring>
#include <vector>

namespace esphome::esp32 {

static const char *const TAG = "preferences";

struct NVSData {
  uint32_t key;
  SmallInlineBuffer<8> data;  // Most prefs fit in 8 bytes (covers fan, cover, select, etc.)
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// open() runs from app_main() before the logger is initialized, so any failure
// must be deferred until after global_logger is set. This is emitted from the
// first make_preference() call, which runs from the generated setup() after
// log->pre_setup() has run at EARLY_INIT priority.
static esp_err_t s_open_err = ESP_OK;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool ESP32PreferenceBackend::save(const uint8_t *data, size_t len) {
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

  return ESPPreferenceObject(pref);
}

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
