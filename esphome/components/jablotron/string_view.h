#pragma once
#include <stdexcept>
#include <cstring>
#include <string>
#include <limits>

namespace esphome {
namespace jablotron {

class StringView {
 public:
  using size_type = std::size_t;

  StringView();
  StringView(const char *begin);
  StringView(const char *begin, size_type size);
  StringView(const std::string &str);

  const char *data() const noexcept;
  bool empty() const noexcept;
  size_type size() const noexcept;
  operator std::string() const;

  bool starts_with(StringView s) const;
  void remove_prefix(size_type n);
  StringView substr(size_type pos = 0, size_type count = NPOS) const;

  const char &operator[](size_type index) const;

  bool operator==(const StringView &other) const noexcept;

  bool operator!=(const StringView &other) const noexcept;

  bool operator==(const std::string &other) const noexcept;
  bool operator!=(const std::string &other) const noexcept;

  bool operator==(const char *other) const noexcept;
  bool operator!=(const char *other) const noexcept;

  static constexpr size_type NPOS = std::numeric_limits<size_t>::max();

 private:
  const char *data_;
  size_type size_;
};

bool operator==(const std::string &string, const StringView &view) noexcept;
bool operator!=(const std::string &string, const StringView &view) noexcept;

bool get_bit_in_hex_string(StringView str, size_t index);
bool starts_with(StringView str, StringView prefix);

bool try_remove_prefix(StringView &str, StringView prefix);
bool try_remove_prefix(StringView &str, const std::string &prefix);
bool try_remove_prefix(StringView &str, const char *prefix);

bool try_remove_prefix_and_space(StringView &str, StringView prefix);
bool try_remove_prefix_and_space(StringView &str, const std::string &prefix);
bool try_remove_prefix_and_space(StringView &str, const char *prefix);

bool try_remove_integer_and_space(StringView &str, int &result);
}  // namespace jablotron
}  // namespace esphome
