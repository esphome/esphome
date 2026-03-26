#pragma once

#include <cstring>
#include <vector>
#include <initializer_list>
#include "esphome/core/helpers.h"

namespace esphome {

namespace fan {

class FanTraits {
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
  /// Return the preset modes supported by the fan.
  const std::vector<const char *> &supported_preset_modes() const {
    return this->preset_modes_ ? *this->preset_modes_ : this->owned_preset_modes_;
  }
  /// Set the preset modes pointer (points to vector owned by Fan base class).
  void set_supported_preset_modes(const std::vector<const char *> *preset_modes) { this->preset_modes_ = preset_modes; }

  // Remove before 2027.1.0
  ESPDEPRECATED("Call set_supported_preset_modes() on the Fan entity instead. Removed in 2026.11.0", "2026.5.0")
  void set_supported_preset_modes(std::initializer_list<const char *> preset_modes) {
    this->owned_preset_modes_ = preset_modes;
    this->preset_modes_ = &this->owned_preset_modes_;
  }
  // Remove before 2027.1.0
  ESPDEPRECATED("Call set_supported_preset_modes() on the Fan entity instead. Removed in 2026.11.0", "2026.5.0")
  void set_supported_preset_modes(const std::vector<const char *> &preset_modes) {
    this->owned_preset_modes_ = preset_modes;
    this->preset_modes_ = &this->owned_preset_modes_;
  }

  /// Return if preset modes are supported
  bool supports_preset_modes() const {
    return (this->preset_modes_ && !this->preset_modes_->empty()) || !this->owned_preset_modes_.empty();
  }
  /// Find and return the matching preset mode pointer from supported modes, or nullptr if not found.
  const char *find_preset_mode(const char *preset_mode) const {
    return this->find_preset_mode(preset_mode, preset_mode ? strlen(preset_mode) : 0);
  }
  const char *find_preset_mode(const char *preset_mode, size_t len) const {
    if (preset_mode == nullptr || len == 0)
      return nullptr;
    const auto &modes = this->preset_modes_ ? *this->preset_modes_ : this->owned_preset_modes_;
    for (const char *mode : modes) {
      if (strncmp(mode, preset_mode, len) == 0 && mode[len] == '\0') {
        return mode;  // Return pointer from traits
      }
    }
    return nullptr;
  }

 protected:
  bool oscillation_{false};
  bool speed_{false};
  bool direction_{false};
  int speed_count_{};
  const std::vector<const char *> *preset_modes_{nullptr};
  std::vector<const char *> owned_preset_modes_{};  ///< Compat: used when old setters are called directly on traits
};

}  // namespace fan
}  // namespace esphome
