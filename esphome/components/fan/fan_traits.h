#pragma once

#include <cstring>
#include <vector>
#include <initializer_list>

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
  const std::vector<const char *> &supported_preset_modes() const { return this->preset_modes_; }
  /// Set the preset modes supported by the fan (from initializer list).
  void set_supported_preset_modes(std::initializer_list<const char *> preset_modes) {
    this->preset_modes_ = preset_modes;
  }
  /// Set the preset modes supported by the fan (from vector).
  void set_supported_preset_modes(const std::vector<const char *> &preset_modes) { this->preset_modes_ = preset_modes; }

  // Deleted overloads to catch incorrect std::string usage at compile time with clear error messages
  void set_supported_preset_modes(const std::vector<std::string> &preset_modes) = delete;
  void set_supported_preset_modes(std::initializer_list<std::string> preset_modes) = delete;

  /// Return if preset modes are supported
  bool supports_preset_modes() const { return !this->preset_modes_.empty(); }
  /// Find and return the matching preset mode pointer from supported modes, or nullptr if not found.
  const char *find_preset_mode(const char *preset_mode) const {
    if (preset_mode == nullptr)
      return nullptr;
    for (const char *mode : this->preset_modes_) {
      if (strcmp(mode, preset_mode) == 0) {
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
  std::vector<const char *> preset_modes_{};
};

}  // namespace fan
}  // namespace esphome
