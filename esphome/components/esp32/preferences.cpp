#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include <nvs_flash.h>
#include <charconv>
#include <cstring>
#include <memory>
#include <vector>

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32.preferences";

// Max uint32_t is "4294967295" (10 chars) + null terminator + 1 padding
static constexpr size_t PREF_KEY_SIZE = 12;

struct NVSData {
  char key[PREF_KEY_SIZE];
  std::unique_ptr<uint8_t[]> data;
  size_t len;

  void set_key(const char *k) {
    strncpy(this->key, k, sizeof(this->key) - 1);
    this->key[sizeof(this->key) - 1] = '\0';
  }
  void set_data(const uint8_t *src, size_t size) {
    this->data = std::make_unique<uint8_t[]>(size);
    memcpy(this->data.get(), src, size);
    this->len = size;
  }
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class ESP32PreferenceBackend : public ESPPreferenceBackend {
 public:
  char key[PREF_KEY_SIZE];
  uint32_t nvs_handle;
  bool save(const uint8_t *data, size_t len) override {
    // try find in pending saves and update that
    for (auto &obj : s_pending_save) {
      if (strcmp(obj.key, this->key) == 0) {
        obj.set_data(data, len);
        return true;
      }
    }
    NVSData save{};
    save.set_key(this->key);
    save.set_data(data, len);
    s_pending_save.emplace_back(std::move(save));
    ESP_LOGVV(TAG, "s_pending_save: key: %s, len: %zu", this->key, len);
    return true;
  }
  bool load(uint8_t *data, size_t len) override {
    // try find in pending saves and load from that
    for (auto &obj : s_pending_save) {
      if (strcmp(obj.key, this->key) == 0) {
        if (obj.len != len) {
          // size mismatch
          return false;
        }
        memcpy(data, obj.data.get(), len);
        return true;
      }
    }

    size_t actual_len;
    esp_err_t err = nvs_get_blob(this->nvs_handle, this->key, nullptr, &actual_len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", this->key, esp_err_to_name(err));
      return false;
    }
    if (actual_len != len) {
      ESP_LOGVV(TAG, "NVS length does not match (%zu!=%zu)", actual_len, len);
      return false;
    }
    err = nvs_get_blob(this->nvs_handle, this->key, data, &len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", this->key, esp_err_to_name(err));
      return false;
    } else {
      ESP_LOGVV(TAG, "nvs_get_blob: key: %s, len: %zu", this->key, len);
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
    return make_preference(length, type);
  }
  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    auto *pref = new ESP32PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    pref->nvs_handle = this->nvs_handle;

    auto [ptr, ec] = std::to_chars(pref->key, pref->key + sizeof(pref->key), type);
    *ptr = '\0';

    return ESPPreferenceObject(pref);
  }

  bool sync() override {
    if (s_pending_save.empty())
      return true;

    ESP_LOGV(TAG, "Saving %zu items...", s_pending_save.size());
    // goal try write all pending saves even if one fails
    int cached = 0, written = 0, failed = 0;
    esp_err_t last_err = ESP_OK;
    char last_key[PREF_KEY_SIZE] = {};

    // go through vector from back to front (makes erase easier/more efficient)
    for (ssize_t i = s_pending_save.size() - 1; i >= 0; i--) {
      const auto &save = s_pending_save[i];
      ESP_LOGVV(TAG, "Checking if NVS data %s has changed", save.key);
      if (this->is_changed(this->nvs_handle, save)) {
        esp_err_t err = nvs_set_blob(this->nvs_handle, save.key, save.data.get(), save.len);
        ESP_LOGV(TAG, "sync: key: %s, len: %zu", save.key, save.len);
        if (err != 0) {
          ESP_LOGV(TAG, "nvs_set_blob('%s', len=%zu) failed: %s", save.key, save.len, esp_err_to_name(err));
          failed++;
          last_err = err;
          strncpy(last_key, save.key, sizeof(last_key) - 1);
          continue;
        }
        written++;
      } else {
        ESP_LOGV(TAG, "NVS data not changed skipping %s  len=%zu", save.key, save.len);
        cached++;
      }
      s_pending_save.erase(s_pending_save.begin() + i);
    }
    ESP_LOGD(TAG, "Writing %d items: %d cached, %d written, %d failed", cached + written + failed, cached, written,
             failed);
    if (failed > 0) {
      ESP_LOGE(TAG, "Writing %d items failed. Last error=%s for key=%s", failed, esp_err_to_name(last_err), last_key);
    }

    // note: commit on esp-idf currently is a no-op, nvs_set_blob always writes
    esp_err_t err = nvs_commit(this->nvs_handle);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_commit() failed: %s", esp_err_to_name(err));
      return false;
    }

    return failed == 0;
  }
  bool is_changed(const uint32_t nvs_handle, const NVSData &to_save) {
    size_t actual_len;
    esp_err_t err = nvs_get_blob(nvs_handle, to_save.key, nullptr, &actual_len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", to_save.key, esp_err_to_name(err));
      return true;
    }
    // Check size first before allocating memory
    if (actual_len != to_save.len) {
      return true;
    }
    auto stored_data = std::make_unique<uint8_t[]>(actual_len);
    err = nvs_get_blob(nvs_handle, to_save.key, stored_data.get(), &actual_len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", to_save.key, esp_err_to_name(err));
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
