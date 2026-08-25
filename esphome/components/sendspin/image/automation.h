#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_ARTWORK)

#include "esphome/core/automation.h"
#include "sendspin_image.h"

namespace esphome::sendspin_ {

template<typename... Ts>
class SendspinImageTransitionFinishedAction final : public Action<Ts...>, public Parented<SendspinImageSlot> {
 public:
  void play(const Ts &...x) override { this->parent_->transition_finished(); }
};

}  // namespace esphome::sendspin_

#endif
