#pragma once

#include "esphome/components/switch/switch.h"

namespace esphome {
namespace ld2410 {

class LD2410Switch : public switch_::Switch {
 public:
  LD2410Switch();

 protected:
  void write_state(bool state) override;
};

}  // namespace ld2410
}  // namespace esphome
