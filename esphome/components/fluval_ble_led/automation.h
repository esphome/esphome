#pragma once

#ifdef USE_ESP32

#include <utility>

#include "esphome/core/automation.h"
#include "fluval_ble_led.h"

namespace esphome {
namespace fluval_ble_led {
static const char *const TAG = "fluval_ble_led.automation";

template<typename... Ts> class FluvalBleLedTimeSyncAction : public Action<Ts...> {
 public:
  explicit FluvalBleLedTimeSyncAction(FluvalBleLed *fluval_ble_led) {
    fluval_ble_led_ = fluval_ble_led;
  }

  void play(Ts... x) override {
    if (fluval_ble_led_->node_state != espbt::ClientState::ESTABLISHED) {
      ESP_LOGW(TAG, "Cannot sync fluval led time - not connected");
      return;
    }
#ifdef USE_TIME
    fluval_ble_led_->sync_time();
#endif
  }

 protected:
  FluvalBleLed *fluval_ble_led_;
};

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
