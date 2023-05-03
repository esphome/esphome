#ifdef USE_ESP8266

#include <c_types.h>
extern "C" {
#include "spi_flash.h"
}

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "preferences.h"

#include <cstring>
#include <vector>

namespace esphome {
namespace esp8266 {

static const char *const TAG = "esp8266.preferences";

static bool s_prevent_write = false;         // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static uint32_t *s_flash_storage = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_flash_dirty = false;           // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static const uint32_t ESP_RTC_USER_MEM_START = 0x60001200;
#define ESP_RTC_USER_MEM ((uint32_t *) ESP_RTC_USER_MEM_START)
static const uint32_t ESP_RTC_USER_MEM_SIZE_WORDS = 128;
static const uint32_t ESP_RTC_USER_MEM_SIZE_BYTES = ESP_RTC_USER_MEM_SIZE_WORDS * 4;

#ifdef USE_ESP8266_PREFERENCES_FLASH
static const uint32_t ESP8266_FLASH_STORAGE_SIZE = 128;
#else
static const uint32_t ESP8266_FLASH_STORAGE_SIZE = 64;
#endif

static inline bool esp_rtc_user_mem_read(uint32_t index, uint32_t *dest) {
  if (index >= ESP_RTC_USER_MEM_SIZE_WORDS) {
    return false;
  }
  *dest = ESP_RTC_USER_MEM[index];  // NOLINT(performance-no-int-to-ptr)
  return true;
}

static inline bool esp_rtc_user_mem_write(uint32_t index, uint32_t value) {
  if (index >= ESP_RTC_USER_MEM_SIZE_WORDS) {
    return false;
  }
  if (index < 32 && s_prevent_write) {
    return false;
  }

  auto *ptr = &ESP_RTC_USER_MEM[index];  // NOLINT(performance-no-int-to-ptr)
  *ptr = value;
  return true;
}

extern "C" uint32_t _SPIFFS_end;  // NOLINT

static uint32_t get_esp8266_flash_sector() {
  union {
    uint32_t *ptr;
    uint32_t uint;
  } data{};
  data.ptr = &_SPIFFS_end;
  return (data.uint - 0x40200000) / SPI_FLASH_SEC_SIZE;
}
static uint32_t get_esp8266_flash_address() { return get_esp8266_flash_sector() * SPI_FLASH_SEC_SIZE; }

template<class It> uint32_t calculate_crc(It first, It last, uint32_t type) {
  uint32_t crc = type;
  while (first != last) {
    crc ^= (*first++ * 2654435769UL) >> 1;
  }
  return crc;
}

static bool save_to_flash(size_t offset, const uint32_t *data, size_t len) {
  for (uint32_t i = 0; i < len; i++) {
    uint32_t j = offset + i;
    if (j >= ESP8266_FLASH_STORAGE_SIZE)
      return false;
    uint32_t v = data[i];
    uint32_t *ptr = &s_flash_storage[j];
    if (*ptr != v)
      s_flash_dirty = true;
    *ptr = v;
  }
  return true;
}

static bool load_from_flash(size_t offset, uint32_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    uint32_t j = offset + i;
    if (j >= ESP8266_FLASH_STORAGE_SIZE)
      return false;
    data[i] = s_flash_storage[j];
  }
  return true;
}

static bool save_to_rtc(size_t offset, const uint32_t *data, size_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if (!esp_rtc_user_mem_write(offset + i, data[i]))
      return false;
  }
  return true;
}

static bool load_from_rtc(size_t offset, uint32_t *data, size_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if (!esp_rtc_user_mem_read(offset + i, &data[i]))
      return false;
  }
  return true;
}

class ESP8266PreferenceBackend : public ESPPreferenceBackend {
 public:
  size_t offset = 0;
  uint32_t type = 0;
  bool in_flash = false;
  size_t length_words = 0;

  bool save(const uint8_t *data, size_t len) override {
    if ((len + 3) / 4 != length_words) {
      return false;
    }
    std::vector<uint32_t> buffer;
    buffer.resize(length_words + 1);
    memcpy(buffer.data(), data, len);
    buffer[buffer.size() - 1] = calculate_crc(buffer.begin(), buffer.end() - 1, type);

    if (in_flash) {
      return save_to_flash(offset, buffer.data(), buffer.size());
    } else {
      return save_to_rtc(offset, buffer.data(), buffer.size());
    }
  }
  bool load(uint8_t *data, size_t len) override {
    if ((len + 3) / 4 != length_words) {
      return false;
    }
    std::vector<uint32_t> buffer;
    buffer.resize(length_words + 1);
    bool ret;
    if (in_flash) {
      ret = load_from_flash(offset, buffer.data(), buffer.size());
    } else {
      ret = load_from_rtc(offset, buffer.data(), buffer.size());
    }
    if (!ret)
      return false;

    uint32_t crc = calculate_crc(buffer.begin(), buffer.end() - 1, type);
    if (buffer[buffer.size() - 1] != crc) {
      return false;
    }

    memcpy(data, buffer.data(), len);
    return true;
  }
};

class ESP8266Preferences : public ESPPreferences {
 public:
  uint32_t current_offset = 0;
  uint32_t current_flash_offset = 0;  // in words

  void setup() {
    s_flash_storage = new uint32_t[ESP8266_FLASH_STORAGE_SIZE];  // NOLINT
    ESP_LOGVV(TAG, "Loading preferences from flash...");

    {
      InterruptLock lock;
      spi_flash_read(get_esp8266_flash_address(), s_flash_storage, ESP8266_FLASH_STORAGE_SIZE * 4);
    }
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    uint32_t length_words = (length + 3) / 4;
    if (in_flash) {
      uint32_t start = current_flash_offset;
      uint32_t end = start + length_words + 1;
      if (end > ESP8266_FLASH_STORAGE_SIZE)
        return {};
      auto *pref = new ESP8266PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
      pref->offset = start;
      pref->type = type;
      pref->length_words = length_words;
      pref->in_flash = true;
      current_flash_offset = end;
      return {pref};
    }

    uint32_t start = current_offset;
    uint32_t end = start + length_words + 1;
    bool in_normal = start < 96;
    // Normal: offset 0-95 maps to RTC offset 32 - 127,
    // Eboot: offset 96-127 maps to RTC offset 0 - 31 words
    if (in_normal && end > 96) {
      // start is in normal but end is not -> switch to Eboot
      current_offset = start = 96;
      end = start + length_words + 1;
      in_normal = false;
    }

    if (end > 128) {
      // Doesn't fit in data, return uninitialized preference obj.
      return {};
    }

    uint32_t rtc_offset = in_normal ? start + 32 : start - 96;

    auto *pref = new ESP8266PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    pref->offset = rtc_offset;
    pref->type = type;
    pref->length_words = length_words;
    pref->in_flash = false;
    current_offset += length_words + 1;
    return pref;
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
#ifdef USE_ESP8266_PREFERENCES_FLASH
    return make_preference(length, type, true);
#else
    return make_preference(length, type, false);
#endif
  }

  bool sync() override {
    if (!s_flash_dirty)
      return true;
    if (s_prevent_write)
      return false;

    ESP_LOGD(TAG, "Saving preferences to flash...");
    SpiFlashOpResult erase_res, write_res = SPI_FLASH_RESULT_OK;
    {
      InterruptLock lock;
      erase_res = spi_flash_erase_sector(get_esp8266_flash_sector());
      if (erase_res == SPI_FLASH_RESULT_OK) {
        write_res = spi_flash_write(get_esp8266_flash_address(), s_flash_storage, ESP8266_FLASH_STORAGE_SIZE * 4);
      }
    }
    if (erase_res != SPI_FLASH_RESULT_OK) {
      ESP_LOGE(TAG, "Erase ESP8266 flash failed!");
      return false;
    }
    if (write_res != SPI_FLASH_RESULT_OK) {
      ESP_LOGE(TAG, "Write ESP8266 flash failed!");
      return false;
    }

    s_flash_dirty = false;
    return true;
  }

  bool reset() override {
    ESP_LOGD(TAG, "Cleaning up preferences in flash...");
    SpiFlashOpResult erase_res;
    {
      InterruptLock lock;
      erase_res = spi_flash_erase_sector(get_esp8266_flash_sector());
    }
    if (erase_res != SPI_FLASH_RESULT_OK) {
      ESP_LOGE(TAG, "Erase ESP8266 flash failed!");
      return false;
    }

    // Protect flash from writing till restart
    s_prevent_write = true;
    return true;
  }
};

void setup_preferences() {
  auto *pref = new ESP8266Preferences();  // NOLINT(cppcoreguidelines-owning-memory)
  pref->setup();
  global_preferences = pref;
}
void preferences_prevent_write(bool prevent) { s_prevent_write = prevent; }

}  // namespace esp8266

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_ESP8266
