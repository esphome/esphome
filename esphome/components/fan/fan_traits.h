#pragma once

#include <cstring>
#include <vector>
#include <initializer_list>
#include "esphome/core/helpers.h"

namespace esphome::fan {

class Fan;  // Forward declaration

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
  // Compat: returns const ref with empty fallback. In 2026.11.0 change to return const vector *.
  const std::vector<const char *> &supported_preset_modes() const;
  // Remove before 2026.11.0
  ESPDEPRECATED("Call set_supported_preset_modes() on the Fan entity instead. Removed in 2026.11.0", "2026.5.0")
  void set_supported_preset_modes(std::initializer_list<const char *> preset_modes) {
    // Compat: store in owned vector. Copies copy the vector (deprecated path still copies this vector).
    this->compat_preset_modes_ = preset_modes;
  }
  // Remove before 2026.11.0
  ESPDEPRECATED("Call set_supported_preset_modes() on the Fan entity instead. Removed in 2026.11.0", "2026.5.0")
  void set_supported_preset_modes(const std::vector<const char *> &preset_modes) {
    this->compat_preset_modes_ = preset_modes;
  }

  // Deleted overloads to catch incorrect std::string usage at compile time with clear error messages
  void set_supported_preset_modes(const std::vector<std::string> &preset_modes) = delete;
  void set_supported_preset_modes(std::initializer_list<std::string> preset_modes) = delete;

  /// Return if preset modes are supported
  bool supports_preset_modes() const {
    // Same precedence as supported_preset_modes() getter
    if (this->preset_modes_) {
      return !this->preset_modes_->empty();
    }
    return !this->compat_preset_modes_.empty();
  }
  /// Find and return the matching preset mode pointer from supported modes, or nullptr if not found.
  const char *find_preset_mode(const char *preset_mode) const {
    return this->find_preset_mode(preset_mode, preset_mode ? strlen(preset_mode) : 0);
  }
  const char *find_preset_mode(const char *preset_mode, size_t len) const {
    if (preset_mode == nullptr || len == 0) {
      return nullptr;
    }
    // Check pointer-based storage (new path) then compat owned vector (deprecated path)
    const auto &modes = this->preset_modes_ ? *this->preset_modes_ : this->compat_preset_modes_;
    for (const char *mode : modes) {
      if (strncmp(mode, preset_mode, len) == 0 && mode[len] == '\0') {
        return mode;
      }
    }
    return nullptr;
  }

 protected:
  /// Set the preset modes pointer (only Fan::wire_preset_modes_() should call this).
  void set_supported_preset_modes_(const std::vector<const char *> *preset_modes) {
    this->preset_modes_ = preset_modes;
  }

  bool oscillation_{false};
  bool speed_{false};
  bool direction_{false};
  int speed_count_{};
  const std::vector<const char *> *preset_modes_{nullptr};
  // Compat: owned storage for deprecated setters. Copies copy the vector (copies include this vector).
  // Remove in 2026.11.0.
  std::vector<const char *> compat_preset_modes_;
};

}  // namespace esphome::fan
