#pragma once

#include "esphome/components/switch/switch.h"
#include "../loki.h"

namespace esphome {
namespace loki {

class EnabledSwitch : public switch_::Switch, public Parented<Loki> {
 public:
  EnabledSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace loki
}  // namespace esphome
