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
  /// The table must outlive the select.
  void set_options(const char *const *options, size_t count) { this->options_ = SelectOptions(options, count); }
  void set_options(SelectOptions options) { this->options_ = options; }
  // Remove before 2027.5.0
  ESPDEPRECATED("Pass a table that outlives the select instead. Removed in 2027.5.0", "2026.11.0")
  void set_options(const std::initializer_list<const char *> &options);
  // Remove before 2027.5.0
  ESPDEPRECATED("Pass a table that outlives the select instead. Removed in 2027.5.0", "2026.11.0")
  void set_options(const FixedVector<const char *> &options);
  const SelectOptions &get_options() const { return this->options_; }

 protected:
  SelectOptions options_;
};

}  // namespace esphome::select
