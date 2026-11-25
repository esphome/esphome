#pragma once

#include <vector>

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"
#include "fan_traits.h"

namespace esphome::fan {

#define LOG_FAN(prefix, type, obj) \
  if (log_entity_header(TAG, prefix, LOG_STR_LITERAL(type), (obj))) { \
    (obj)->dump_traits_(TAG, prefix); \
  }

/// Simple enum to represent the direction of a fan.
enum class FanDirection { FORWARD = 0, REVERSE = 1 };

/// Restore mode of a fan.
enum class FanRestoreMode {
  NO_RESTORE,
  ALWAYS_OFF,
  ALWAYS_ON,
  RESTORE_DEFAULT_OFF,
  RESTORE_DEFAULT_ON,
  RESTORE_INVERTED_DEFAULT_OFF,
  RESTORE_INVERTED_DEFAULT_ON,
};

const LogString *fan_direction_to_string(FanDirection direction);

class Fan;

class FanCall {
 public:
  explicit FanCall(Fan &parent) : parent_(parent) {}

  FanCall &set_state(bool binary_state) {
    this->binary_state_ = binary_state;
    return *this;
  }
  FanCall &set_state(optional<bool> binary_state) {
    this->binary_state_ = binary_state;
    return *this;
  }
  optional<bool> get_state() const { return this->binary_state_; }
  FanCall &set_oscillating(bool oscillating) {
    this->oscillating_ = oscillating;
    return *this;
  }
  FanCall &set_oscillating(optional<bool> oscillating) {
    this->oscillating_ = oscillating;
    return *this;
  }
  optional<bool> get_oscillating() const { return this->oscillating_; }
  FanCall &set_speed(int speed) {
    this->speed_ = speed;
    return *this;
  }
  optional<int> get_speed() const { return this->speed_; }
  FanCall &set_direction(FanDirection direction) {
    this->direction_ = direction;
    return *this;
  }
  FanCall &set_direction(optional<FanDirection> direction) {
    this->direction_ = direction;
    return *this;
  }
  optional<FanDirection> get_direction() const { return this->direction_; }
  FanCall &set_preset_mode(const std::string &preset_mode);
  FanCall &set_preset_mode(const char *preset_mode);
  FanCall &set_preset_mode(const char *preset_mode, size_t len);
  const char *get_preset_mode() const { return this->preset_mode_; }
  bool has_preset_mode() const { return this->preset_mode_ != nullptr; }

  void perform();

 protected:
  void validate_();

  Fan &parent_;
  optional<bool> binary_state_;
  optional<bool> oscillating_;
  optional<int> speed_;
  optional<FanDirection> direction_{};
  const char *preset_mode_{nullptr};  // Pointer to string in traits (after validation)
};

struct FanRestoreState {
  static constexpr uint8_t NO_PRESET = UINT8_MAX;

  bool state;
  int speed;
  bool oscillating;
  FanDirection direction;
  uint8_t preset_mode{NO_PRESET};

  /// Convert this struct to a fan call that can be performed.
  FanCall to_call(Fan &fan);
  /// Apply these settings to the fan.
  void apply(Fan &fan);
} __attribute__((packed));

#ifdef USE_FAN_CHANGE_TRIGGERS
/// The triggers that fire when a published value changes.
enum class FanChangeTrigger : uint8_t {
  FAN_CHANGE_TRIGGER_TURN_ON,
  FAN_CHANGE_TRIGGER_TURN_OFF,
  FAN_CHANGE_TRIGGER_DIRECTION_SET,
  FAN_CHANGE_TRIGGER_OSCILLATING_SET,
  FAN_CHANGE_TRIGGER_SPEED_SET,
  FAN_CHANGE_TRIGGER_PRESET_SET,
};

/// The value each change trigger last saw; each trigger keeps its own, as on_turn_on and on_turn_off do.
struct FanChangeSeen {
  int speed{0};
  const char *preset_mode{nullptr};
  uint8_t direction{static_cast<uint8_t>(FanDirection::FORWARD)};
  bool turn_on_state : 1 {false};
  bool turn_off_state : 1 {false};
  bool oscillating : 1 {false};
#ifdef USE_FAN_CHANGE_TRIGGERS_MULTI
  /// Per trigger, the next of its automations still owed the change its first automation saw.
  uint8_t next_automation[6]{};
#endif
};
#endif

class Fan : public EntityBase {
 public:
  /// The current on/off state of the fan.
  bool state{false};
  /// The current oscillation state of the fan.
  bool oscillating{false};
  /// The current fan speed level
  int speed{0};
  /// The current direction of the fan
  FanDirection direction{FanDirection::FORWARD};

  FanCall turn_on() { return this->make_call().set_state(true); }
  FanCall turn_off() { return this->make_call().set_state(false); }
  FanCall toggle() { return this->make_call().set_state(!this->state); }
  FanCall make_call() { return FanCall(*this); }

  /// Register a callback that will be called each time the state changes.
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }

  void publish_state();

#ifdef USE_FAN_CHANGE_TRIGGERS
  /// For the generated state callbacks: whether automation `index` of trigger T fires now.
  template<FanChangeTrigger T> bool take_change(uint8_t index);
#endif

  virtual FanTraits get_traits() = 0;

  /// Set the supported preset modes; the list is copied (stored on Fan, referenced by FanTraits).
  void set_supported_preset_modes(std::initializer_list<const char *> preset_modes) {
    this->supported_preset_modes_.assign_copy(preset_modes.begin(), preset_modes.size());
  }
  void set_supported_preset_modes(const std::vector<const char *> &preset_modes) {
    this->supported_preset_modes_.assign_copy(preset_modes.data(), preset_modes.size());
  }
  void set_supported_preset_modes(const FanPresetModes &preset_modes) {
    this->supported_preset_modes_.assign_copy(preset_modes.data(), preset_modes.size());
  }
  /// Points at a static table that outlives the fan (codegen); call before any runtime copy.
  void set_supported_preset_modes_static(const char *const *preset_modes, size_t count) {
    this->supported_preset_modes_.assign_static(preset_modes, count);
  }

  /// Set the restore mode of this fan.
  void set_restore_mode(FanRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }

  /// Get the current preset mode.
  /// Returns a StringRef of the string stored in traits, or empty ref if not set.
  /// The returned ref points to string literals from codegen (static storage).
  /// Traits are set once at startup and valid for the lifetime of the program.
  StringRef get_preset_mode() const { return StringRef::from_maybe_nullptr(this->preset_mode_); }

  /// Check if a preset mode is currently active
  bool has_preset_mode() const { return this->preset_mode_ != nullptr; }

 protected:
  friend FanCall;
  friend struct FanRestoreState;

  virtual void control(const FanCall &call) = 0;

  optional<FanRestoreState> restore_state_();
  void save_state_();

  void dump_traits_(const char *tag, const char *prefix);

  /// Set the preset mode (finds and stores pointer from traits). Returns true if changed.
  /// Passing nullptr or empty string clears the preset mode.
  bool set_preset_mode_(const char *preset_mode, size_t len);
  bool set_preset_mode_(const char *preset_mode);
  bool set_preset_mode_(const std::string &preset_mode);
  bool set_preset_mode_(StringRef preset_mode);
  /// Clear the preset mode
  void clear_preset_mode_();
  /// Apply preset mode from a FanCall (handles speed-clears-preset convention)
  void apply_preset_mode_(const FanCall &call);
  /// Find and return the matching preset mode pointer from traits, or nullptr if not found.
  const char *find_preset_mode_(const char *preset_mode);
  const char *find_preset_mode_(const char *preset_mode, size_t len);

  /// Wire the Fan-owned preset modes pointer into the given traits object.
  void wire_preset_modes_(FanTraits &traits) { traits.set_supported_preset_modes_(&this->supported_preset_modes_); }

  LazyCallbackManager<void()> state_callback_{};
  ESPPreferenceObject rtc_;
  FanRestoreMode restore_mode_{FanRestoreMode::NO_RESTORE};

 private:
  FanPresetModes supported_preset_modes_;
  const char *preset_mode_{nullptr};
#ifdef USE_FAN_CHANGE_TRIGGERS
  FanChangeSeen change_seen_;
#endif
};

#ifdef USE_FAN_CHANGE_TRIGGERS
template<FanChangeTrigger T> bool Fan::take_change(uint8_t index) {
  auto &seen = this->change_seen_;
#ifdef USE_FAN_CHANGE_TRIGGERS_MULTI
  // Only the first automation compares; the rest each fire once for what it saw, in order, even when an
  // action publishes again before they ran.
  uint8_t &next = seen.next_automation[static_cast<uint8_t>(T)];
  if (index != 0) {
    if (next != index)
      return false;
    next = index + 1;
    return true;
  }
#endif
  bool changed;
  if constexpr (T == FanChangeTrigger::FAN_CHANGE_TRIGGER_TURN_ON) {
    changed = this->state && !seen.turn_on_state;
    seen.turn_on_state = this->state;
  } else if constexpr (T == FanChangeTrigger::FAN_CHANGE_TRIGGER_TURN_OFF) {
    changed = !this->state && seen.turn_off_state;
    seen.turn_off_state = this->state;
  } else if constexpr (T == FanChangeTrigger::FAN_CHANGE_TRIGGER_DIRECTION_SET) {
    auto direction = static_cast<uint8_t>(this->direction);
    changed = direction != seen.direction;
    seen.direction = direction;
  } else if constexpr (T == FanChangeTrigger::FAN_CHANGE_TRIGGER_OSCILLATING_SET) {
    changed = this->oscillating != seen.oscillating;
    seen.oscillating = this->oscillating;
  } else if constexpr (T == FanChangeTrigger::FAN_CHANGE_TRIGGER_SPEED_SET) {
    changed = this->speed != seen.speed;
    seen.speed = this->speed;
  } else {
    changed = this->get_preset_mode() != StringRef::from_maybe_nullptr(seen.preset_mode);
    seen.preset_mode = this->preset_mode_;
  }
#ifdef USE_FAN_CHANGE_TRIGGERS_MULTI
  if (changed)
    next = 1;
#endif
  return changed;
}
#endif

}  // namespace esphome::fan
