#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESPECTRE

#include "esphome/components/button/button.h"
#include "../espectre.h"

namespace esphome::espectre {

class RecalibrateButton final : public button::Button, public Parented<ESPectreComponent> {
 protected:
  void press_action() override { this->parent_->recalibrate(); }
};

}  // namespace esphome::espectre

#endif  // USE_ESPECTRE
