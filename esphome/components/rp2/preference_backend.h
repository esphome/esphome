#pragma once
#ifdef USE_RP2

#include <cstddef>
#include <cstdint>

namespace esphome::rp2 {

class RP2PreferenceBackend final {
 public:
  bool save(const uint8_t *data, size_t len);
  bool load(uint8_t *data, size_t len);

  size_t offset = 0;
  uint32_t type = 0;
};

class RP2Preferences;
RP2Preferences *get_preferences();

}  // namespace esphome::rp2

namespace esphome {
using PreferenceBackend = rp2::RP2PreferenceBackend;
}  // namespace esphome

#endif  // USE_RP2
