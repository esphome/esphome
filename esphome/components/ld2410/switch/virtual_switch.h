#pragma once

#include "esphome/components/switch/switch.h"

namespace esphome {
namespace ld2410 {

class VirtualSwitch : public switch_::Switch {
 public:
  VirtualSwitch();

 protected:
  void write_state(bool state) override;
};

}  // namespace ld2410
}  // namespace esphome
