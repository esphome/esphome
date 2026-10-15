#pragma once

#ifdef USE_ESP32
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"

namespace esphome::fendt_caravan {

class MainControlUnitHub;

/// Its state is published only once the panel reports it, so a press the panel ignored stays visible
class FendtSwitch : public switch_::Switch, public Parented<MainControlUnitHub> {
 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::fendt_caravan
#endif
