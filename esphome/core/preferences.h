#pragma once

#include "esphome/core/esphal.h"
#include "esphome/core/defines.h"

namespace esphome {

class ESPPreferenceObject {
 public:
  ESPPreferenceObject();
  virtual ~ESPPreferenceObject() = default;

  template<typename T> bool save(T *src);

  template<typename T> bool load(T *dest);

  bool is_initialized() const;

 protected:
  ESPPreferenceObject(size_t offset, size_t length, uint32_t type);

  bool save_();
  bool load_();
  virtual bool save_internal() { return false; }
  virtual bool load_internal() { return false; }

  uint32_t calculate_crc_() const;

  size_t offset_;
  size_t length_words_;
  uint32_t type_;
  uint32_t *data_;
  bool in_flash_{false};
};

#ifdef USE_ESP8266_PREFERENCES_FLASH
static bool DEFAULT_IN_FLASH = true;
#else
static bool DEFAULT_IN_FLASH = false;
#endif

class ESPPreferences {
 public:
  ESPPreferences();
  virtual ~ESPPreferences() = default;

  virtual void begin() = 0;
  ESPPreferenceObject make_preference(size_t length, uint32_t type) {
    return this->make_preference(length, type, DEFAULT_IN_FLASH);
  }
  virtual ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) = 0;
  template<typename T> ESPPreferenceObject make_preference(uint32_t type, bool in_flash = DEFAULT_IN_FLASH);

  virtual void prevent_write(bool) {}
  virtual bool is_prevent_write() { return false; }

 protected:
  uint32_t current_offset_;
};

extern ESPPreferences &global_preferences;

template<typename T> ESPPreferenceObject ESPPreferences::make_preference(uint32_t type, bool in_flash) {
  return this->make_preference((sizeof(T) + 3) / 4, type, in_flash);
}

template<typename T> bool ESPPreferenceObject::save(T *src) {
  if (!this->is_initialized())
    return false;
  memset(this->data_, 0, this->length_words_ * 4);
  memcpy(this->data_, src, sizeof(T));
  return this->save_();
}

template<typename T> bool ESPPreferenceObject::load(T *dest) {
  memset(this->data_, 0, this->length_words_ * 4);
  if (!this->load_())
    return false;

  memcpy(dest, this->data_, sizeof(T));
  return true;
}

}  // namespace esphome
