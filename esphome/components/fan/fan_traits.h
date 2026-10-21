#pragma once

#include <cstring>
#include <type_traits>
#include "esphome/core/helpers.h"

namespace esphome::fan {

class Fan;  // Forward declaration

/// Preset modes: a flash table from codegen, or an owned copy set at runtime.
using FanPresetModes = ConstVector<const char *, true>;

class FanTraits {
  friend class Fan;  // Allow Fan to access protected pointer setter

 public:
  FanTraits() = default;
  FanTraits(bool oscillation, bool speed, bool direction, int speed_count)
      : oscillation_(oscillation), speed_(speed), direction_(direction), speed_count_(speed_count) {}

  /// Return if this fan supports oscillation.
  bool supports_oscillation() const { return this->oscillation_; }
  /// Set whether this fan supports oscillation.
  void set_oscillation(bool oscillation) { this->oscillation_ = oscillation; }
  /// Return if this fan supports speed modes.
  bool supports_speed() const { return this->speed_; }
  /// Set whether this fan supports speed levels.
  void set_speed(bool speed) { this->speed_ = speed; }
  /// Return how many speed levels the fan has
  int supported_speed_count() const { return this->speed_count_; }
  /// Set how many speed levels this fan has.
  void set_supported_speed_count(int speed_count) { this->speed_count_ = speed_count; }
  /// Return if this fan supports changing direction
  bool supports_direction() const { return this->direction_; }
  /// Set whether this fan supports changing direction
  void set_direction(bool direction) { this->direction_ = direction; }
  /// Empty when the fan has no preset modes. Set them on the Fan entity.
  const FanPresetModes &supported_preset_modes() const;

  /// Return if preset modes are supported
  bool supports_preset_modes() const { return this->preset_modes_ != nullptr && !this->preset_modes_->empty(); }
  /// Find and return the matching preset mode pointer from supported modes, or nullptr if not found.
  const char *find_preset_mode(const char *preset_mode) const {
    return this->find_preset_mode(preset_mode, preset_mode ? strlen(preset_mode) : 0);
  }
  const char *find_preset_mode(const char *preset_mode, size_t len) const {
    if (preset_mode == nullptr || len == 0) {
      return nullptr;
    }
    for (const char *mode : this->supported_preset_modes()) {
      if (strncmp(mode, preset_mode, len) == 0 && mode[len] == '\0') {
        return mode;
      }
    }
    return nullptr;
  }

 protected:
  /// Set the preset modes pointer (only Fan::wire_preset_modes_() should call this).
  void set_supported_preset_modes_(const FanPresetModes *preset_modes) { this->preset_modes_ = preset_modes; }

  bool oscillation_{false};
  bool speed_{false};
  bool direction_{false};
  int speed_count_{};
  const FanPresetModes *preset_modes_{nullptr};  // owned by the Fan entity
};
static_assert(std::is_trivially_copyable_v<FanTraits>, "FanTraits is returned by value from get_traits()");

}  // namespace esphome::fan
