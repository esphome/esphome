#pragma once

#include "esphome/core/helpers.h"
#include <initializer_list>

namespace esphome::select {

/// Non-owning view of the option strings; codegen points it at a shared flash table.
/// Iterators are raw pointers, like FixedVector, so existing lambdas keep compiling.
class SelectOptions {
 public:
  constexpr SelectOptions() = default;
  constexpr SelectOptions(const char *const *data, size_t size) : data_(data), size_(size) {}
  const char *const *begin() const { return this->data_; }
  const char *const *end() const { return this->data_ + this->size_; }
  const char *const *data() const { return this->data_; }
  size_t size() const { return this->size_; }
  bool empty() const { return this->size_ == 0; }
  const char *operator[](size_t index) const { return this->data_[index]; }
  const char *at(size_t index) const { return this->data_[index]; }

 protected:
  const char *const *data_{nullptr};
  size_t size_{0};
};

class SelectTraits {
 public:
  SelectTraits() = default;
  SelectTraits(const SelectTraits &) = delete;
  SelectTraits &operator=(const SelectTraits &) = delete;

  /// Codegen only: points at a table that must outlive the select, no copy.
  void set_options_static(const char *const *options, size_t count) { this->options_ = SelectOptions(options, count); }
  /// Runtime lists: the pointer list is copied, as before; the strings must still outlive the select.
  void set_options(const SelectOptions &options) { this->set_options_copy_(options.data(), options.size()); }
  void set_options(const std::initializer_list<const char *> &options);
  void set_options(const FixedVector<const char *> &options);
  const SelectOptions &get_options() const { return this->options_; }

 protected:
  void set_options_copy_(const char *const *options, size_t count);

  SelectOptions options_;
};

}  // namespace esphome::select
