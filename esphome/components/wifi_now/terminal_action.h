#pragma once

#include <functional>

#include "esphome/core/defines.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace wifi_now {

template<typename... Ts> class WifiNowTerminalAction : public Action<Ts...> {
 public:
  explicit WifiNowTerminalAction(std::function<bool(Ts...)> &&f) : f_(std::move(f)) {}

  void play_complex(Ts... x) override {
    this->num_running_++;
    if (!this->f_(x...)) {
      this->play_next_(x...);
    }
    this->num_running_--;
  }

  void play(Ts... x) override{
      /* everything is in play_complex */
  };

 protected:
  std::function<bool(Ts...)> f_;
};

}  // namespace wifi_now
}  // namespace esphome
