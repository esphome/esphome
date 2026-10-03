#pragma once

#include "esphome/core/helpers.h"
#include <initializer_list>

namespace esphome::select {

class SelectTraits {
 public:
  void set_options(const std::initializer_list<const char *> &options);
  void set_options(const FixedVector<const char *> &options);
  const FixedVector<const char *> &get_options() const { return this->options_; }
  /// Mutable access for platforms whose options change at runtime. To avoid allocating after setup, reserve
  /// the capacity once with init() during setup and afterwards only use clear() and push_back().
  FixedVector<const char *> &get_options_mutable() { return this->options_; }

 protected:
  FixedVector<const char *> options_;
};

}  // namespace esphome::select
