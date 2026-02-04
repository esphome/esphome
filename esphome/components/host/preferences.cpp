#ifdef USE_HOST

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace host {
namespace fs = std::filesystem;

static const char *const TAG = "host.preferences";

void HostPreferences::setup_() {
  if (this->setup_complete_)
    return;
  fs::path preferences_root;
#ifdef ESPHOME_HOST_PREFERENCES_PATH
  preferences_root = ESPHOME_HOST_PREFERENCES_PATH;
#else
  const char *home = getenv("HOME");
  if (home == nullptr || *home == '\0') {
    ESP_LOGE(TAG, "HOME is not set and no host preferences path was configured.");
    return;
  }
  preferences_root = fs::path(home) / ".esphome" / "prefs";
#endif
  std::string preferences_root_str = preferences_root.string();
  std::error_code error;
  fs::create_directories(preferences_root, error);
  if (error) {
    ESP_LOGE(TAG, "Failed to create preferences directory '%s': %s", preferences_root_str.c_str(),
             error.message().c_str());
    return;
  }
  std::error_code status_error;
  fs::perms permissions = fs::status(preferences_root, status_error).permissions();
  if (!status_error) {
    fs::perms writable = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
    if ((permissions & writable) == fs::perms::none) {
      ESP_LOGE(TAG, "Preferences directory '%s' is not writable.", preferences_root_str.c_str());
    }
  }
  fs::path preferences_file = preferences_root / (App.get_name() + std::string(".prefs"));
  this->filename_ = preferences_file.string();
  ESP_LOGD(TAG, "Using preferences path: %s", this->filename_.c_str());
  FILE *fp = fopen(this->filename_.c_str(), "rb");
  if (fp == nullptr) {
    ESP_LOGE(TAG, "Failed to open preferences file '%s' for reading: %s", this->filename_.c_str(), strerror(errno));
    this->setup_complete_ = true;
    return;
  }
  while (!feof((fp))) {
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
    this->data[key] = vec;
  }
  fclose(fp);
  this->setup_complete_ = true;
}

bool HostPreferences::sync() {
  this->setup_();
  if (this->filename_.empty()) {
    ESP_LOGE(TAG, "Preferences file path is not set, cannot sync.");
    return false;
  }
  FILE *fp = fopen(this->filename_.c_str(), "wb");
  if (fp == nullptr) {
    ESP_LOGE(TAG, "Failed to open preferences file '%s' for writing: %s", this->filename_.c_str(), strerror(errno));
    return false;
  }
  std::map<uint32_t, std::vector<uint8_t>>::iterator it;

  for (it = this->data.begin(); it != this->data.end(); ++it) {
    fwrite(&it->first, sizeof(uint32_t), 1, fp);
    uint8_t len = it->second.size();
    fwrite(&len, sizeof(len), 1, fp);
    fwrite(it->second.data(), sizeof(uint8_t), it->second.size(), fp);
  }
  fclose(fp);
  return true;
}

bool HostPreferences::reset() {
  host_preferences->data.clear();
  return true;
}

ESPPreferenceObject HostPreferences::make_preference(size_t length, uint32_t type, bool in_flash) {
  auto backend = new HostPreferenceBackend(type);
  return ESPPreferenceObject(backend);
};

static HostPreferences s_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void setup_preferences() {
  host_preferences = &s_preferences;
  global_preferences = &s_preferences;
}

bool HostPreferenceBackend::save(const uint8_t *data, size_t len) {
  return host_preferences->save(this->key_, data, len);
}

bool HostPreferenceBackend::load(uint8_t *data, size_t len) { return host_preferences->load(this->key_, data, len); }

HostPreferences *host_preferences;
}  // namespace host

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome

#endif  // USE_HOST
