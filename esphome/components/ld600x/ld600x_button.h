#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BUTTON

#include "esphome/components/button/button.h"
#include "ld600x.h"

namespace esphome::ld600x {

class LD600XButton final : public button::Button, public Parented<LD600XComponent> {
 public:
  explicit LD600XButton(uint8_t kind) : kind_(kind) {}

 protected:
  void press_action() override;

  uint8_t kind_;
};

}  // namespace esphome::ld600x

#endif  // USE_BUTTON
