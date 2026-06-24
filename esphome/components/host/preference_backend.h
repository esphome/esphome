#pragma once
#ifdef USE_HOST

#include <cstddef>
#include <cstdint>

namespace esphome::host {

class HostPreferenceBackend final {
 public:
  explicit HostPreferenceBackend(uint32_t key) : key_(key) {}

  bool save(const uint8_t *data, size_t len);
  bool load(uint8_t *data, size_t len);

 protected:
  uint32_t key_{};
};

class HostPreferences;
HostPreferences *get_preferences();

}  // namespace esphome::host

namespace esphome {
using PreferenceBackend = host::HostPreferenceBackend;
}  // namespace esphome

#endif  // USE_HOST
