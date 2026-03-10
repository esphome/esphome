#ifdef USE_RP2040

#include <Arduino.h>

#include <hardware/flash.h>
#include <hardware/sync.h>

#include "preferences.h"

#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace rp2040 {

static const char *const TAG = "rp2040.preferences";

static constexpr uint32_t RP2040_FLASH_STORAGE_SIZE = 512;

static bool s_prevent_write = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static uint8_t
    s_flash_storage[RP2040_FLASH_STORAGE_SIZE];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_flash_dirty = false;               // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// No preference can exceed the total flash storage, so stack buffer covers all cases.
static constexpr size_t PREF_MAX_BUFFER_SIZE = RP2040_FLASH_STORAGE_SIZE;

extern "C" uint8_t _EEPROM_start;

template<class It> uint8_t calculate_crc(It first, It last, uint32_t type) {
  std::array<uint8_t, 4> type_array = decode_value(type);
  uint8_t crc = type_array[0] ^ type_array[1] ^ type_array[2] ^ type_array[3];
  while (first != last) {
    crc ^= (*first++);
  }
  return crc;
}

class RP2040PreferenceBackend : public ESPPreferenceBackend {
 public:
  size_t offset = 0;
  uint32_t type = 0;

  bool save(const uint8_t *data, size_t len) override {
    const size_t buffer_size = len + 1;
    if (buffer_size > PREF_MAX_BUFFER_SIZE)
      return false;
    uint8_t buffer[PREF_MAX_BUFFER_SIZE];
    memcpy(buffer, data, len);
    buffer[len] = calculate_crc(buffer, buffer + len, this->type);

    for (size_t i = 0; i < buffer_size; i++) {
      uint32_t j = this->offset + i;
      if (j >= RP2040_FLASH_STORAGE_SIZE)
        return false;
      uint8_t v = buffer[i];
      uint8_t *ptr = &s_flash_storage[j];
      if (*ptr != v)
        s_flash_dirty = true;
      *ptr = v;
    }
    return true;
  }
  bool load(uint8_t *data, size_t len) override {
    const size_t buffer_size = len + 1;
    if (buffer_size > PREF_MAX_BUFFER_SIZE)
      return false;
    uint8_t buffer[PREF_MAX_BUFFER_SIZE];

    for (size_t i = 0; i < buffer_size; i++) {
      uint32_t j = this->offset + i;
      if (j >= RP2040_FLASH_STORAGE_SIZE)
        return false;
      buffer[i] = s_flash_storage[j];
    }

    uint8_t crc = calculate_crc(buffer, buffer + len, this->type);
    if (buffer[len] != crc) {
      return false;
    }

    memcpy(data, buffer, len);
    return true;
  }
};

class RP2040Preferences : public ESPPreferences {
 public:
  uint32_t current_flash_offset = 0;

  RP2040Preferences() : eeprom_sector_(&_EEPROM_start) {}
  void setup() {
    ESP_LOGVV(TAG, "Loading preferences from flash");
    memcpy(s_flash_storage, this->eeprom_sector_, RP2040_FLASH_STORAGE_SIZE);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    uint32_t start = this->current_flash_offset;
    uint32_t end = start + length + 1;
    if (end > RP2040_FLASH_STORAGE_SIZE) {
      return {};
    }
    auto *pref = new RP2040PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    pref->offset = start;
    pref->type = type;
    current_flash_offset = end;
    return {pref};
  }

  bool sync() override {
    if (!s_flash_dirty)
      return true;
    if (s_prevent_write)
      return false;

    ESP_LOGD(TAG, "Saving");

    {
      InterruptLock lock;
      ::rp2040.idleOtherCore();
      flash_range_erase((intptr_t) eeprom_sector_ - (intptr_t) XIP_BASE, 4096);
      flash_range_program((intptr_t) eeprom_sector_ - (intptr_t) XIP_BASE, s_flash_storage, RP2040_FLASH_STORAGE_SIZE);
      ::rp2040.resumeOtherCore();
    }

    s_flash_dirty = false;
    return true;
  }

  bool reset() override {
    ESP_LOGD(TAG, "Erasing storage");
    {
      InterruptLock lock;
      ::rp2040.idleOtherCore();
      flash_range_erase((intptr_t) eeprom_sector_ - (intptr_t) XIP_BASE, 4096);
      ::rp2040.resumeOtherCore();
    }
    s_prevent_write = true;
    return true;
  }

 protected:
  uint8_t *eeprom_sector_;
};

static RP2040Preferences s_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void setup_preferences() {
  s_preferences.setup();
  global_preferences = &s_preferences;
}
void preferences_prevent_write(bool prevent) { s_prevent_write = prevent; }

}  // namespace rp2040

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_RP2040
