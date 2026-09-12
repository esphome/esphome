#pragma once

#include "esphome/components/stepper/stepper.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include <array>

namespace esphome::wijiboard {

/// One entry of the board's character map, generated into flash by the Python code generation.
struct WijiBoardLetter {
  /// The character this entry describes, always upper case.
  char key;
  /// When true, `a` and `b` are arm angles in degrees; when false they are board coordinates in mm.
  bool is_angles;
  float a;
  float b;
};

/// A pair of arm angles in degrees, as produced by the inverse kinematics.
struct WijiBoardAngles {
  float theta1;
  float theta2;
};

enum class WijiBoardState : uint8_t {
  /// Nothing to do; the loop is disabled.
  WIJIBOARD_STATE_IDLE,
  /// Running the multi step homing sequence against the mechanical stops.
  WIJIBOARD_STATE_HOMING,
  /// Both steppers are travelling towards the current target.
  WIJIBOARD_STATE_MOVING,
  /// Sitting on a letter so it can be read.
  WIJIBOARD_STATE_HOLDING,
  /// Travelling back to the rest position.
  WIJIBOARD_STATE_RETURNING,
  /// Waiting between two letters.
  WIJIBOARD_STATE_PAUSING,
};

class WijiBoard : public Component {
 public:
  WijiBoard(stepper::Stepper *stepper_1, stepper::Stepper *stepper_2) : stepper_1_(stepper_1), stepper_2_(stepper_2) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_letters(const WijiBoardLetter *letters, uint8_t letter_count) {
    this->letters_ = letters;
    this->letter_count_ = letter_count;
  }
  void set_geometry(float base_separation, float upper_arm_length, float forearm_length) {
    this->half_base_ = base_separation / 2.0f;
    this->upper_arm_length_ = upper_arm_length;
    this->forearm_length_ = forearm_length;
  }
  void set_steps_per_rotation(uint16_t steps) { this->step_angle_ = 360.0f / static_cast<float>(steps); }
  void set_rest_position(int32_t position_1, int32_t position_2) {
    this->rest_position_1_ = position_1;
    this->rest_position_2_ = position_2;
  }
  void set_homing_positions(const std::array<int32_t, 6> &positions) { this->homing_positions_ = positions; }
  void set_hold_time(uint32_t hold_time) { this->hold_time_ = hold_time; }
  void set_letter_pause(uint32_t letter_pause) { this->letter_pause_ = letter_pause; }
  void set_space_pause(uint32_t space_pause) { this->space_pause_ = space_pause; }
  void set_return_home_between_letters(bool value) { this->return_home_between_letters_ = value; }
  void set_home_on_boot(bool value) { this->home_on_boot_ = value; }

  template<typename F> void add_on_word_start_callback(F &&callback) {
    this->word_start_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_letter_callback(F &&callback) {
    this->letter_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_word_end_callback(F &&callback) {
    this->word_end_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_home_callback(F &&callback) { this->home_callback_.add(std::forward<F>(callback)); }

  /** Spell out a word, one letter at a time.
   *
   * Characters missing from the board are skipped and spaces become a pause. Anything past
   * WIJIBOARD_MAX_WORD_LENGTH characters is dropped. A word already in progress is abandoned.
   */
  void write_word(const char *word, size_t length);
  void write_word(const std::string &word) { this->write_word(word.c_str(), word.size()); }

  /// Point the planchette at a single character from the board's character map.
  void write_letter(char letter);

  /// Point the planchette at an arbitrary board coordinate in mm.
  void goto_xy(float x, float y);

  /// Drive both arms to the given angles in degrees, bypassing the inverse kinematics.
  void goto_angles(float theta1, float theta2);

  /// Re-run the homing sequence against the mechanical stops.
  void home();

  /// Abandon whatever is in progress and park the planchette at the rest position.
  void stop();

  bool is_busy() const { return this->state_ != WijiBoardState::WIJIBOARD_STATE_IDLE; }
  bool is_homed() const { return this->homed_; }

 protected:
  /// Look up a character and convert it to arm angles. Returns false when it cannot be reached.
  bool resolve_letter_(char letter, WijiBoardAngles &angles) const;
  /// Solve the five bar linkage for a board coordinate. Returns false when the point is out of reach.
  bool calculate_inverse_kinematics_(float x, float y, WijiBoardAngles &angles) const;

  void start_move_(const WijiBoardAngles &angles);
  void start_return_();
  void set_targets_(int32_t target_1, int32_t target_2);
  void teleport_to_(int32_t position_1, int32_t position_2);
  bool steppers_arrived_() const;

  void start_homing_();
  void advance_homing_();
  /// Move on to the next letter of the pending word, or finish the sequence.
  void advance_word_();
  void finish_word_();
  void go_idle_();

  stepper::Stepper *stepper_1_;
  stepper::Stepper *stepper_2_;

  const WijiBoardLetter *letters_{nullptr};
  uint8_t letter_count_{0};

  float half_base_{12.9f};
  float upper_arm_length_{85.0f};
  float forearm_length_{110.0f};
  float step_angle_{360.0f / 2048.0f};

  int32_t rest_position_1_{-1024};
  int32_t rest_position_2_{0};
  std::array<int32_t, 6> homing_positions_{{1024, 2048, -1050, -1300, 550, -530}};

  uint32_t hold_time_{500};
  uint32_t letter_pause_{200};
  uint32_t space_pause_{1000};
  uint32_t wait_start_{0};
  uint32_t wait_time_{0};

  std::array<char, WIJIBOARD_MAX_WORD_LENGTH> word_{};
  uint8_t word_length_{0};
  uint8_t word_index_{0};

  WijiBoardState state_{WijiBoardState::WIJIBOARD_STATE_IDLE};
  uint8_t homing_step_{0};
  bool homed_{false};
  bool home_on_boot_{true};
  bool return_home_between_letters_{true};
  /// Set while a single move is running so it does not get treated as a one letter word.
  bool single_move_{false};
  /// A word handed over before homing finished, replayed once the arms are homed.
  bool word_pending_{false};

  LazyCallbackManager<void(std::string)> word_start_callback_;
  LazyCallbackManager<void(uint8_t)> letter_callback_;
  LazyCallbackManager<void()> word_end_callback_;
  LazyCallbackManager<void()> home_callback_;
};

template<typename... Ts> class WriteWordAction final : public Action<Ts...> {
 public:
  explicit WriteWordAction(WijiBoard *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, word)

  void play(const Ts &...x) override { this->parent_->write_word(this->word_.value(x...)); }

 protected:
  WijiBoard *parent_;
};

template<typename... Ts> class WriteLetterAction final : public Action<Ts...> {
 public:
  explicit WriteLetterAction(WijiBoard *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, letter)

  void play(const Ts &...x) override {
    const std::string letter = this->letter_.value(x...);
    if (!letter.empty())
      this->parent_->write_letter(letter[0]);
  }

 protected:
  WijiBoard *parent_;
};

template<typename... Ts> class GotoXYAction final : public Action<Ts...> {
 public:
  explicit GotoXYAction(WijiBoard *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, x)
  TEMPLATABLE_VALUE(float, y)

  void play(const Ts &...x) override { this->parent_->goto_xy(this->x_.value(x...), this->y_.value(x...)); }

 protected:
  WijiBoard *parent_;
};

template<typename... Ts> class GotoAnglesAction final : public Action<Ts...> {
 public:
  explicit GotoAnglesAction(WijiBoard *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, theta1)
  TEMPLATABLE_VALUE(float, theta2)

  void play(const Ts &...x) override {
    this->parent_->goto_angles(this->theta1_.value(x...), this->theta2_.value(x...));
  }

 protected:
  WijiBoard *parent_;
};

template<typename... Ts> class HomeAction final : public Action<Ts...> {
 public:
  explicit HomeAction(WijiBoard *parent) : parent_(parent) {}

  void play(const Ts &...) override { this->parent_->home(); }

 protected:
  WijiBoard *parent_;
};

template<typename... Ts> class StopAction final : public Action<Ts...> {
 public:
  explicit StopAction(WijiBoard *parent) : parent_(parent) {}

  void play(const Ts &...) override { this->parent_->stop(); }

 protected:
  WijiBoard *parent_;
};

template<typename... Ts> class IsBusyCondition final : public Condition<Ts...> {
 public:
  explicit IsBusyCondition(WijiBoard *parent) : parent_(parent) {}

  bool check(const Ts &...) override { return this->parent_->is_busy(); }

 protected:
  WijiBoard *parent_;
};

}  // namespace esphome::wijiboard
