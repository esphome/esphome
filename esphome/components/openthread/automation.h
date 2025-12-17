#pragma once
#include "openthread.h"

#include "esphome/core/automation.h"

namespace esphome::openthread {

template<typename... Ts> class OpenThreadComponentRadioAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(bool, keep_radio_on)

  void play(const Ts &...x) override {
#ifdef USE_OPENTHREAD_POLL_PERIOD
    bool keep_radio_on = false;
    if (this->keep_radio_on_.has_value()) {
      keep_radio_on = this->keep_radio_on_.value(x...);
    }
    openthread::global_openthread_component->keep_radio_on_during_idle(keep_radio_on);
#endif
  }
};
}  // namespace esphome::openthread
