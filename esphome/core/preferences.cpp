#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

#ifdef ARDUINO_ARCH_ESP8266
extern "C" {
#include "spi_flash.h"
}
#endif
#ifdef ARDUINO_ARCH_ESP32
#include "nvs.h"
#include "nvs_flash.h"
#endif

namespace esphome {

static const char *TAG = "preferences";

ESPPreferenceObject::ESPPreferenceObject() : offset_(0), length_words_(0), type_(0), data_(nullptr) {}
ESPPreferenceObject::ESPPreferenceObject(size_t offset, size_t length, uint32_t type)
    : offset_(offset), length_words_(length), type_(type) {
  this->data_ = new uint32_t[this->length_words_ + 1];
  for (uint32_t i = 0; i < this->length_words_ + 1; i++)
    this->data_[i] = 0;
}
bool ESPPreferenceObject::load_() {
  if (!this->is_initialized()) {
    ESP_LOGV(TAG, "Load Pref Not initialized!");
    return false;
  }
  if (!this->load_internal_())
    return false;

  bool valid = this->data_[this->length_words_] == this->calculate_crc_();

  ESP_LOGVV(TAG, "LOAD %u: valid=%s, 0=0x%08X 1=0x%08X (Type=%u, CRC=0x%08X)", this->offset_,  // NOLINT
            YESNO(valid), this->data_[0], this->data_[1], this->type_, this->calculate_crc_());
  return valid;
}
bool ESPPreferenceObject::save_() {
  if (!this->is_initialized()) {
    ESP_LOGV(TAG, "Save Pref Not initialized!");
    return false;
  }

  this->data_[this->length_words_] = this->calculate_crc_();
  if (!this->save_internal_())
    return false;
  ESP_LOGVV(TAG, "SAVE %u: 0=0x%08X 1=0x%08X (Type=%u, CRC=0x%08X)", this->offset_,  // NOLINT
            this->data_[0], this->data_[1], this->type_, this->calculate_crc_());
  return true;
}

#ifdef ARDUINO_ARCH_ESP8266

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
  *dest = ESP_RTC_USER_MEM[index];
  return true;
}

static bool esp8266_flash_dirty = false;

static inline bool esp_rtc_user_mem_write(uint32_t index, uint32_t value) {
  if (index >= ESP_RTC_USER_MEM_SIZE_WORDS) {
    return false;
  }
  if (index < 32 && global_preferences.is_prevent_write()) {
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

void ESPPreferences::save_esp8266_flash_() {
  if (!esp8266_flash_dirty)
    return;

  ESP_LOGVV(TAG, "Saving preferences to flash...");
  disable_interrupts();
  auto erase_res = spi_flash_erase_sector(get_esp8266_flash_sector());
  if (erase_res != SPI_FLASH_RESULT_OK) {
    enable_interrupts();
    ESP_LOGV(TAG, "Erase ESP8266 flash failed!");
    return;
  }

  auto write_res = spi_flash_write(get_esp8266_flash_address(), this->flash_storage_, ESP8266_FLASH_STORAGE_SIZE * 4);
  enable_interrupts();
  if (write_res != SPI_FLASH_RESULT_OK) {
    ESP_LOGV(TAG, "Write ESP8266 flash failed!");
    return;
  }

  esp8266_flash_dirty = false;
}

bool ESPPreferenceObject::save_internal_() {
  if (this->in_flash_) {
    for (uint32_t i = 0; i <= this->length_words_; i++) {
      uint32_t j = this->offset_ + i;
      if (j >= ESP8266_FLASH_STORAGE_SIZE)
        return false;
      uint32_t v = this->data_[i];
      uint32_t *ptr = &global_preferences.flash_storage_[j];
      if (*ptr != v)
        esp8266_flash_dirty = true;
      *ptr = v;
    }
    global_preferences.save_esp8266_flash_();
    return true;
  }

  for (uint32_t i = 0; i <= this->length_words_; i++) {
    if (!esp_rtc_user_mem_write(this->offset_ + i, this->data_[i]))
      return false;
  }

  return true;
}
bool ESPPreferenceObject::load_internal_() {
  if (this->in_flash_) {
    for (uint32_t i = 0; i <= this->length_words_; i++) {
      uint32_t j = this->offset_ + i;
      if (j >= ESP8266_FLASH_STORAGE_SIZE)
        return false;
      this->data_[i] = global_preferences.flash_storage_[j];
    }

    return true;
  }

  for (uint32_t i = 0; i <= this->length_words_; i++) {
    if (!esp_rtc_user_mem_read(this->offset_ + i, &this->data_[i]))
      return false;
  }
  return true;
}
ESPPreferences::ESPPreferences()
    // offset starts from start of user RTC mem (64 words before that are reserved for system),
    // an additional 32 words at the start of user RTC are for eboot (OTA, see eboot_command.h),
    // which will be reset each time OTA occurs
    : current_offset_(0) {}

void ESPPreferences::begin() {
  this->flash_storage_ = new uint32_t[ESP8266_FLASH_STORAGE_SIZE];
  ESP_LOGVV(TAG, "Loading preferences from flash...");
  disable_interrupts();
  spi_flash_read(get_esp8266_flash_address(), this->flash_storage_, ESP8266_FLASH_STORAGE_SIZE * 4);
  enable_interrupts();
}

ESPPreferenceObject ESPPreferences::make_preference(size_t length, uint32_t type, bool in_flash) {
  if (in_flash) {
    uint32_t start = this->current_flash_offset_;
    uint32_t end = start + length + 1;
    if (end > ESP8266_FLASH_STORAGE_SIZE)
      return {};
    auto pref = ESPPreferenceObject(start, length, type);
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

  auto pref = ESPPreferenceObject(rtc_offset, length, type);
  this->current_offset_ += length + 1;
  return pref;
}
void ESPPreferences::prevent_write(bool prevent) { this->prevent_write_ = prevent; }
bool ESPPreferences::is_prevent_write() { return this->prevent_write_; }
#endif

#ifdef ARDUINO_ARCH_ESP32
bool ESPPreferenceObject::save_internal_() {
  if (global_preferences.nvs_handle_ == 0)
    return false;

  char key[32];
  sprintf(key, "%u", this->offset_);
  uint32_t len = (this->length_words_ + 1) * 4;
  esp_err_t err = nvs_set_blob(global_preferences.nvs_handle_, key, this->data_, len);
  if (err) {
    ESP_LOGV(TAG, "nvs_set_blob('%s', len=%u) failed: %s", key, len, esp_err_to_name(err));
    return false;
  }
  err = nvs_commit(global_preferences.nvs_handle_);
  if (err) {
    ESP_LOGV(TAG, "nvs_commit('%s', len=%u) failed: %s", key, len, esp_err_to_name(err));
    return false;
  }
  return true;
}
bool ESPPreferenceObject::load_internal_() {
  if (global_preferences.nvs_handle_ == 0)
    return false;

  char key[32];
  sprintf(key, "%u", this->offset_);
  uint32_t len = (this->length_words_ + 1) * 4;

  uint32_t actual_len;
  esp_err_t err = nvs_get_blob(global_preferences.nvs_handle_, key, nullptr, &actual_len);
  if (err) {
    ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", key, esp_err_to_name(err));
    return false;
  }
  if (actual_len != len) {
    ESP_LOGVV(TAG, "NVS length does not match. Assuming key changed (%u!=%u)", actual_len, len);
    return false;
  }
  err = nvs_get_blob(global_preferences.nvs_handle_, key, this->data_, &len);
  if (err) {
    ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", key, esp_err_to_name(err));
    return false;
  }
  return true;
}
ESPPreferences::ESPPreferences() : current_offset_(0) {}
void ESPPreferences::begin() {
  auto ns = truncate_string(App.get_name(), 15);
  esp_err_t err = nvs_open(ns.c_str(), NVS_READWRITE, &this->nvs_handle_);
  if (err) {
    ESP_LOGW(TAG, "nvs_open failed: %s - erasing NVS...", esp_err_to_name(err));
    nvs_flash_deinit();
    nvs_flash_erase();
    nvs_flash_init();

    err = nvs_open(ns.c_str(), NVS_READWRITE, &this->nvs_handle_);
    if (err) {
      this->nvs_handle_ = 0;
    }
  }
}

ESPPreferenceObject ESPPreferences::make_preference(size_t length, uint32_t type, bool in_flash) {
  auto pref = ESPPreferenceObject(this->current_offset_, length, type);
  this->current_offset_++;
  return pref;
}
#endif
uint32_t ESPPreferenceObject::calculate_crc_() const {
  uint32_t crc = this->type_;
  for (size_t i = 0; i < this->length_words_; i++) {
    crc ^= (this->data_[i] * 2654435769UL) >> 1;
  }
  return crc;
}
bool ESPPreferenceObject::is_initialized() const { return this->data_ != nullptr; }

ESPPreferences global_preferences;

}  // namespace esphome
