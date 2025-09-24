#pragma once

#ifdef USE_HOST

#include "esphome/core/preferences.h"
#include <map>

namespace esphome {
namespace host {

class HostPreferenceBackend : public ESPPreferenceBackend {
 public:
  explicit HostPreferenceBackend(uint32_t key) { this->key_ = key; }

  bool save(const uint8_t *data, size_t len) override;
  bool load(uint8_t *data, size_t len) override;

 protected:
  uint32_t key_{};
};

class HostPreferences : public ESPPreferences {
 public:
  bool sync() override;
  bool reset() override;

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override;
  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    return make_preference(length, type, false);
  }

  bool save(uint32_t key, const uint8_t *data, size_t len) {
    if (len > 255)
      return false;
    this->setup_();
    std::vector vec(data, data + len);
    this->data[key] = vec;
    return true;
  }

  bool load(uint32_t key, uint8_t *data, size_t len) {
    if (len > 255)
      return false;
    this->setup_();
    if (this->data.count(key) == 0)
      return false;
    auto vec = this->data[key];
    if (vec.size() != len)
      return false;
    memcpy(data, vec.data(), len);
    return true;
  }

 protected:
  void setup_();
  bool setup_complete_{};
  std::string filename_{};
  std::map<uint32_t, std::vector<uint8_t>> data{};
};
void setup_preferences();
extern HostPreferences *host_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace host
}  // namespace esphome

#endif  // USE_HOST
