#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <nvs_flash.h>

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32.preferences";

class ESP32Preference : public Preference {
 public:
  uint32_t nvs_handle;
  std::string key;

 protected:
  bool save_(const uint8_t *data, size_t len) {
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
  bool load_(uint8_t *data, size_t len) {
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

class ESP32PreferenceStore : public PreferenceStore {
 public:
  uint32_t nvs_handle = 0;

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
  std::unique_ptr<Preference> make_preference(std::string key, uint32_t length) override {
    auto *pref = new ESP32Preference();
    pref->nvs_handle = nvs_handle;

    // max ESP32 keylen is 16 bytes, but we need to accept longer keys as well
    // so instead use a hash of the key (fnv1 is good enough, very unlikely to get collisions
    // unless you're intentionally trying to)
    uint32_t keyval = fnv1_hash(key);
    char keybuf[16];
    snprintf(keybuf, sizeof(keybuf), "%d", keyval);
    pref->key = keybuf;  // copied to std::string

    // ignore length
    return std::unique_ptr<Preference>{pref};
  }
};

void setup_preferences() {
  auto *prefs = new ESP32PreferenceStore();
  prefs->open();
  global_preferences = prefs;
}

}  // namespace esp32

PreferenceStore *global_preferences;

}  // namespace esphome
