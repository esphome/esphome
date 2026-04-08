#pragma once
#ifdef USE_RP2040

#include <cstddef>
#include <cstdint>

namespace esphome::rp2040 {

class RP2040PreferenceBackend final {
 public:
  bool save(const uint8_t *data, size_t len);
  bool load(uint8_t *data, size_t len);

  size_t offset = 0;
  uint32_t type = 0;
};

class RP2040Preferences;
RP2040Preferences *get_preferences();

}  // namespace esphome::rp2040

namespace esphome {
using PreferenceBackend = rp2040::RP2040PreferenceBackend;
}  // namespace esphome

#endif  // USE_RP2040
