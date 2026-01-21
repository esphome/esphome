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
#include <memory>

namespace esphome::esp8266 {

static const char *const TAG = "esp8266.preferences";

static uint32_t *s_flash_storage = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_prevent_write = false;         // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_flash_dirty = false;           // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static constexpr uint32_t ESP_RTC_USER_MEM_START = 0x60001200;
static constexpr uint32_t ESP_RTC_USER_MEM_SIZE_WORDS = 128;
static constexpr uint32_t ESP_RTC_USER_MEM_SIZE_BYTES = ESP_RTC_USER_MEM_SIZE_WORDS * 4;

// RTC memory layout for preferences:
// - Eboot region: RTC words 0-31 (reserved, mapped from preference offset 96-127)
// - Normal region: RTC words 32-127 (mapped from preference offset 0-95)
static constexpr uint32_t RTC_EBOOT_REGION_WORDS = 32;   // Words 0-31 reserved for eboot
static constexpr uint32_t RTC_NORMAL_REGION_WORDS = 96;  // Words 32-127 for normal prefs
static constexpr uint32_t PREF_TOTAL_WORDS = RTC_EBOOT_REGION_WORDS + RTC_NORMAL_REGION_WORDS;  // 128

// Maximum preference size in words (limited by uint8_t length_words field)
static constexpr uint32_t MAX_PREFERENCE_WORDS = 255;

#define ESP_RTC_USER_MEM ((uint32_t *) ESP_RTC_USER_MEM_START)

#ifdef USE_ESP8266_PREFERENCES_FLASH
static constexpr uint32_t ESP8266_FLASH_STORAGE_SIZE = 128;
#else
static constexpr uint32_t ESP8266_FLASH_STORAGE_SIZE = 64;
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

static inline size_t bytes_to_words(size_t bytes) { return (bytes + 3) / 4; }

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

// Stack buffer size - 16 words total: up to 15 words of preference data + 1 word CRC (60 bytes of preference data)
// This handles virtually all real-world preferences without heap allocation
static constexpr size_t PREF_BUFFER_WORDS = 16;

class ESP8266PreferenceBackend : public ESPPreferenceBackend {
 public:
  uint32_t type = 0;
  uint16_t offset = 0;
  uint8_t length_words = 0;  // Max 255 words (1020 bytes of data)
  bool in_flash = false;

  bool save(const uint8_t *data, size_t len) override {
    if (bytes_to_words(len) != this->length_words)
      return false;

    const size_t buffer_size = static_cast<size_t>(this->length_words) + 1;
    uint32_t stack_buffer[PREF_BUFFER_WORDS];
    std::unique_ptr<uint32_t[]> heap_buffer;
    uint32_t *buffer;

    if (buffer_size <= PREF_BUFFER_WORDS) {
      buffer = stack_buffer;
    } else {
      heap_buffer = make_unique<uint32_t[]>(buffer_size);
      buffer = heap_buffer.get();
    }
    memset(buffer, 0, buffer_size * sizeof(uint32_t));

    memcpy(buffer, data, len);
    buffer[this->length_words] = calculate_crc(buffer, buffer + this->length_words, this->type);

    return this->in_flash ? save_to_flash(this->offset, buffer, buffer_size)
                          : save_to_rtc(this->offset, buffer, buffer_size);
  }

  bool load(uint8_t *data, size_t len) override {
    if (bytes_to_words(len) != this->length_words)
      return false;

    const size_t buffer_size = static_cast<size_t>(this->length_words) + 1;
    uint32_t stack_buffer[PREF_BUFFER_WORDS];
    std::unique_ptr<uint32_t[]> heap_buffer;
    uint32_t *buffer;

    if (buffer_size <= PREF_BUFFER_WORDS) {
      buffer = stack_buffer;
    } else {
      heap_buffer = make_unique<uint32_t[]>(buffer_size);
      buffer = heap_buffer.get();
    }

    bool ret = this->in_flash ? load_from_flash(this->offset, buffer, buffer_size)
                              : load_from_rtc(this->offset, buffer, buffer_size);
    if (!ret)
      return false;

    if (buffer[this->length_words] != calculate_crc(buffer, buffer + this->length_words, this->type))
      return false;

    memcpy(data, buffer, len);
    return true;
  }
};

class ESP8266Preferences : public ESPPreferences {
 public:
  uint32_t current_offset = 0;
  uint32_t current_flash_offset = 0;  // in words

  void setup() {
    s_flash_storage = new uint32_t[ESP8266_FLASH_STORAGE_SIZE];  // NOLINT
    ESP_LOGVV(TAG, "Loading preferences from flash");

    {
      InterruptLock lock;
      spi_flash_read(get_esp8266_flash_address(), s_flash_storage, ESP8266_FLASH_STORAGE_SIZE * 4);
    }
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    const uint32_t length_words = bytes_to_words(length);
    if (length_words > MAX_PREFERENCE_WORDS) {
      ESP_LOGE(TAG, "Preference too large: %u words", static_cast<unsigned int>(length_words));
      return {};
    }

    const uint32_t total_words = length_words + 1;  // +1 for CRC
    uint16_t offset;

    if (in_flash) {
      if (this->current_flash_offset + total_words > ESP8266_FLASH_STORAGE_SIZE)
        return {};
      offset = static_cast<uint16_t>(this->current_flash_offset);
      this->current_flash_offset += total_words;
    } else {
      uint32_t start = this->current_offset;
      bool in_normal = start < RTC_NORMAL_REGION_WORDS;
      // Normal: offset 0-95 maps to RTC offset 32-127
      // Eboot: offset 96-127 maps to RTC offset 0-31
      if (in_normal && start + total_words > RTC_NORMAL_REGION_WORDS) {
        // start is in normal but end is not -> switch to Eboot
        this->current_offset = start = RTC_NORMAL_REGION_WORDS;
        in_normal = false;
      }
      if (start + total_words > PREF_TOTAL_WORDS)
        return {};  // Doesn't fit in RTC memory
      // Convert preference offset to RTC memory offset
      offset = static_cast<uint16_t>(in_normal ? start + RTC_EBOOT_REGION_WORDS : start - RTC_NORMAL_REGION_WORDS);
      this->current_offset = start + total_words;
    }

    auto *pref = new ESP8266PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    pref->offset = offset;
    pref->type = type;
    pref->length_words = static_cast<uint8_t>(length_words);
    pref->in_flash = in_flash;
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

    ESP_LOGD(TAG, "Saving");
    SpiFlashOpResult erase_res, write_res = SPI_FLASH_RESULT_OK;
    {
      InterruptLock lock;
      erase_res = spi_flash_erase_sector(get_esp8266_flash_sector());
      if (erase_res == SPI_FLASH_RESULT_OK) {
        write_res = spi_flash_write(get_esp8266_flash_address(), s_flash_storage, ESP8266_FLASH_STORAGE_SIZE * 4);
      }
    }
    if (erase_res != SPI_FLASH_RESULT_OK) {
      ESP_LOGE(TAG, "Erasing failed");
      return false;
    }
    if (write_res != SPI_FLASH_RESULT_OK) {
      ESP_LOGE(TAG, "Writing failed");
      return false;
    }

    s_flash_dirty = false;
    return true;
  }

  bool reset() override {
    ESP_LOGD(TAG, "Erasing storage");
    SpiFlashOpResult erase_res;
    {
      InterruptLock lock;
      erase_res = spi_flash_erase_sector(get_esp8266_flash_sector());
    }
    if (erase_res != SPI_FLASH_RESULT_OK) {
      ESP_LOGE(TAG, "Erasing failed");
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

}  // namespace esphome::esp8266

namespace esphome {
ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome

#endif  // USE_ESP8266
