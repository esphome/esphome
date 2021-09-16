#ifdef USE_ESP32

#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <nvs_flash.h>

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32.preferences";

class ESP32PreferenceBackend : public ESPPreferenceBackend {
 public:
  std::string key;
  uint32_t nvs_handle;
  bool save(const uint8_t *data, size_t len) override {
    esp_err_t err = nvs_set_blob(nvs_handle, key.c_str(), data, len);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_set_blob('%s', len=%u) failed: %s", key.c_str(), len, esp_err_to_name(err));
      return false;
    }
    err = nvs_commit(nvs_handle);
    if (err != 0) {
      ESP_LOGV(TAG, "nvs_commit('%s', len=%u) failed: %s", key.c_str(), len, esp_err_to_name(err));
      return false;
    }
    return true;
  }
  bool load(uint8_t *data, size_t len) override {
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

    // ignore length
    return ESPPreferenceObject(pref);
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
