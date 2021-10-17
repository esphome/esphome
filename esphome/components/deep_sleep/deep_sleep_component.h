#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32
#include <esp_sleep.h>
#endif

namespace esphome {
namespace deep_sleep {

#ifdef USE_ESP32

/** The values of this enum define what should be done if deep sleep is set up with a wakeup pin on the ESP32
 * and the scenario occurs that the wakeup pin is already in the wakeup state.
 */
enum WakeupPinMode {
  WAKEUP_PIN_MODE_IGNORE = 0,  ///< Ignore the fact that we will wake up when going into deep sleep.
  WAKEUP_PIN_MODE_KEEP_AWAKE,  ///< As long as the wakeup pin is still in the wakeup state, keep awake.

  /** Automatically invert the wakeup level. For example if we were set up to wake up on HIGH, but the pin
   * is already high when attempting to enter deep sleep, re-configure deep sleep to wake up on LOW level.
   */
  WAKEUP_PIN_MODE_INVERT_WAKEUP,
};

struct Ext1Wakeup {
  uint64_t mask;
  esp_sleep_ext1_wakeup_mode_t wakeup_mode;
};

#endif

template<typename... Ts> class EnterDeepSleepAction;

template<typename... Ts> class PreventDeepSleepAction;

/** This component allows setting up the node to go into deep sleep mode to conserve battery.
 *
 * To set this component up, first set *when* the deep sleep should trigger using set_run_cycles
 * and set_run_duration, then set how long the deep sleep should last using set_sleep_duration and optionally
 * on the ESP32 set_wakeup_pin.
 */
class DeepSleepComponent : public Component {
 public:
  /// Set the duration in ms the component should sleep once it's in deep sleep mode.
  void set_sleep_duration(uint32_t time_ms);
#ifdef USE_ESP32
  /** Set the pin to wake up to on the ESP32 once it's in deep sleep mode.
   * Use the inverted property to set the wakeup level.
   */
  void set_wakeup_pin(InternalGPIOPin *pin) { this->wakeup_pin_ = pin; }

  void set_wakeup_pin_mode(WakeupPinMode wakeup_pin_mode);

  void set_ext1_wakeup(Ext1Wakeup ext1_wakeup);

  void set_touch_wakeup(bool touch_wakeup);
#endif
  /// Set a duration in ms for how long the code should run before entering deep sleep mode.
  void set_run_duration(uint32_t time_ms);

  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_loop_priority() const override;
  float get_setup_priority() const override;

  /// Helper to enter deep sleep mode
  void begin_sleep(bool manual = false);

  void prevent_deep_sleep();

 protected:
  optional<uint64_t> sleep_duration_;
#ifdef USE_ESP32
  InternalGPIOPin *wakeup_pin_;
  WakeupPinMode wakeup_pin_mode_{WAKEUP_PIN_MODE_IGNORE};
  optional<Ext1Wakeup> ext1_wakeup_;
  optional<bool> touch_wakeup_;
#endif
  optional<uint32_t> run_duration_;
  bool next_enter_deep_sleep_{false};
  bool prevent_{false};
};

extern bool global_has_deep_sleep;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<typename... Ts> class EnterDeepSleepAction : public Action<Ts...> {
 public:
  EnterDeepSleepAction(DeepSleepComponent *deep_sleep) : deep_sleep_(deep_sleep) {}
  TEMPLATABLE_VALUE(uint32_t, sleep_duration);

  void play(Ts... x) override {
    if (this->sleep_duration_.has_value()) {
      this->deep_sleep_->set_sleep_duration(this->sleep_duration_.value(x...));
    }
    this->deep_sleep_->begin_sleep(true);
  }

 protected:
  DeepSleepComponent *deep_sleep_;
};

template<typename... Ts> class PreventDeepSleepAction : public Action<Ts...> {
 public:
  PreventDeepSleepAction(DeepSleepComponent *deep_sleep) : deep_sleep_(deep_sleep) {}

  void play(Ts... x) override { this->deep_sleep_->prevent_deep_sleep(); }

 protected:
  DeepSleepComponent *deep_sleep_;
};

}  // namespace deep_sleep
}  // namespace esphome
