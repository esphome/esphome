#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_PLAYER) && defined(USE_SENDSPIN_CONTROLLER)

#include "esphome/core/automation.h"
#include "sendspin_media_source.h"

namespace esphome::sendspin_ {

template<typename... Ts>
class EnableStaticDelayAdjustmentAction : public Action<Ts...>, public Parented<SendspinMediaSource> {
 public:
  void play(const Ts &...x) override { this->parent_->set_static_delay_adjustable(true); }
};

template<typename... Ts>
class DisableStaticDelayAdjustmentAction : public Action<Ts...>, public Parented<SendspinMediaSource> {
 public:
  void play(const Ts &...x) override { this->parent_->set_static_delay_adjustable(false); }
};

}  // namespace esphome::sendspin_

#endif
