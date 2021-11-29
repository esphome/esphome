#ifdef USE_ESP32

#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <nvs_flash.h>
#include <cstring>
#include <vector>
#include <string>

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32.preferences";

struct NVSData {
  std::string key;
  std::vector<uint8_t> data;
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class ESP32PreferenceBackend : public ESPPreferenceBackend {
 public:
  std::string key;
  uint32_t nvs_handle;
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

    size_t actual_len;
    esp_err_t err = nvs_get_blob(nvs_handle, key.c_str(), nullptr, &actual_len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", key.c_str(), esp_err_to_name(err));
      return false;
    }
    if (actual_len != len) {
      ESP_LOGVV(TAG, "NVS length does not match (%u!=%u)", actual_len, len);
      return false;
    }
    err = nvs_get_blob(nvs_handle, key.c_str(), data, &len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", key.c_str(), esp_err_to_name(err));
      return false;
    }
    return true;
  }
};

class ESP32Preferences : public ESPPreferences {
 public:
  uint32_t nvs_handle;
  uint32_t current_offset = 0;

  void open() {
    nvs_flash_init();
    esp_err_t err = nvs_open("esphome", NVS_READWRITE, &nvs_handle);
    if (err == 0)
      return;

    ESP_LOGW(TAG, "nvs_open failed: %s - erasing NVS...", esp_err_to_name(err));
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
    pref->nvs_handle = nvs_handle;
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
      esp_err_t err = nvs_set_blob(nvs_handle, save.key.c_str(), save.data.data(), save.data.size());
      if (err != 0) {
        ESP_LOGV(TAG, "nvs_set_blob('%s', len=%u) failed: %s", save.key.c_str(), save.data.size(),
                 esp_err_to_name(err));
        any_failed = true;
        continue;
      }
      s_pending_save.erase(s_pending_save.begin() + i);
    }

    // note: commit on esp-idf currently is a no-op, nvs_set_blob always writes
    esp_err_t err = nvs_commit(nvs_handle);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_commit() failed: %s", esp_err_to_name(err));
      return false;
    }

    return !any_failed;
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
