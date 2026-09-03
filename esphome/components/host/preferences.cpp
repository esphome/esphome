#ifdef USE_HOST

#include <filesystem>
#include <fstream>
#include "preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::host {
namespace fs = std::filesystem;

static const char *const TAG = "preferences";

void HostPreferences::setup_() {
  if (this->setup_complete_)
    return;
  const char *prefdir = getenv("ESPHOME_PREFDIR");
  std::string pref_path;
  if (prefdir != nullptr) {
    pref_path = prefdir;
  } else {
    const char *home = getenv("HOME");
    if (home == nullptr) {
      ESP_LOGE(TAG, "ESPHOME_PREFDIR and HOME environment variables not set, unable to save preferences");
      return;
    }
    pref_path = std::string(home) + "/.esphome/prefs";
  }
  std::error_code ec;
  fs::create_directories(pref_path, ec);
  if (ec) {
    ESP_LOGE(TAG, "Failed to create preferences directory: %s (%s)", pref_path.c_str(), ec.message().c_str());
    return;
  }
  this->filename_ = pref_path;
  this->filename_.append("/");
  this->filename_.append(App.get_name());
  this->filename_.append(".prefs");
  FILE *fp = fopen(this->filename_.c_str(), "rb");
  if (fp != nullptr) {
    while (!feof(fp)) {
      uint32_t key;
      uint8_t len;
      if (fread(&key, sizeof(key), 1, fp) != 1)
        break;
      if (fread(&len, sizeof(len), 1, fp) != 1)
        break;
      uint8_t data[len];
      if (fread(data, sizeof(uint8_t), len, fp) != len)
        break;
      std::vector vec(data, data + len);
      this->data_[key] = vec;
    }
    fclose(fp);
  }
  this->setup_complete_ = true;
}

bool HostPreferences::sync() {
  this->setup_();
  if (this->filename_.empty()) {
    ESP_LOGE(TAG, "Preferences filename not set, unable to save preferences");
    return false;
  }
  FILE *fp = fopen(this->filename_.c_str(), "wb");
  if (fp == nullptr) {
    ESP_LOGE(TAG, "Failed to open preferences file for writing: %s", this->filename_.c_str());
    return false;
  }

  for (auto &it : this->data_) {
    fwrite(&it.first, sizeof(uint32_t), 1, fp);
    uint8_t len = it.second.size();
    fwrite(&len, sizeof(len), 1, fp);
    fwrite(it.second.data(), sizeof(uint8_t), it.second.size(), fp);
  }
  fclose(fp);
  return true;
}

bool HostPreferences::reset() {
  host_preferences->data_.clear();
  return true;
}

ESPPreferenceObject HostPreferences::make_preference(size_t length, uint32_t type, bool in_flash) {
  auto *backend = new HostPreferenceBackend(type);
  return ESPPreferenceObject(backend);
};

static HostPreferences s_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

HostPreferences *get_preferences() { return &s_preferences; }

void setup_preferences() {
  host_preferences = &s_preferences;
  global_preferences = &s_preferences;
}

bool HostPreferenceBackend::save(const uint8_t *data, size_t len) const {
  return host_preferences->save(this->key_, data, len);
}

bool HostPreferenceBackend::load(uint8_t *data, size_t len) const {
  return host_preferences->load(this->key_, data, len);
}

HostPreferences *host_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::host

namespace esphome {
ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome

#endif  // USE_HOST
