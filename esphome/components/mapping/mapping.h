#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include <map>
#include <string>

namespace esphome::mapping {

using alloc_string_t = std::basic_string<char, std::char_traits<char>, RAMAllocator<char>>;

/**
 *
 * Mapping class with custom allocator.
 * Additionally, when std::string is used as key or value, it will be replaced with a custom string type
 * that uses RAMAllocator.
 * @tparam K The type of the key in the mapping.
 * @tparam V The type of the value in the mapping. Should be a basic type or pointer.
 */

static const char *const TAG = "mapping";

template<typename K, typename V> class Mapping {
 public:
  // Constructor
  Mapping() = default;

  using key_t = const std::conditional_t<std::is_same_v<K, std::string>,
                                         alloc_string_t,  // if K is std::string, custom string type
                                         K>;
  using value_t = std::conditional_t<std::is_same_v<V, std::string>,
                                     alloc_string_t,  // if V is std::string, custom string type
                                     V>;

  void set(const K &key, const V &value) { this->map_[key_t{key}] = value; }

  V get(const K &key) const {
    auto it = this->map_.find(key_t{key});
    if (it != this->map_.end()) {
      return V{it->second};
    }
    if constexpr (std::is_pointer_v<K>) {
      esph_log_e(TAG, "Key '%p' not found in mapping", key);
    } else if constexpr (std::is_same_v<K, std::string>) {
      esph_log_e(TAG, "Key '%s' not found in mapping", key.c_str());
    } else if constexpr (std::is_integral_v<K>) {
      char buf[24];  // enough for 64-bit integer
      if constexpr (std::is_unsigned_v<K>) {
        buf_append_printf(buf, sizeof(buf), 0, "%" PRIu64, static_cast<uint64_t>(key));
      } else {
        buf_append_printf(buf, sizeof(buf), 0, "%" PRId64, static_cast<int64_t>(key));
      }
      esph_log_e(TAG, "Key '%s' not found in mapping", buf);
    } else {
      // All supported key types are handled above - this should never be reached
      static_assert(sizeof(K) == 0, "Unsupported key type for Mapping error logging");
    }
    return {};
  }

  // index map overload
  V operator[](K key) { return this->get(key); }

  // convenience function for strings to get a C-style string
  template<typename T = V, std::enable_if_t<std::is_same_v<T, std::string>, int> = 0>
  const char *operator[](K key) const {
    auto it = this->map_.find(key_t{key});
    if (it != this->map_.end()) {
      return it->second.c_str();  // safe since value remains in map
    }
    return "";
  }

 protected:
  std::map<key_t, value_t, std::less<key_t>, RAMAllocator<std::pair<key_t, value_t>>> map_;
};

}  // namespace esphome::mapping
