#include "preferences.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {

static const char *TAG = "preferences";

#ifdef ARDUINO_ARCH_ESP32
#include "nvs.h"
#include "nvs_flash.h"

class ESP32PreferenceObject : public ESPPreferenceObject {
 public:
  ESP32PreferenceObject(size_t offset, size_t length, uint32_t type) : ESPPreferenceObject(offset, length, type){};

 protected:
  friend class ESP32Preferences;

  bool save_internal_() override;
  bool load_internal_() override;
};

class ESP32Preferences : public ESPPreferences {
 public:
  void begin() override;
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash = DEFAULT_IN_FLASH) override;

 protected:
  friend ESP32PreferenceObject;

  uint32_t nvs_handle_;
};

ESP32Preferences global_esp32_preferences;
ESPPreferences &global_preferences = global_esp32_preferences;

bool ESP32PreferenceObject::save_internal_() {
  if (global_esp32_preferences.nvs_handle_ == 0)
    return false;

  char key[32];
  sprintf(key, "%u", this->offset_);
  uint32_t len = (this->length_words_ + 1) * 4;
  esp_err_t err = nvs_set_blob(global_esp32_preferences.nvs_handle_, key, this->data_, len);
  if (err) {
    ESP_LOGV(TAG, "nvs_set_blob('%s', len=%u) failed: %s", key, len, esp_err_to_name(err));
    return false;
  }
  err = nvs_commit(global_esp32_preferences.nvs_handle_);
  if (err) {
    ESP_LOGV(TAG, "nvs_commit('%s', len=%u) failed: %s", key, len, esp_err_to_name(err));
    return false;
  }
  return true;
}
bool ESP32PreferenceObject::load_internal_() {
  if (global_esp32_preferences.nvs_handle_ == 0)
    return false;

  char key[32];
  sprintf(key, "%u", this->offset_);
  uint32_t len = (this->length_words_ + 1) * 4;

  uint32_t actual_len;
  esp_err_t err = nvs_get_blob(global_esp32_preferences.nvs_handle_, key, nullptr, &actual_len);
  if (err) {
    ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", key, esp_err_to_name(err));
    return false;
  }
  if (actual_len != len) {
    ESP_LOGVV(TAG, "NVS length does not match. Assuming key changed (%u!=%u)", actual_len, len);
    return false;
  }
  err = nvs_get_blob(global_esp32_preferences.nvs_handle_, key, this->data_, &len);
  if (err) {
    ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", key, esp_err_to_name(err));
    return false;
  }
  return true;
}

void ESP32Preferences::begin() {
  auto ns = truncate_string(App.get_name(), 15);
  esp_err_t err = nvs_open(ns.c_str(), NVS_READWRITE, &this->nvs_handle_);
  if (err) {
    ESP_LOGW(TAG, "nvs_open failed: %s - erasing NVS...", esp_err_to_name(err));
    nvs_flash_deinit();
    nvs_flash_erase();
    nvs_flash_init();

    err = nvs_open(ns.c_str(), NVS_READWRITE, &this->nvs_handle_);
    if (err) {
      this->nvs_handle_ = 0;
    }
  }
}

ESPPreferenceObject ESP32Preferences::make_preference(size_t length, uint32_t type, bool in_flash) {
  auto pref = ESP32PreferenceObject(this->current_offset_, length, type);
  this->current_offset_++;
  return pref;
}
#endif
}  // namespace esphome
