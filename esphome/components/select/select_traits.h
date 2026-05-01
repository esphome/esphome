#pragma once

#include "esphome/core/helpers.h"
#include <initializer_list>

namespace esphome::select {

class SelectTraits {
 public:
  void set_options(const std::initializer_list<const char *> &options);
  void set_options(const FixedVector<const char *> &options);
  const FixedVector<const char *> &get_options() const;

 protected:
  FixedVector<const char *> options_;
};

}  // namespace esphome::select
