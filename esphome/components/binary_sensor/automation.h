#pragma once

#include <cinttypes>
#include <type_traits>
#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::binary_sensor {

struct MultiClickTriggerEvent {
  uint32_t state;  // Not bool: all-word fields allow aligned loads from PROGMEM on ESP8266
  uint32_t min_length;
  uint32_t max_length;
};
// Read straight from PROGMEM; a byte-sized field would fault on ESP8266.
static_assert(std::is_same_v<decltype(MultiClickTriggerEvent::state), uint32_t> &&
                  sizeof(MultiClickTriggerEvent) == 3 * sizeof(uint32_t),
              "MultiClickTriggerEvent fields must all be uint32_t");

bool match_interval(uint32_t min_length, uint32_t max_length, uint32_t length);

/// Times the presses of one on_click automation.
class ClickDetector {
 public:
  /// Whether `state` ends a press of min_length to max_length ms; a max_length of 0 means no maximum.
  bool check(bool state, uint32_t min_length, uint32_t max_length);

 protected:
  uint32_t start_time_{0};  ///< The millis() time when the press started.
};

/// Times the presses of one on_double_click automation.
class DoubleClickDetector {
 public:
  /// Whether `state` is the second press, with the first press and the gap both in the window.
  bool check(bool state, uint32_t min_length, uint32_t max_length);

 protected:
  uint32_t start_time_{0};  ///< The millis() time of the edge before the last one.
  uint32_t end_time_{0};    ///< The millis() time of the last edge.
};

class MultiClickTrigger final : public Trigger<>, public Component {
 public:
  /// `timing` is a codegen PROGMEM table shared by triggers with the same timing.
  MultiClickTrigger(BinarySensor *parent, const MultiClickTriggerEvent *timing, uint8_t timing_count)
      : parent_(parent), timing_(timing), timing_count_(timing_count) {}

  void setup() override {
    this->last_state_ = this->parent_->get_state_default(false);
    this->parent_->add_on_state_callback([this](bool state) { this->on_state_(state); });
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_invalid_cooldown(uint32_t invalid_cooldown) { this->invalid_cooldown_ = invalid_cooldown; }

  void cancel();
  MultiClickTrigger(const MultiClickTrigger &) = delete;
  MultiClickTrigger &operator=(const MultiClickTrigger &) = delete;

 protected:
  void on_state_(bool state);
  void schedule_cooldown_();
  void schedule_is_valid_(uint32_t min_length);
  void schedule_is_not_valid_(uint32_t max_length);
  void trigger_();

  BinarySensor *parent_;
  const MultiClickTriggerEvent *timing_;
  uint32_t invalid_cooldown_{1000};  // Must match DEFAULT_INVALID_COOLDOWN_MS in __init__.py
  uint8_t timing_count_;
  uint8_t at_index_{0};  // 0: not matching, otherwise the index of the next timing event
  bool last_state_{false};
  bool is_in_cooldown_{false};
  bool is_valid_{false};
};

}  // namespace esphome::binary_sensor
