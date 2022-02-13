#pragma once

namespace esphome {
namespace cover {

class CoverTraits {
 public:
  CoverTraits() = default;

  bool get_is_assumed_state() const { return this->is_assumed_state_; }
  void set_is_assumed_state(bool is_assumed_state) { this->is_assumed_state_ = is_assumed_state; }
  bool get_supports_position() const { return this->supports_position_; }
  void set_supports_position(bool supports_position) { this->supports_position_ = supports_position; }
  bool get_supports_tilt() const { return this->supports_tilt_; }
  void set_supports_tilt(bool supports_tilt) { this->supports_tilt_ = supports_tilt; }
  bool get_supports_toggle() const { return this->supports_toggle_; }
  void set_supports_toggle(bool supports_toggle) { this->supports_toggle_ = supports_toggle; }

 protected:
  bool is_assumed_state_{false};
  bool supports_position_{false};
  bool supports_tilt_{false};
  bool supports_toggle_{false};
};

}  // namespace cover
}  // namespace esphome
