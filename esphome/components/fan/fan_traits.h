#include <set>
#include <utility>

#pragma once

namespace esphome {

#ifdef USE_API
namespace api {
class APIConnection;
}  // namespace api
#endif

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
  std::set<std::string> supported_preset_modes() const { return this->preset_modes_; }
  /// Set the preset modes supported by the fan.
  void set_supported_preset_modes(const std::set<std::string> &preset_modes) { this->preset_modes_ = preset_modes; }
  /// Return if preset modes are supported
  bool supports_preset_modes() const { return !this->preset_modes_.empty(); }

 protected:
#ifdef USE_API
  // The API connection is a friend class to access internal methods
  friend class api::APIConnection;
  // This method returns a reference to the internal preset modes set.
  // It is used by the API to avoid copying data when encoding messages.
  // Warning: Do not use this method outside of the API connection code.
  // It returns a reference to internal data that can be invalidated.
  const std::set<std::string> &supported_preset_modes_for_api_() const { return this->preset_modes_; }
#endif
  bool oscillation_{false};
  bool speed_{false};
  bool direction_{false};
  int speed_count_{};
  std::set<std::string> preset_modes_{};
};

}  // namespace fan
}  // namespace esphome
