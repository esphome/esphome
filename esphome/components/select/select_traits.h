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

 protected:
  FixedVector<std::string> options_;
};

}  // namespace select
}  // namespace esphome
