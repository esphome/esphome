#pragma once

namespace esphome {
namespace fan {

class FanTraits {
 public:
  FanTraits() = default;
  FanTraits(bool oscillation, bool speed) : oscillation_(false), speed_(speed) {}

  /// Return if this fan supports oscillation.
  bool supports_oscillation() const { return this->oscillation_; }
  /// Set whether this fan supports oscillation.
  void set_oscillation(bool oscillation) { this->oscillation_ = oscillation; }
  /// Return if this fan supports speed modes.
  bool supports_speed() const { return this->speed_; }
  /// Set whether this fan supports speed modes.
  void set_speed(bool speed) { this->speed_ = speed; }

 protected:
  bool oscillation_{false};
  bool speed_{false};
};

}  // namespace fan
}  // namespace esphome
