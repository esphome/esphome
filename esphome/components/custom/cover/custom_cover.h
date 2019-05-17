#pragma once

#include "esphome/components/cover/cover.h"

namespace esphome {
namespace custom {

class CustomCoverConstructor {
 public:
  CustomCoverConstructor(const std::function<std::vector<cover::Cover *>()> &init) { this->covers_ = init(); }

  cover::Cover *get_cover(int i) { return this->covers_[i]; }

 protected:
  std::vector<cover::Cover *> covers_;
};

}  // namespace custom
}  // namespace esphome
