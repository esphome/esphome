#pragma once
//
// Copyright (c) 2017 Martin Moene
//
// https://github.com/martinmoene/optional-bare
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Modified by Otto Winter on 18.05.18

#include <algorithm>

namespace esphome {

// type for nullopt

struct nullopt_t {  // NOLINT
  struct init {};   // NOLINT
  nullopt_t(init /*unused*/) {}
};

// extra parenthesis to prevent the most vexing parse:

const nullopt_t nullopt((nullopt_t::init()));  // NOLINT

// Simplistic optional: requires T to be default constructible, copyable.

template<typename T> class optional {  // NOLINT
 private:
  using safe_bool = void (optional::*)() const;

 public:
  using value_type = T;

  optional() {}

  optional(nullopt_t /*unused*/) {}

  optional(T const &arg) : has_value_(true), value_(arg) {}  // NOLINT

  template<class U> optional(optional<U> const &other) : has_value_(other.has_value()), value_(other.value()) {}

  optional &operator=(nullopt_t /*unused*/) {
    reset();
    return *this;
  }

  template<class U> optional &operator=(optional<U> const &other) {
    has_value_ = other.has_value();
    value_ = other.value();
    return *this;
  }

  void swap(optional &rhs) noexcept {
    using std::swap;
    if (has_value() && rhs.has_value()) {
      swap(**this, *rhs);
    } else if (!has_value() && rhs.has_value()) {
      initialize(*rhs);
      rhs.reset();
    } else if (has_value() && !rhs.has_value()) {
      rhs.initialize(**this);
      reset();
    }
  }

  // observers

  value_type const *operator->() const { return &value_; }

  value_type *operator->() { return &value_; }

  value_type const &operator*() const { return value_; }

  value_type &operator*() { return value_; }

  operator safe_bool() const { return has_value() ? &optional::this_type_does_not_support_comparisons : nullptr; }

  bool has_value() const { return has_value_; }

  value_type const &value() const { return value_; }

  value_type &value() { return value_; }

  template<class U> value_type value_or(U const &v) const { return has_value() ? value() : static_cast<value_type>(v); }

  // modifiers

  void reset() { has_value_ = false; }

 private:
  void this_type_does_not_support_comparisons() const {}  // NOLINT

  template<typename V> void initialize(V const &value) {  // NOLINT
    value_ = value;
    has_value_ = true;
  }

  bool has_value_{false};  // NOLINT
  value_type value_;       // NOLINT
};

// Relational operators

template<typename T, typename U> inline bool operator==(optional<T> const &x, optional<U> const &y) {
  return bool(x) != bool(y) ? false : !bool(x) ? true : *x == *y;
}

template<typename T, typename U> inline bool operator!=(optional<T> const &x, optional<U> const &y) {
  return !(x == y);
}

template<typename T, typename U> inline bool operator<(optional<T> const &x, optional<U> const &y) {
  return (!y) ? false : (!x) ? true : *x < *y;
}

template<typename T, typename U> inline bool operator>(optional<T> const &x, optional<U> const &y) { return (y < x); }

template<typename T, typename U> inline bool operator<=(optional<T> const &x, optional<U> const &y) { return !(y < x); }

template<typename T, typename U> inline bool operator>=(optional<T> const &x, optional<U> const &y) { return !(x < y); }

// Comparison with nullopt

template<typename T> inline bool operator==(optional<T> const &x, nullopt_t /*unused*/) { return (!x); }

template<typename T> inline bool operator==(nullopt_t /*unused*/, optional<T> const &x) { return (!x); }

template<typename T> inline bool operator!=(optional<T> const &x, nullopt_t /*unused*/) { return bool(x); }

template<typename T> inline bool operator!=(nullopt_t /*unused*/, optional<T> const &x) { return bool(x); }

template<typename T> inline bool operator<(optional<T> const & /*unused*/, nullopt_t /*unused*/) { return false; }

template<typename T> inline bool operator<(nullopt_t /*unused*/, optional<T> const &x) { return bool(x); }

template<typename T> inline bool operator<=(optional<T> const &x, nullopt_t /*unused*/) { return (!x); }

template<typename T> inline bool operator<=(nullopt_t /*unused*/, optional<T> const & /*unused*/) { return true; }

template<typename T> inline bool operator>(optional<T> const &x, nullopt_t /*unused*/) { return bool(x); }

template<typename T> inline bool operator>(nullopt_t /*unused*/, optional<T> const & /*unused*/) { return false; }

template<typename T> inline bool operator>=(optional<T> const & /*unused*/, nullopt_t /*unused*/) { return true; }

template<typename T> inline bool operator>=(nullopt_t /*unused*/, optional<T> const &x) { return (!x); }

// Comparison with T

template<typename T, typename U> inline bool operator==(optional<T> const &x, U const &v) {
  return bool(x) ? *x == v : false;
}

template<typename T, typename U> inline bool operator==(U const &v, optional<T> const &x) {
  return bool(x) ? v == *x : false;
}

template<typename T, typename U> inline bool operator!=(optional<T> const &x, U const &v) {
  return bool(x) ? *x != v : true;
}

template<typename T, typename U> inline bool operator!=(U const &v, optional<T> const &x) {
  return bool(x) ? v != *x : true;
}

template<typename T, typename U> inline bool operator<(optional<T> const &x, U const &v) {
  return bool(x) ? *x < v : true;
}

template<typename T, typename U> inline bool operator<(U const &v, optional<T> const &x) {
  return bool(x) ? v < *x : false;
}

template<typename T, typename U> inline bool operator<=(optional<T> const &x, U const &v) {
  return bool(x) ? *x <= v : true;
}

template<typename T, typename U> inline bool operator<=(U const &v, optional<T> const &x) {
  return bool(x) ? v <= *x : false;
}

template<typename T, typename U> inline bool operator>(optional<T> const &x, U const &v) {
  return bool(x) ? *x > v : false;
}

template<typename T, typename U> inline bool operator>(U const &v, optional<T> const &x) {
  return bool(x) ? v > *x : true;
}

template<typename T, typename U> inline bool operator>=(optional<T> const &x, U const &v) {
  return bool(x) ? *x >= v : false;
}

template<typename T, typename U> inline bool operator>=(U const &v, optional<T> const &x) {
  return bool(x) ? v >= *x : true;
}

// Specialized algorithms

template<typename T> void swap(optional<T> &x, optional<T> &y) noexcept { x.swap(y); }

// Convenience function to create an optional.

template<typename T> inline optional<T> make_optional(T const &v) { return optional<T>(v); }

}  // namespace esphome
