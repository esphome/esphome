
#pragma once
#include "esphome/core/automation.h"
#include "max17043.h"

namespace esphome {
namespace max17043 {

template<typename... Ts> class SleepAction : public Action<Ts...> {
 public:
  explicit SleepAction(MAX17043Component *max17043) : max17043_(max17043) {}

  void play(Ts... x) override { this->max17043_->sleep_mode(); }

 protected:
  MAX17043Component *max17043_;
};

}  // namespace max17043
}  // namespace esphome
