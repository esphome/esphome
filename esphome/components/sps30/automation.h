#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sps30.h"

namespace esphome {
namespace sps30 {

template<typename... Ts> class StartFanAction : public Action<Ts...> {
 public:
  explicit StartFanAction(SPS30Component *sps30) : sps30_(sps30) {}

  void play(Ts... x) override { this->sps30_->start_fan_cleaning(); }

 protected:
  SPS30Component *sps30_;
};

}  // namespace sps30
}  // namespace esphome
