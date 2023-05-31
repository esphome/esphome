#pragma once

#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include "esphome/core/defines.h"

#ifdef USE_JSON
#include "esphome/components/json/json_util.h"
#endif  // USE_JSON

namespace esphome {

/**
 * StringRef is a reference to a string owned by something else.  So it behaves like simple string, but it does not own
 * pointer.  When it is default constructed, it has empty string.  You can freely copy or move around this struct, but
 * never free its pointer.  str() function can be used to export the content as std::string. StringRef is adopted from
 * <https://github.com/nghttp2/nghttp2/blob/29cbf8b83ff78faf405d1086b16adc09a8772eca/src/template.h#L376>
 */
class StringRef {
 public:
  using traits_type = std::char_traits<char>;
  using value_type = traits_type::char_type;
  using allocator_type = std::allocator<char>;
  using size_type = std::allocator_traits<allocator_type>::size_type;
  using difference_type = std::allocator_traits<allocator_type>::difference_type;
  using const_reference = const value_type &;
  using const_pointer = const value_type *;
  using const_iterator = const_pointer;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr StringRef() : base_(""), len_(0) {}
  explicit StringRef(const std::string &s) : base_(s.c_str()), len_(s.size()) {}
  explicit StringRef(const char *s) : base_(s), len_(strlen(s)) {}
  constexpr StringRef(const char *s, size_t n) : base_(s), len_(n) {}
  template<typename CharT>
  constexpr StringRef(const CharT *s, size_t n) : base_(reinterpret_cast<const char *>(s)), len_(n) {}
  template<typename InputIt>
  StringRef(InputIt first, InputIt last)
      : base_(reinterpret_cast<const char *>(&*first)), len_(std::distance(first, last)) {}
  template<typename InputIt>
  StringRef(InputIt *first, InputIt *last)
      : base_(reinterpret_cast<const char *>(first)), len_(std::distance(first, last)) {}
  template<typename CharT, size_t N> constexpr static StringRef from_lit(const CharT (&s)[N]) {
    return StringRef{s, N - 1};
  }
  static StringRef from_maybe_nullptr(const char *s) {
    if (s == nullptr) {
      return StringRef();
    }

    return StringRef(s);
  }

  constexpr const_iterator begin() const { return base_; };
  constexpr const_iterator cbegin() const { return base_; };

  constexpr const_iterator end() const { return base_ + len_; };
  constexpr const_iterator cend() const { return base_ + len_; };

  const_reverse_iterator rbegin() const { return const_reverse_iterator{base_ + len_}; }
  const_reverse_iterator crbegin() const { return const_reverse_iterator{base_ + len_}; }

  const_reverse_iterator rend() const { return const_reverse_iterator{base_}; }
  const_reverse_iterator crend() const { return const_reverse_iterator{base_}; }

  constexpr const char *c_str() const { return base_; }
  constexpr size_type size() const { return len_; }
  constexpr bool empty() const { return len_ == 0; }
  constexpr const_reference operator[](size_type pos) const { return *(base_ + pos); }

  std::string str() const { return std::string(base_, len_); }
  const uint8_t *byte() const { return reinterpret_cast<const uint8_t *>(base_); }

  operator std::string() const { return str(); }

 private:
  const char *base_;
  size_type len_;
};

inline bool operator==(const StringRef &lhs, const StringRef &rhs) {
  return lhs.size() == rhs.size() && std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs));
}

inline bool operator==(const StringRef &lhs, const std::string &rhs) {
  return lhs.size() == rhs.size() && std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs));
}

inline bool operator==(const std::string &lhs, const StringRef &rhs) { return rhs == lhs; }

inline bool operator==(const StringRef &lhs, const char *rhs) {
  return lhs.size() == strlen(rhs) && std::equal(std::begin(lhs), std::end(lhs), rhs);
}

inline bool operator==(const char *lhs, const StringRef &rhs) { return rhs == lhs; }

inline bool operator!=(const StringRef &lhs, const StringRef &rhs) { return !(lhs == rhs); }

inline bool operator!=(const StringRef &lhs, const std::string &rhs) { return !(lhs == rhs); }

inline bool operator!=(const std::string &lhs, const StringRef &rhs) { return !(rhs == lhs); }

inline bool operator!=(const StringRef &lhs, const char *rhs) { return !(lhs == rhs); }

inline bool operator!=(const char *lhs, const StringRef &rhs) { return !(rhs == lhs); }

inline bool operator<(const StringRef &lhs, const StringRef &rhs) {
  return std::lexicographical_compare(std::begin(lhs), std::end(lhs), std::begin(rhs), std::end(rhs));
}

inline std::string &operator+=(std::string &lhs, const StringRef &rhs) {
  lhs.append(rhs.c_str(), rhs.size());
  return lhs;
}

inline std::string operator+(const char *lhs, const StringRef &rhs) {
  auto str = std::string(lhs);
  str.append(rhs.c_str(), rhs.size());
  return str;
}

inline std::string operator+(const StringRef &lhs, const char *rhs) {
  auto str = lhs.str();
  str.append(rhs);
  return str;
}

#ifdef USE_JSON
// NOLINTNEXTLINE(readability-identifier-naming)
void convertToJson(const StringRef &src, JsonVariant dst);
#endif  // USE_JSON

}  // namespace esphome
