#pragma once
#ifdef USE_ZEPHYR
#ifdef CONFIG_SETTINGS

#include <cstddef>
#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <vector>

namespace esphome::zephyr {

static constexpr const char *ESPHOME_SETTINGS_KEY = "esphome";

// Buffer size for key: "esphome/" (8) + max hex uint32 (8) + null terminator (1) = 17; use 20 for safety margin
static constexpr size_t KEY_BUFFER_SIZE = 20;

class ZephyrPreferenceBackend final {
 public:
  explicit ZephyrPreferenceBackend(uint32_t type) : type_(type) {}
  ZephyrPreferenceBackend(uint32_t type, std::vector<uint8_t> &&data) : data(std::move(data)), type_(type) {}

  bool save(const uint8_t *data, size_t len);
  bool load(uint8_t *data, size_t len);

  uint32_t get_type() const { return this->type_; }
  void format_key(char *buf, size_t size) const {
    snprintf(buf, size, "%s/%" PRIx32, ESPHOME_SETTINGS_KEY, this->type_);
  }

  std::vector<uint8_t> data;

 protected:
  uint32_t type_ = 0;
};

class ZephyrPreferences;
ZephyrPreferences *get_preferences();

}  // namespace esphome::zephyr

namespace esphome {
using PreferenceBackend = zephyr::ZephyrPreferenceBackend;
}  // namespace esphome

#endif  // CONFIG_SETTINGS
#endif  // USE_ZEPHYR
