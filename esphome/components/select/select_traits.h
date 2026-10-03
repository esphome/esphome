#pragma once

#include "esphome/core/helpers.h"
#include <initializer_list>
#include <span>

namespace esphome::select {

/// Non-owning view of the option strings; codegen points it at a shared flash table.
class SelectOptions : public std::span<const char *const> {
 public:
  using std::span<const char *const>::span;
  const char *at(size_t index) const { return (*this)[index]; }
};

class SelectTraits {
 public:
  /// Codegen path: points at a table that outlives the select, no copy.
  void set_options(const char *const *options, size_t count) { this->options_ = SelectOptions(options, count); }
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
