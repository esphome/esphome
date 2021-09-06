#pragma once

#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

namespace esphome {

class Preference {
 public:
  template<typename T> bool save(const T *data) {
    return this->save_(reinterpret_cast<const uint8_t *>(data), sizeof(T));
  }
  template<typename T> bool load(T *data) {
    return this->load_(reinterpret_cast<uint8_t *>(data), sizeof(T));
  }
 protected:
  virtual bool save_(const uint8_t *data, size_t len) = 0;
  virtual bool load_(uint8_t *data, size_t len) = 0;
};

class PreferenceStore {
 public:
  virtual std::unique_ptr<Preference> make_preference(std::string key, uint32_t length) = 0;
  template<
    typename T,
    typename std::enable_if<
      std::is_trivially_copyable<T>::value, bool
    >::type = true
  >
  std::unique_ptr<Preference> make_preference(std::string key) {
    return make_preference(key, sizeof(T));
  }
};

extern PreferenceStore *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome
