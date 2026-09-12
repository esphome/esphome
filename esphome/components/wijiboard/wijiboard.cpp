#include "wijiboard.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>

namespace esphome::wijiboard {

static const char *const TAG = "wijiboard";

static constexpr float RADIANS_TO_DEGREES = 180.0f / static_cast<float>(M_PI);

/// Upper case an ASCII letter; everything else is passed through untouched.
static char to_upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c; }

void WijiBoard::setup() {
  if (this->home_on_boot_) {
    this->start_homing_();
    return;
  }
  // Without a homing run we have to trust that the planchette was parked at the rest position.
  this->homed_ = true;
  this->teleport_to_(this->rest_position_1_, this->rest_position_2_);
  this->go_idle_();
}

void WijiBoard::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  switch (this->state_) {
    case WijiBoardState::WIJIBOARD_STATE_IDLE:
      this->disable_loop();
      break;

    case WijiBoardState::WIJIBOARD_STATE_HOMING:
      if (this->steppers_arrived_())
        this->advance_homing_();
      break;

    case WijiBoardState::WIJIBOARD_STATE_MOVING:
      if (this->steppers_arrived_()) {
        this->state_ = WijiBoardState::WIJIBOARD_STATE_HOLDING;
        this->wait_start_ = now;
        this->wait_time_ = this->hold_time_;
      }
      break;

    case WijiBoardState::WIJIBOARD_STATE_HOLDING:
      if (now - this->wait_start_ >= this->wait_time_) {
        if (this->single_move_ || this->return_home_between_letters_ || this->word_index_ >= this->word_length_) {
          this->start_return_();
        } else {
          this->state_ = WijiBoardState::WIJIBOARD_STATE_PAUSING;
          this->wait_start_ = now;
          this->wait_time_ = this->letter_pause_;
        }
      }
      break;

    case WijiBoardState::WIJIBOARD_STATE_RETURNING:
      if (this->steppers_arrived_()) {
        if (this->single_move_ || this->word_length_ == 0) {
          // A one-off move or an explicit stop; there is no word left to spell.
          this->single_move_ = false;
          this->home_callback_.call();
          this->go_idle_();
        } else if (this->word_index_ >= this->word_length_) {
          this->finish_word_();
        } else {
          this->state_ = WijiBoardState::WIJIBOARD_STATE_PAUSING;
          this->wait_start_ = now;
          this->wait_time_ = this->letter_pause_;
        }
      }
      break;

    case WijiBoardState::WIJIBOARD_STATE_PAUSING:
      if (now - this->wait_start_ >= this->wait_time_)
        this->advance_word_();
      break;
  }
}

void WijiBoard::dump_config() {
  ESP_LOGCONFIG(TAG,
                "WijiBoard:\n"
                "  Base separation: %.1f mm\n"
                "  Upper arm: %.1f mm\n"
                "  Forearm: %.1f mm\n"
                "  Step angle: %.5f deg\n"
                "  Rest position: %" PRId32 ", %" PRId32 "\n"
                "  Hold time: %" PRIu32 " ms\n"
                "  Letter pause: %" PRIu32 " ms\n"
                "  Space pause: %" PRIu32 " ms\n"
                "  Return home between letters: %s\n"
                "  Home on boot: %s\n"
                "  Characters: %u\n"
                "  Maximum word length: %u",
                this->half_base_ * 2.0f, this->upper_arm_length_, this->forearm_length_, this->step_angle_,
                this->rest_position_1_, this->rest_position_2_, this->hold_time_, this->letter_pause_,
                this->space_pause_, YESNO(this->return_home_between_letters_), YESNO(this->home_on_boot_),
                this->letter_count_, static_cast<unsigned>(WIJIBOARD_MAX_WORD_LENGTH));
}

void WijiBoard::write_word(const char *word, size_t length) {
  this->word_length_ = 0;
  this->word_index_ = 0;
  this->single_move_ = false;

  for (size_t i = 0; i < length && this->word_length_ < WIJIBOARD_MAX_WORD_LENGTH; i++) {
    const char c = word[i];
    // Collapse every run of blanks into the single pause the board can show.
    if (c == '\r' || c == '\n' || c == '\t') {
      this->word_[this->word_length_++] = ' ';
    } else {
      this->word_[this->word_length_++] = to_upper(c);
    }
  }
  if (length > WIJIBOARD_MAX_WORD_LENGTH) {
    ESP_LOGW(TAG, "Word truncated to %u characters", static_cast<unsigned>(WIJIBOARD_MAX_WORD_LENGTH));
  }

  this->word_start_callback_.call(std::string(this->word_.data(), this->word_length_));

  if (!this->homed_) {
    // Homing is still running, so hold on to the word and spell it once the arms know where they are.
    this->word_pending_ = true;
    return;
  }
  this->enable_loop();
  this->advance_word_();
}

void WijiBoard::write_letter(char letter) {
  WijiBoardAngles angles{};
  if (!this->resolve_letter_(letter, angles))
    return;
  if (!this->homed_) {
    ESP_LOGW(TAG, "Not homed yet, ignoring letter");
    return;
  }
  this->word_length_ = 0;
  this->word_index_ = 0;
  this->single_move_ = true;
  this->letter_callback_.call(static_cast<uint8_t>(to_upper(letter)));
  this->enable_loop();
  this->start_move_(angles);
}

void WijiBoard::goto_xy(float x, float y) {
  WijiBoardAngles angles{};
  if (!this->calculate_inverse_kinematics_(x, y, angles)) {
    ESP_LOGW(TAG, "Coordinate (%.1f, %.1f) is out of reach", x, y);
    return;
  }
  this->goto_angles(angles.theta1, angles.theta2);
}

void WijiBoard::goto_angles(float theta1, float theta2) {
  if (!this->homed_) {
    ESP_LOGW(TAG, "Not homed yet, ignoring move");
    return;
  }
  this->word_length_ = 0;
  this->word_index_ = 0;
  this->single_move_ = true;
  this->enable_loop();
  this->start_move_(WijiBoardAngles{theta1, theta2});
}

void WijiBoard::home() {
  this->word_length_ = 0;
  this->word_index_ = 0;
  this->single_move_ = false;
  this->word_pending_ = false;
  this->start_homing_();
}

void WijiBoard::stop() {
  this->word_length_ = 0;
  this->word_index_ = 0;
  this->single_move_ = false;
  this->word_pending_ = false;
  if (this->state_ == WijiBoardState::WIJIBOARD_STATE_HOMING)
    return;
  this->enable_loop();
  this->start_return_();
}

bool WijiBoard::resolve_letter_(char letter, WijiBoardAngles &angles) const {
  const char key = to_upper(letter);
  for (uint8_t i = 0; i < this->letter_count_; i++) {
    const WijiBoardLetter &entry = this->letters_[i];
    if (entry.key != key)
      continue;
    if (entry.is_angles) {
      angles = WijiBoardAngles{entry.a, entry.b};
      return true;
    }
    if (!this->calculate_inverse_kinematics_(entry.a, entry.b, angles)) {
      ESP_LOGW(TAG, "Character '%c' maps to a point the arms cannot reach", key);
      return false;
    }
    return true;
  }
  ESP_LOGD(TAG, "Character '%c' is not on the board, skipping", key);
  return false;
}

bool WijiBoard::calculate_inverse_kinematics_(float x, float y, WijiBoardAngles &angles) const {
  const float l1 = this->upper_arm_length_;
  const float l2 = this->forearm_length_;
  const float x_minus = x - this->half_base_;
  const float x_plus = x + this->half_base_;

  // Left arm: distance from its shoulder to the target, then the elbow angle that spans it.
  const float s = std::sqrt(x_minus * x_minus + y * y);
  const float t = std::sqrt(x_plus * x_plus + y * y);
  if (s <= 0.0f || t <= 0.0f)
    return false;

  const float cos_w1 = (l2 * l2 - s * s - l1 * l1) / (-2.0f * l1 * s);
  const float cos_w2 = (l2 * l2 - t * t - l1 * l1) / (-2.0f * l1 * t);
  if (cos_w1 < -1.0f || cos_w1 > 1.0f || cos_w2 < -1.0f || cos_w2 > 1.0f)
    return false;

  angles.theta1 = (std::atan2(y, x_minus) - std::acos(cos_w1)) * RADIANS_TO_DEGREES;
  angles.theta2 = (std::atan2(y, x_plus) + std::acos(cos_w2)) * RADIANS_TO_DEGREES;
  return true;
}

void WijiBoard::start_move_(const WijiBoardAngles &angles) {
  // The arms are cross-linked: stepper 1 carries the right hand angle and vice versa.
  const int32_t target_1 = -static_cast<int32_t>(std::lround(angles.theta2 / this->step_angle_));
  const int32_t target_2 = -static_cast<int32_t>(std::lround(angles.theta1 / this->step_angle_));
  ESP_LOGV(TAG, "Moving to %.2f/%.2f deg (%" PRId32 "/%" PRId32 " steps)", angles.theta1, angles.theta2, target_1,
           target_2);
  this->set_targets_(target_1, target_2);
  this->state_ = WijiBoardState::WIJIBOARD_STATE_MOVING;
}

void WijiBoard::start_return_() {
  this->set_targets_(this->rest_position_1_, this->rest_position_2_);
  this->state_ = WijiBoardState::WIJIBOARD_STATE_RETURNING;
}

void WijiBoard::set_targets_(int32_t target_1, int32_t target_2) {
  this->stepper_1_->set_target(target_1);
  this->stepper_2_->set_target(target_2);
}

void WijiBoard::teleport_to_(int32_t position_1, int32_t position_2) {
  this->stepper_1_->report_position(position_1);
  this->stepper_2_->report_position(position_2);
  this->set_targets_(position_1, position_2);
}

bool WijiBoard::steppers_arrived_() const {
  return this->stepper_1_->has_reached_target() && this->stepper_2_->has_reached_target();
}

void WijiBoard::start_homing_() {
  ESP_LOGD(TAG, "Homing");
  this->homed_ = false;
  this->homing_step_ = 0;
  this->teleport_to_(0, 0);
  this->set_targets_(this->homing_positions_[0], this->homing_positions_[1]);
  this->state_ = WijiBoardState::WIJIBOARD_STATE_HOMING;
  this->enable_loop();
}

void WijiBoard::advance_homing_() {
  switch (this->homing_step_) {
    case 0:
      // Second arm has reached its stop, so call that zero and drive both back against the other side.
      this->stepper_2_->report_position(0);
      this->set_targets_(this->homing_positions_[2], this->homing_positions_[3]);
      this->homing_step_ = 1;
      break;
    case 1:
      this->stepper_1_->report_position(0);
      this->set_targets_(this->homing_positions_[4], this->homing_positions_[5]);
      this->homing_step_ = 2;
      break;
    default:
      // Both arms are now at the known rest pose.
      this->teleport_to_(this->rest_position_1_, this->rest_position_2_);
      this->homed_ = true;
      ESP_LOGD(TAG, "Homing finished");
      this->home_callback_.call();
      if (this->word_pending_) {
        this->word_pending_ = false;
        this->word_index_ = 0;
        this->advance_word_();
      } else {
        this->go_idle_();
      }
      break;
  }
}

void WijiBoard::advance_word_() {
  while (this->word_index_ < this->word_length_) {
    const char c = this->word_[this->word_index_++];
    if (c == ' ') {
      this->state_ = WijiBoardState::WIJIBOARD_STATE_PAUSING;
      this->wait_start_ = App.get_loop_component_start_time();
      this->wait_time_ = this->space_pause_;
      return;
    }
    WijiBoardAngles angles{};
    if (!this->resolve_letter_(c, angles))
      continue;
    ESP_LOGD(TAG, "Letter '%c' (%u/%u)", c, this->word_index_, this->word_length_);
    this->letter_callback_.call(static_cast<uint8_t>(c));
    this->start_move_(angles);
    return;
  }
  // Nothing left to spell; park the planchette, which ends the sequence.
  this->start_return_();
}

void WijiBoard::finish_word_() {
  ESP_LOGD(TAG, "Word finished");
  this->word_length_ = 0;
  this->word_index_ = 0;
  this->word_end_callback_.call();
  this->go_idle_();
}

void WijiBoard::go_idle_() {
  this->state_ = WijiBoardState::WIJIBOARD_STATE_IDLE;
  this->disable_loop();
}

}  // namespace esphome::wijiboard
