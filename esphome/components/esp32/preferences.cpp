#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include <nvs_flash.h>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <vector>

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32.preferences";

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

class ESP32PreferenceBackend : public ESPPreferenceBackend {
 public:
  uint32_t key;
  uint32_t nvs_handle;
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
};

class ESP32Preferences : public ESPPreferences {
 public:
  uint32_t nvs_handle;

  void open() {
    nvs_flash_init();
    esp_err_t err = nvs_open("esphome", NVS_READWRITE, &nvs_handle);
    if (err == 0)
      return;

    ESP_LOGW(TAG, "nvs_open failed: %s - erasing NVS", esp_err_to_name(err));
    nvs_flash_deinit();
    nvs_flash_erase();
    nvs_flash_init();

    err = nvs_open("esphome", NVS_READWRITE, &nvs_handle);
    if (err != 0) {
      nvs_handle = 0;
    }
  }
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return this->make_preference(length, type);
  }
  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    auto *pref = new ESP32PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    pref->nvs_handle = this->nvs_handle;
    pref->key = type;

    return ESPPreferenceObject(pref);
  }

  bool sync() override {
    if (s_pending_save.empty())
      return true;

    ESP_LOGV(TAG, "Saving %zu items...", s_pending_save.size());
    // goal try write all pending saves even if one fails
    int cached = 0, written = 0, failed = 0;
    esp_err_t last_err = ESP_OK;
    uint32_t last_key = 0;

    // go through vector from back to front (makes erase easier/more efficient)
    for (ssize_t i = s_pending_save.size() - 1; i >= 0; i--) {
      const auto &save = s_pending_save[i];
      char key_str[KEY_BUFFER_SIZE];
      snprintf(key_str, sizeof(key_str), "%" PRIu32, save.key);
      ESP_LOGVV(TAG, "Checking if NVS data %s has changed", key_str);
      if (this->is_changed_(this->nvs_handle, save, key_str)) {
        esp_err_t err = nvs_set_blob(this->nvs_handle, key_str, save.data.get(), save.len);
        ESP_LOGV(TAG, "sync: key: %s, len: %zu", key_str, save.len);
        if (err != 0) {
          ESP_LOGV(TAG, "nvs_set_blob('%s', len=%zu) failed: %s", key_str, save.len, esp_err_to_name(err));
          failed++;
          last_err = err;
          last_key = save.key;
          continue;
        }
        written++;
      } else {
        ESP_LOGV(TAG, "NVS data not changed skipping %" PRIu32 "  len=%zu", save.key, save.len);
        cached++;
      }
      s_pending_save.erase(s_pending_save.begin() + i);
    }
    ESP_LOGD(TAG, "Writing %d items: %d cached, %d written, %d failed", cached + written + failed, cached, written,
             failed);
    if (failed > 0) {
      ESP_LOGE(TAG, "Writing %d items failed. Last error=%s for key=%" PRIu32, failed, esp_err_to_name(last_err),
               last_key);
    }

    // note: commit on esp-idf currently is a no-op, nvs_set_blob always writes
    esp_err_t err = nvs_commit(this->nvs_handle);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_commit() failed: %s", esp_err_to_name(err));
      return false;
    }

    return failed == 0;
  }

 protected:
  bool is_changed_(uint32_t nvs_handle, const NVSData &to_save, const char *key_str) {
    size_t actual_len;
    esp_err_t err = nvs_get_blob(nvs_handle, key_str, nullptr, &actual_len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", key_str, esp_err_to_name(err));
      return true;
    }
    // Check size first before allocating memory
    if (actual_len != to_save.len) {
      return true;
    }
    auto stored_data = std::make_unique<uint8_t[]>(actual_len);
    err = nvs_get_blob(nvs_handle, key_str, stored_data.get(), &actual_len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", key_str, esp_err_to_name(err));
      return true;
    }
    return memcmp(to_save.data.get(), stored_data.get(), to_save.len) != 0;
  }

  bool reset() override {
    ESP_LOGD(TAG, "Erasing storage");
    s_pending_save.clear();

    nvs_flash_deinit();
    nvs_flash_erase();
    // Make the handle invalid to prevent any saves until restart
    nvs_handle = 0;
    return true;
  }
};

void setup_preferences() {
  auto *prefs = new ESP32Preferences();  // NOLINT(cppcoreguidelines-owning-memory)
  prefs->open();
  global_preferences = prefs;
}

}  // namespace esp32

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_ESP32
