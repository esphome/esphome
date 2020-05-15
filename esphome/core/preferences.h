#pragma once

#include <string>

#include "esphome/core/esphal.h"
#include "esphome/core/defines.h"

namespace esphome {

enum RestoreMode {
    RESTORE_ALWAYS_INITIAL_VALUE,
    RESTORE_DEFAULT,
    RESTORE_FROM_FLASH,
};

#define LOG_STATEFUL_COMPONENT(this) \
  const char *restore_mode = ""; \
  switch (this->restore_mode_) { \
    case RESTORE_ALWAYS_INITIAL_VALUE: \
      restore_mode = "Always restore initial value"; \
      break; \
    case RESTORE_DEFAULT: \
      restore_mode = "Restore using default state storage (always flash on esp32 and \\
        esp8266_restore_from_flash mode for esp8266"; \
      break; \
    case RESTORE_FROM_FLASH: \
      restore_mode = "Always restore from flash"; \
      break; \
    case esphome::switch_::SWITCH_ALWAYS_ON: \
      restore_mode = "Always ON"; \
      break; \
  } \
  ESP_LOGCONFIG(TAG, "  Restore Mode: %s", restore_mode); \

class ESPPreferenceObject {
 public:
  ESPPreferenceObject();
  ESPPreferenceObject(size_t offset, size_t length, uint32_t type);

  template<typename T> bool save(const T *src);

  template<typename T> bool load(T *dest);

  bool is_initialized() const;

 protected:
  friend class ESPPreferences;

  bool save_();
  bool load_();
  bool save_internal_();
  bool load_internal_();

  uint32_t calculate_crc_() const;

  size_t offset_;
  size_t length_words_;
  uint32_t type_;
  uint32_t *data_;
#ifdef ARDUINO_ARCH_ESP8266
  bool in_flash_{false};
#endif
};

#ifdef ARDUINO_ARCH_ESP8266
#ifdef USE_ESP8266_PREFERENCES_FLASH
static bool DEFAULT_IN_FLASH = true;
#else
static bool DEFAULT_IN_FLASH = false;
#endif
#endif

#ifdef ARDUINO_ARCH_ESP32
static bool DEFAULT_IN_FLASH = true;
#endif

template<typename T>
class TypedESPPreferenceObject : public ESPPreferenceObject {
 public:
  TypedESPPreferenceObject() {}
  TypedESPPreferenceObject(ESPPreferenceObject&& base) : ESPPreferenceObject(base) {}
  void save(const T& value);
  T&& load();
 protected:
  friend class ESPPreferences;
  // TODO: it's so much easier to follow the code when we make a copy of the initial value rather
  // than immediately storing it in the base class data_. But this does double the memory usage, so
  // how concerned with memory are we? (especially with the additional copy below?)
  T initial_state_;
  RestoreMode restore_mode_ = RESTORE_DEFAULT;
};

class ESPPreferences {
 public:
  ESPPreferences();
  void begin();
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash = DEFAULT_IN_FLASH);
  template<typename T> ESPPreferenceObject make_preference(uint32_t type, bool in_flash = DEFAULT_IN_FLASH);
  template<typename T> TypedESPPreferenceObject<T> make_typed_preference(
    uint32_t type,
    RestoreMode restore_mode,
    T&& initial_value);

#ifdef ARDUINO_ARCH_ESP8266
  /** On the ESP8266, we can't override the first 128 bytes during OTA uploads
   * as the eboot parameters are stored there. Writing there during an OTA upload
   * would invalidate applying the new firmware. During normal operation, we use
   * this part of the RTC user memory, but stop writing to it during OTA uploads.
   *
   * @param prevent Whether to prevent writing to the first 32 words of RTC user memory.
   */
  void prevent_write(bool prevent);
  bool is_prevent_write();
#endif

 protected:
  friend ESPPreferenceObject;

  uint32_t current_offset_;
#ifdef ARDUINO_ARCH_ESP32
  uint32_t nvs_handle_;
#endif
#ifdef ARDUINO_ARCH_ESP8266
  void save_esp8266_flash_();
  bool prevent_write_{false};
  uint32_t *flash_storage_;
  uint32_t current_flash_offset_;
#endif
};

extern ESPPreferences global_preferences;

template<typename T> ESPPreferenceObject ESPPreferences::make_preference(uint32_t type, bool in_flash) {
  return this->make_preference((sizeof(T) + 3) / 4, type, in_flash);
}

template<typename T> TypedESPPreferenceObject<T> ESPPreferences::make_typed_preference(
  uint32_t type,
  RestoreMode restore_mode,
  T&& initial_value) {

  bool in_flash = DEFAULT_IN_FLASH;
  if (restore_mode == RESTORE_ALWAYS_INITIAL_VALUE)
    in_flash = false;
  else if (restore_mode == RESTORE_FROM_FLASH)
    in_flash = true;

  TypedESPPreferenceObject<T> result = TypedESPPreferenceObject<T>(this->make_preference<T>(type, in_flash));

  result.restore_mode_ = restore_mode;
  result.initial_state_ = initial_value;

  return result;
}

template<typename T> bool ESPPreferenceObject::save(const T *src) {
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

template<typename T> void TypedESPPreferenceObject<T>::save(const T& value) {
  // Store the new state if we couldn't load the old state, or it's a new value and we're not always
  // just restoring the initial value anyway. It's very important we don't store the state if it's
  // the same as the old state because there are a limited number of writes to the esp8266 flash.
  T current_state;
  if ((this->restore_mode_ != RESTORE_ALWAYS_INITIAL_VALUE) &&
    (!(ESPPreferenceObject::template load<T>(&current_state)) || (value != current_state))) {
    ESPPreferenceObject::template save<T>(&value);
  }
}

template<typename T> T&& TypedESPPreferenceObject<T>::load() {
  T result = this->initial_state_;

  if (this->restore_mode_ != RESTORE_ALWAYS_INITIAL_VALUE)
    ESPPreferenceObject::template load<T>(&result);

  return std::move(result);
}

}  // namespace esphome
