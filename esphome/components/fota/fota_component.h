#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace fota {

class FOTAComponent : public Component {
 public:
  void setup() override;
};

}  // namespace fota
}  // namespace esphome
