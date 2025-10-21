#pragma once

#include <string>
#include <initializer_list>
#include "esphome/core/helpers.h"

namespace esphome {
namespace select {

class SelectTraits {
 public:
  void set_options(std::initializer_list<std::string> options);
  const FixedVector<std::string> &get_options() const;
  /// Copy options from another SelectTraits (for copy_select, lvgl)
  void copy_options(const FixedVector<std::string> &other) { this->options_.copy_from(other); }

 protected:
  FixedVector<std::string> options_;
};

}  // namespace select
}  // namespace esphome
