#include "preferences.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {

static const char *TAG = "preferences";

#ifdef ARDUINO_ARCH_ESP8266
extern "C" {
#include "spi_flash.h"
}

static const uint32_t ESP_RTC_USER_MEM_START = 0x60001200;
#define ESP_RTC_USER_MEM ((uint32_t *) ESP_RTC_USER_MEM_START)
static const uint32_t ESP_RTC_USER_MEM_SIZE_WORDS = 128;
static const uint32_t ESP_RTC_USER_MEM_SIZE_BYTES = ESP_RTC_USER_MEM_SIZE_WORDS * 4;

class ESP8266PreferenceObject : public ESPPreferenceObject {
 public:
  ESP8266PreferenceObject(size_t offset, size_t length, uint32_t type) : ESPPreferenceObject(offset, length, type){};

 protected:
  friend class ESP8266Preferences;

  bool save_internal() override;
  bool load_internal() override;
};

class ESP8266Preferences : public ESPPreferences {
 public:
  void begin() override;
  /** On the ESP8266, we can't override the first 128 bytes during OTA uploads
   * as the eboot parameters are stored there. Writing there during an OTA upload
   * would invalidate applying the new firmware. During normal operation, we use
   * this part of the RTC user memory, but stop writing to it during OTA uploads.
   *
   * @param prevent Whether to prevent writing to the first 32 words of RTC user memory.
   */
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override;
  void prevent_write(bool prevent) override;
  bool is_prevent_write() override;

 protected:
  friend ESP8266PreferenceObject;

  void save_esp8266_flash_();
  bool prevent_write_{false};
  uint32_t *flash_storage_;
  uint32_t current_flash_offset_;
};

ESP8266Preferences global_esp8266_preferences;
ESPPreferences &global_preferences = global_esp8266_preferences;

#ifdef USE_ESP8266_PREFERENCES_FLASH
static const uint32_t ESP8266_FLASH_STORAGE_SIZE = 128;
#else
static const uint32_t ESP8266_FLASH_STORAGE_SIZE = 64;
#endif

static inline bool esp_rtc_user_mem_read(uint32_t index, uint32_t *dest) {
  if (index >= ESP_RTC_USER_MEM_SIZE_WORDS) {
    return false;
  }
  *dest = ESP_RTC_USER_MEM[index];
  return true;
}

static bool esp8266_flash_dirty = false;

static inline bool esp_rtc_user_mem_write(uint32_t index, uint32_t value) {
  if (index >= ESP_RTC_USER_MEM_SIZE_WORDS) {
    return false;
  }
  if (index < 32 && global_esp8266_preferences.is_prevent_write()) {
    return false;
  }

  auto *ptr = &ESP_RTC_USER_MEM[index];
  *ptr = value;
  return true;
}

extern "C" uint32_t _SPIFFS_end;

static const uint32_t get_esp8266_flash_sector() {
  union {
    uint32_t *ptr;
    uint32_t uint;
  } data{};
  data.ptr = &_SPIFFS_end;
  return (data.uint - 0x40200000) / SPI_FLASH_SEC_SIZE;
}
static const uint32_t get_esp8266_flash_address() { return get_esp8266_flash_sector() * SPI_FLASH_SEC_SIZE; }

void ESP8266Preferences::save_esp8266_flash_() {
  if (!esp8266_flash_dirty)
    return;

  ESP_LOGVV(TAG, "Saving preferences to flash...");
  SpiFlashOpResult erase_res, write_res = SPI_FLASH_RESULT_OK;
  {
    InterruptLock lock;
    erase_res = spi_flash_erase_sector(get_esp8266_flash_sector());
    if (erase_res == SPI_FLASH_RESULT_OK) {
      write_res = spi_flash_write(get_esp8266_flash_address(), this->flash_storage_, ESP8266_FLASH_STORAGE_SIZE * 4);
    }
  }
  if (erase_res != SPI_FLASH_RESULT_OK) {
    ESP_LOGV(TAG, "Erase ESP8266 flash failed!");
    return;
  }
  if (write_res != SPI_FLASH_RESULT_OK) {
    ESP_LOGV(TAG, "Write ESP8266 flash failed!");
    return;
  }

  esp8266_flash_dirty = false;
}

bool ESP8266PreferenceObject::save_internal() {
  if (this->in_flash_) {
    for (uint32_t i = 0; i <= this->length_words_; i++) {
      uint32_t j = this->offset_ + i;
      if (j >= ESP8266_FLASH_STORAGE_SIZE)
        return false;
      uint32_t v = this->data_[i];
      uint32_t *ptr = &global_esp8266_preferences.flash_storage_[j];
      if (*ptr != v)
        esp8266_flash_dirty = true;
      *ptr = v;
    }
    global_esp8266_preferences.save_esp8266_flash_();
    return true;
  }

  for (uint32_t i = 0; i <= this->length_words_; i++) {
    if (!esp_rtc_user_mem_write(this->offset_ + i, this->data_[i]))
      return false;
  }

  return true;
}
bool ESP8266PreferenceObject::load_internal() {
  if (this->in_flash_) {
    for (uint32_t i = 0; i <= this->length_words_; i++) {
      uint32_t j = this->offset_ + i;
      if (j >= ESP8266_FLASH_STORAGE_SIZE)
        return false;
      this->data_[i] = global_esp8266_preferences.flash_storage_[j];
    }

    return true;
  }

  for (uint32_t i = 0; i <= this->length_words_; i++) {
    if (!esp_rtc_user_mem_read(this->offset_ + i, &this->data_[i]))
      return false;
  }
  return true;
}

void ESP8266Preferences::begin() {
  this->flash_storage_ = new uint32_t[ESP8266_FLASH_STORAGE_SIZE];
  ESP_LOGVV(TAG, "Loading preferences from flash...");

  {
    InterruptLock lock;
    spi_flash_read(get_esp8266_flash_address(), this->flash_storage_, ESP8266_FLASH_STORAGE_SIZE * 4);
  }
}

ESPPreferenceObject ESP8266Preferences::make_preference(size_t length, uint32_t type, bool in_flash) {
  if (in_flash) {
    uint32_t start = this->current_flash_offset_;
    uint32_t end = start + length + 1;
    if (end > ESP8266_FLASH_STORAGE_SIZE)
      return {};
    auto pref = ESP8266PreferenceObject(start, length, type);
    pref.in_flash_ = true;
    this->current_flash_offset_ = end;
    return pref;
  }

  uint32_t start = this->current_offset_;
  uint32_t end = start + length + 1;
  bool in_normal = start < 96;
  // Normal: offset 0-95 maps to RTC offset 32 - 127,
  // Eboot: offset 96-127 maps to RTC offset 0 - 31 words
  if (in_normal && end > 96) {
    // start is in normal but end is not -> switch to Eboot
    this->current_offset_ = start = 96;
    end = start + length + 1;
    in_normal = false;
  }

  if (end > 128) {
    // Doesn't fit in data, return uninitialized preference obj.
    return {};
  }

  uint32_t rtc_offset;
  if (in_normal) {
    rtc_offset = start + 32;
  } else {
    rtc_offset = start - 96;
  }

  auto pref = ESP8266PreferenceObject(rtc_offset, length, type);
  this->current_offset_ += length + 1;
  return pref;
}
void ESP8266Preferences::prevent_write(bool prevent) { this->prevent_write_ = prevent; }
bool ESP8266Preferences::is_prevent_write() { return this->prevent_write_; }
#endif
}  // namespace esphome
