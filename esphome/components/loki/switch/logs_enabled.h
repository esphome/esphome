#pragma once

#include "esphome/components/switch/switch.h"
#include "../loki.h"

namespace esphome {
namespace loki {

class LogsEnabledSwitch : public switch_::Switch, public Parented<Loki> {
 public:
  LogsEnabledSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace loki
}  // namespace esphome
