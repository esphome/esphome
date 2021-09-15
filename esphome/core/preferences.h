#pragma once

#include <cstring>
#include <cstdint>
#include <type_traits>

namespace esphome {

class ESPPreferenceBackend {
 public:
  virtual bool save(const uint8_t *data, size_t len) = 0;
  virtual bool load(uint8_t *data, size_t len) = 0;
};

class ESPPreferenceObject {
 public:
  ESPPreferenceObject() = default;
  ESPPreferenceObject(ESPPreferenceBackend *backend) : backend_(backend) {}

  template<typename T> bool save(const T *src) {
    if (backend_ == nullptr)
      return false;
    return backend_->save(reinterpret_cast<const uint8_t *>(src), sizeof(T));
  }

  template<typename T> bool load(T *dest) {
    if (backend_ == nullptr)
      return false;
    return backend_->load(reinterpret_cast<uint8_t *>(dest), sizeof(T));
  }

 protected:
  ESPPreferenceBackend *backend_;
};

class ESPPreferences {
 public:
  virtual ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) = 0;
  virtual ESPPreferenceObject make_preference(size_t length, uint32_t type) = 0;
  template<
     typename T,
     typename std::enable_if<
       std::is_trivially_copyable<T>::value, bool
     >::type = true
  >
  ESPPreferenceObject make_preference(uint32_t type, bool in_flash) {
    return this->make_preference((sizeof(T) + 3) / 4, type, in_flash);
  }
  template<
     typename T,
     typename std::enable_if<
       std::is_trivially_copyable<T>::value, bool
     >::type = true
  >
  ESPPreferenceObject make_preference(uint32_t type) {
    return this->make_preference((sizeof(T) + 3) / 4, type);
  }
};

extern ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome
