#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESPECTRE

#include "esphome/components/select/select.h"
#include "../espectre.h"

namespace esphome::espectre {

class TrafficModeSelect final : public select::Select, public Parented<ESPectreComponent> {
 protected:
  void control(size_t index) override;
};

}  // namespace esphome::espectre

#endif  // USE_ESPECTRE
