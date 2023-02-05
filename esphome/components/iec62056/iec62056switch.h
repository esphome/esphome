#pragma once

#include "esphome/components/switch/switch.h"
#include "iec62056.h"

namespace esphome {
namespace iec62056 {

class IEC62056Switch : public switch_::Switch {
 public:
  void set_parent(IEC62056Component *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  IEC62056Component *parent_;
};

}  // namespace iec62056
}  // namespace esphome
