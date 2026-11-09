#pragma once

#include "esphome/core/helpers.h"
#include <initializer_list>

namespace esphome::select {

/// Option strings: a shared codegen table, or a copy of a runtime list.
using SelectOptions = ConstVector<const char *, true>;

class SelectTraits {
 public:
  SelectTraits() = default;
  SelectTraits(const SelectTraits &) = delete;
  SelectTraits &operator=(const SelectTraits &) = delete;

  /// Codegen only: points at a table that outlives the select. Call before any runtime set_options;
  /// it does not free a previous copy (generated setup() runs before any lambda or automation).
  void set_options_static(const char *const *options, size_t count) { this->options_.assign_static(options, count); }
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
