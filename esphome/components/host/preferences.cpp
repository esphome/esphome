#ifdef USE_HOST

#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <utility>
#include "preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::host {
namespace fs = std::filesystem;

ESPHOME_LOG_TAG(TAG, "preferences");

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
      std::vector<uint8_t> data(len);
      if (fread(data.data(), sizeof(uint8_t), len, fp) != len)
        break;
      this->data_[key] = std::move(data);
    }
    fclose(fp);
  }
  this->committed_ = this->data_;
  this->setup_complete_ = true;
}

bool HostPreferences::sync() {
  this->setup_();
  if (!this->write_file_(this->data_))
    return false;
  this->committed_ = this->data_;
  return true;
}

bool HostPreferences::sync_key(uint32_t key) {
  this->setup_();
  auto it = this->data_.find(key);
  if (it == this->data_.end())
    return true;
  auto cit = this->committed_.find(key);
  if (cit != this->committed_.end() && cit->second == it->second)
    return true;
  auto committed = this->committed_;
  committed[key] = it->second;
  if (!this->write_file_(committed))
    return false;
  this->committed_ = std::move(committed);
  return true;
}

bool HostPreferences::write_file_(const std::map<uint32_t, std::vector<uint8_t>> &data) {
  if (this->filename_.empty()) {
    ESP_LOGE(TAG, "Preferences filename not set, unable to save preferences");
    return false;
  }
  // Written to a temporary file and renamed over the old one, so a crash never leaves a partial file
  const std::string tmp = this->filename_ + ".tmp";
  FILE *fp = fopen(tmp.c_str(), "wb");
  if (fp == nullptr) {
    ESP_LOGE(TAG, "Failed to open preferences file for writing: %s", tmp.c_str());
    return false;
  }

  bool ok = true;
  for (const auto &it : data) {
    uint8_t len = it.second.size();
    ok = ok && fwrite(&it.first, sizeof(uint32_t), 1, fp) == 1 && fwrite(&len, sizeof(len), 1, fp) == 1 &&
         fwrite(it.second.data(), sizeof(uint8_t), len, fp) == len;
  }
  ok = ok && fflush(fp) == 0 && fsync(fileno(fp)) == 0;
  ok = fclose(fp) == 0 && ok;
  if (!ok || rename(tmp.c_str(), this->filename_.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to write preferences file: %s", this->filename_.c_str());
    remove(tmp.c_str());
    return false;
  }
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

bool HostPreferenceBackend::sync() const { return host_preferences->sync_key(this->key_); }

HostPreferences *host_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::host

namespace esphome {
ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome

#endif  // USE_HOST
