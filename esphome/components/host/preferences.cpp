#ifdef USE_HOST

#include <cstdlib>
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
  this->filename_.clear();

  std::string root_path;
#ifdef ESPHOME_HOST_PREFERENCES_PATH
  root_path = ESPHOME_HOST_PREFERENCES_PATH;
#endif

  if (root_path.empty()) {
    const char *home = getenv("HOME");
    if (home != nullptr) {
      root_path = home;
    }
  }

  if (root_path.empty()) {
    ESP_LOGE(TAG, "No preferences path configured and HOME environment variable not set.");
    return;
  }

  fs::path prefs_dir = fs::path(root_path) / ".esphome" / "prefs";
  ESP_LOGD(TAG, "Using host preferences directory: '%s'", prefs_dir.string().c_str());
  std::error_code ec;
  fs::create_directories(prefs_dir, ec);
  if (ec) {
    ESP_LOGE(TAG, "Failed to create preferences directory '%s': %s", prefs_dir.string().c_str(),
             ec.message().c_str());
    return;
  }

  const auto writable_test = prefs_dir / ".prefs_write_test";
  {
    std::ofstream test_file(writable_test, std::ios::out | std::ios::trunc);
    test_file << "";
    if (!test_file.good()) {
      ESP_LOGE(TAG, "Preferences directory '%s' is not writable; please check permissions.",
               prefs_dir.string().c_str());
      return;
    }
  }
  fs::remove(writable_test, ec);

  this->filename_ = (prefs_dir / (std::string(App.get_name()) + ".prefs")).string();
  FILE *fp = fopen(this->filename_.c_str(), "rb");
  if (fp != nullptr) {
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
  }
  this->setup_complete_ = true;
}

bool HostPreferences::sync() {
  this->setup_();
  if (!this->setup_complete_) {
    ESP_LOGE(TAG, "Preferences not initialized; unable to sync data to disk.");
    return false;
  }
  FILE *fp = fopen(this->filename_.c_str(), "wb");
  if (fp == nullptr) {
    ESP_LOGE(TAG, "Failed to open preferences file '%s' for writing.", this->filename_.c_str());
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

void setup_preferences() {
  auto *pref = new HostPreferences();  // NOLINT(cppcoreguidelines-owning-memory)
  host_preferences = pref;
  global_preferences = pref;
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
