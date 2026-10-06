#pragma once

#include "esphome/components/ld600x/ld600x.h"

namespace esphome::ld6002b {

class LD6002BComponent final : public ld600x::LD600XComponent {
 public:
  LD6002BComponent() : LD600XComponent("LD6002B", "ld6002b") {}
};

}  // namespace esphome::ld6002b
