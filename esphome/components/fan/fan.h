#pragma once

#include <type_traits>
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
    this->binary_state_ = to_byte(binary_state);
    return *this;
  }
  optional<bool> get_state() const { return from_byte<bool>(this->binary_state_); }
  FanCall &set_oscillating(bool oscillating) {
    this->oscillating_ = oscillating;
    return *this;
  }
  FanCall &set_oscillating(optional<bool> oscillating) {
    this->oscillating_ = to_byte(oscillating);
    return *this;
  }
  optional<bool> get_oscillating() const { return from_byte<bool>(this->oscillating_); }
  FanCall &set_speed(int speed) {
    this->speed_ = speed;
    return *this;
  }
  optional<int> get_speed() const { return this->speed_ == NO_SPEED ? optional<int>{} : optional<int>{this->speed_}; }
  FanCall &set_direction(FanDirection direction) {
    this->direction_ = static_cast<uint8_t>(direction);
    return *this;
  }
  FanCall &set_direction(optional<FanDirection> direction) {
    this->direction_ = to_byte(direction);
    return *this;
  }
  optional<FanDirection> get_direction() const { return from_byte<FanDirection>(this->direction_); }
  FanCall &set_preset_mode(const std::string &preset_mode);
  FanCall &set_preset_mode(const char *preset_mode);
  FanCall &set_preset_mode(const char *preset_mode, size_t len);
  const char *get_preset_mode() const;
  bool has_preset_mode() const { return this->preset_mode_ != UNSET; }

  void perform();

 protected:
  friend Fan;

  // Plain bytes with UNSET for "not set", and the preset as an index into the fan's preset list, instead of
  // optional<> and a pointer, so the call fits the scheduler's 12-byte inline slot
  // 0x80 rather than 0xFF: Xtensa compares it in one instruction; it also caps the preset index at 127
  static constexpr int NO_SPEED = INT32_MIN;
  static constexpr uint8_t UNSET = 0x80;
  template<typename T> static uint8_t to_byte(optional<T> value) {
    return value.has_value() ? static_cast<uint8_t>(*value) : UNSET;
  }
  template<typename T> static optional<T> from_byte(uint8_t value) {
    if (value == UNSET)
      return {};
    if constexpr (std::is_same_v<T, bool>) {
      return (value & 1) != 0;  // the setters store 0 or 1, so no normalising
    } else {
      return static_cast<T>(value);
    }
  }

  void validate_();

  Fan &parent_;
  int speed_{NO_SPEED};
  uint8_t binary_state_{UNSET};
  uint8_t oscillating_{UNSET};
  uint8_t direction_{UNSET};
  uint8_t preset_mode_{UNSET};  // Index into the fan's supported preset modes (after validation)
};
static_assert(sizeof(void *) != 4 || sizeof(FanCall) <= 12, "FanCall must stay within 12 bytes on 32-bit");

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
};

}  // namespace esphome::fan
