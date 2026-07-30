#ifdef USE_BK72XX

#include "deep_sleep_component.h"
#include "esphome/core/log.h"

namespace esphome::deep_sleep {

static const char *const TAG = "deep_sleep.bk72xx";

#ifdef USE_DEEP_SLEEP_ON_WAKE
WakeupCause get_wakeup_cause() {
  switch (lt_get_reboot_reason()) {
    case REBOOT_REASON_SLEEP_GPIO:
      return WAKEUP_CAUSE_GPIO;
    case REBOOT_REASON_SLEEP_RTC:
      return WAKEUP_CAUSE_TIMER;
    case REBOOT_REASON_SLEEP_USB:
      return WAKEUP_CAUSE_UNKNOWN;
    default:
      return WAKEUP_CAUSE_NONE;
  }
}
#endif  // USE_DEEP_SLEEP_ON_WAKE

optional<uint32_t> DeepSleepComponent::get_run_duration_() const { return this->run_duration_; }

void DeepSleepComponent::dump_config_platform_() {
  for (const WakeUpPinItem &item : this->wakeup_pins_) {
    LOG_PIN("  Wakeup Pin: ", item.wakeup_pin);
  }
}

bool DeepSleepComponent::pin_prevents_sleep_(WakeUpPinItem &pin_item) const {
  return (pin_item.wakeup_pin_mode == WAKEUP_PIN_MODE_KEEP_AWAKE && pin_item.wakeup_pin != nullptr &&
          !this->sleep_duration_.has_value() && (pin_item.wakeup_level == get_real_pin_state_(*pin_item.wakeup_pin)));
}

bool DeepSleepComponent::prepare_to_sleep_() {
  if (!this->wakeup_pins_.empty()) {
    for (WakeUpPinItem &item : this->wakeup_pins_) {
      if (this->pin_prevents_sleep_(item)) {
        // Defer deep sleep until inactive
        if (!this->next_enter_deep_sleep_) {
          this->status_set_warning();
          ESP_LOGV(TAG, "Waiting for pin to switch state to enter deep sleep...");
        }
        this->next_enter_deep_sleep_ = true;
        return false;
      }
    }
  }
  return true;
}

void DeepSleepComponent::deep_sleep_() {
  for (WakeUpPinItem &item : this->wakeup_pins_) {
    if (item.wakeup_pin_mode == WAKEUP_PIN_MODE_INVERT_WAKEUP) {
      if (item.wakeup_level == get_real_pin_state_(*item.wakeup_pin)) {
        item.wakeup_level = !item.wakeup_level;
      }
    }
    ESP_LOGI(TAG, "Wake-up on P%u %s (%" PRId32 ")", item.wakeup_pin->get_pin(), item.wakeup_level ? "HIGH" : "LOW",
             static_cast<int32_t>(item.wakeup_pin_mode));
  }

  if (this->sleep_duration_.has_value())
    lt_deep_sleep_config_timer((*this->sleep_duration_ / 1000) & 0xFFFFFFFF);

  for (WakeUpPinItem &item : this->wakeup_pins_) {
    lt_deep_sleep_config_gpio(1 << item.wakeup_pin->get_pin(), item.wakeup_level);
    lt_deep_sleep_keep_floating_gpio(1 << item.wakeup_pin->get_pin(), true);
  }

  lt_deep_sleep_enter();
}

bool DeepSleepComponent::should_teardown_() { return true; }

}  // namespace esphome::deep_sleep

#endif  // USE_BK72XX
