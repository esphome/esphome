#include "hbridge.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cmath>

namespace esphome {
namespace hbridge {

static const char *const TAG = "hbridge";

void HBridge::setup() {
  // Always start in off-state
  set_state(HBridgeMode::OFF, 0);
}

void HBridge::loop() {
  float new_dutycycle = 0;
  bool dutycycle_transition_done = false;

  // If we are in a transition
  if (this->transition_state_ != TransitionState::OFF) {
    // Calculate the time between this iteration and the previous one
    uint32_t mode_duration = millis() - this->transition_mode_start_time_;
    uint32_t ms_since_last_step = millis() - this->transition_last_step_time_;

    switch (this->transition_state_) {
      case TransitionState::RAMP_DOWN:
        // Apply next step
        new_dutycycle =
            this->current_mode_dutycycle_ - (this->transition_rampup_dutycycle_delta_per_ms_ * ms_since_last_step);
        dutycycle_transition_done = false;

        if (this->current_mode_ != this->transition_target_mode_) {
          // Range limit to 0
          if (new_dutycycle <= 0) {
            new_dutycycle = this->transition_target_mode_dutycycle_;
            dutycycle_transition_done = true;
          }
        } else {
          // Range limit by target dutycycle
          if (new_dutycycle <= this->transition_target_mode_dutycycle_) {
            new_dutycycle = this->transition_target_mode_dutycycle_;
            dutycycle_transition_done = true;
          }
        }

        // Set output
        set_output_state_(this->current_mode_, this->transition_target_mode_dutycycle_);

        // If reason to go to next mode
        if (dutycycle_transition_done) {
          // Determine next mode
          if (this->transition_shorting_buildup_duration_ms_ > 0) {
            // We need to buildup a short
            this->transition_state_ = TransitionState::SHORTING_BUILDUP;
            ESP_LOGD(TAG, "Transition mode (ramp-down > short buildup)");
          } else if (this->transition_short_duration_ms_ > 0) {
            // We need to full short for a while
            this->transition_state_ = TransitionState::FULL_SHORT;
            ESP_LOGD(TAG, "Transition mode (ramp-down > full short)");
          } else if (this->transition_target_mode_ != this->current_mode_) {
            // We need to transition to new (active) dutycycle
            this->transition_state_ = TransitionState::RAMP_UP;
            ESP_LOGD(TAG, "Transition mode (ramp-down > ramp-up)");
          } else {
            // We have reached our target (mode)
            this->transition_state_ = TransitionState::OFF;
            ESP_LOGD(TAG, "Transition done (ramp-down)");
          }
          this->transition_mode_start_time_ = millis();
        }
        break;

      case TransitionState::SHORTING_BUILDUP:
        // Apply next step
        this->transition_shorting_dutycycle_ +=
            (this->transition_shorting_dutycycle_delta_per_ms_ * ms_since_last_step);
        set_output_state_(HBridgeMode::SHORT, this->transition_shorting_dutycycle_);

        // If reason to go to next mode (Timeout, or target duty cycle reached)
        if (mode_duration > this->transition_shorting_buildup_duration_ms_ ||
            (this->transition_target_mode_ == HBridgeMode::SHORT &&
             this->transition_shorting_dutycycle_ >= this->transition_target_mode_dutycycle_)) {
          // Determine next mode
          if (this->transition_short_duration_ms_ > 0) {
            // We need to full short for a while
            this->transition_state_ = TransitionState::FULL_SHORT;
            ESP_LOGD(TAG, "Transition mode (short buildup > short)");
          } else if (this->transition_target_mode_ != this->current_mode_) {
            // We need to transition to new (active) dutycycle
            this->transition_state_ = TransitionState::RAMP_UP;
            ESP_LOGD(TAG, "Transition mode (short buildup > ramp-up)");
          } else {
            // We have reached our target mode
            this->transition_state_ = TransitionState::OFF;
            ESP_LOGD(TAG, "Transition done (buildup short)");
          }
          this->transition_mode_start_time_ = millis();
        }
        break;

      case TransitionState::FULL_SHORT:
        // Apply full short
        set_output_state_(HBridgeMode::SHORT, 1);

        // If reason to go to next mode (timeout)
        if (mode_duration > this->transition_short_duration_ms_) {
          if (this->transition_target_mode_ != this->current_mode_) {
            // We need to transition to new (active) dutycycle
            this->transition_state_ = TransitionState::RAMP_UP;
            ESP_LOGD(TAG, "Transition mode (short > ramp-up)");
          } else {
            // We have reached our target mode
            this->transition_state_ = TransitionState::OFF;
            ESP_LOGD(TAG, "Transition done (short)");
          }
          this->transition_mode_start_time_ = millis();
        }
        break;

      case TransitionState::RAMP_UP:
        // Apply next step
        new_dutycycle =
            this->current_mode_dutycycle_ + (this->transition_rampup_dutycycle_delta_per_ms_ * ms_since_last_step);
        dutycycle_transition_done = false;

        // Range limit by target dutycycle
        if (new_dutycycle >= this->transition_target_mode_dutycycle_) {
          new_dutycycle = this->transition_target_mode_dutycycle_;
          dutycycle_transition_done = true;
        }

        // Set output
        set_output_state_(this->transition_target_mode_, this->transition_target_mode_dutycycle_);

        // If reason to go to next mode
        if (dutycycle_transition_done) {
          // Set final mode
          this->transition_state_ = TransitionState::OFF;
          ESP_LOGD(TAG, "Transition done (ramp-up)");
        }
        break;

      default:
      case TransitionState::OFF:
        // Nothing to do here (Supress warning)
        break;
    }

    // Store last step time
    this->transition_last_step_time_ = millis();
  }
}

void HBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "HBridge:");
  ESP_LOGCONFIG(TAG, "   Ramp-up time %d ms", this->setting_full_rampup_time_ms_);
  ESP_LOGCONFIG(TAG, "   Ramp-down time %d ms", this->setting_full_rampdown_time_ms_);
  ESP_LOGCONFIG(TAG, "   Short-buildup (brake) time %d ms", this->setting_short_buildup_time_ms_);
  ESP_LOGCONFIG(TAG, "   Short (brake) time %d ms", this->setting_short_time_ms_);
  ESP_LOGCONFIG(TAG, "   Min. dutycycle: %f ", this->setting_min_dutycycle);
}

void HBridge::transition_to_state(HBridgeMode target_mode, float target_dutycycle, uint32_t full_rampup_duration_ms = 0,
                                  uint32_t full_rampdown_duration_ms = 0, uint32_t short_buildup_duration_ms = 0,
                                  uint32_t short_duration_ms = 0) {
  // --- Behaviour Examples:
  // Note: In slow-current-decay mode the short-buildup is skipped, as it is already shorted in the off-time
  //
  // ### Transition - Direction A 100% to Direction A 20% (No shorting is applied regardless of values) ###
  // |             A100% >> A0%                |
  // |< ((full_rampdown_duration_ms/100)*(100-20)) >|
  //
  // ### Transition - Direction B 30% to Direction B 80% (No shorting is applied regardless of values) ###
  // |             B0% >> B80%              |
  // |< ((full_rampup_duration_ms/100)*(80-30)) >|
  //
  // ### Transition - Direction A 100% to Direction B 100%, No shorting (duration set to 0) ###
  // |              A100% >> A0%              |             B0% >> B100%             |
  // |< ((full_rampdown_duration_ms/100)*(100-0)) >|
  //                                          |< ((full_rampup_duration_ms/100)*(100-0)) >|
  //
  // ### Transition - Direction A 100% to 0%, With shorting (duration set > 0) ###
  // |              A100% >> A0%              |    Short 0% >> Short 100%   |    Shorting 100%    |
  // |< ((full_rampdown_duration_ms/100)*(100-0)) >|
  //                                          |< short_buildup_duration_ms >|
  //                                                                        |< short_duration_ms >|
  //
  // ### Transition - Direction A 100% to Direction B 100%, With shorting (duration set > 0) ###
  // Part 1:
  //  |              A100% >> A0%                 |   Short 0% >> Short 100%  | (Part 2)
  //  |<((full_rampdown_duration_ms/100)*(100-0))>|
  //                                              |<short_buildup_duration_ms>|
  //
  // Part 2:
  //  (Part 1) |   Shorting 100%   |            B0% >> B100%                 |
  //           |<short_duration_ms>|
  //                               |<((full_rampup_duration_ms/100)*(100-0))>|

  // Store targets
  this->transition_target_mode_ = target_mode;
  this->transition_target_mode_dutycycle_ = target_dutycycle;

  // Transition step flags
  bool transition_crosses_zero_point_with_shorting = false;
  bool transition_rampdown = false;
  bool transition_rampup = false;

  // Check if transition crosses zero point and shorting procedure is wished by the given function arguments
  if ((this->current_mode_ != this->transition_target_mode_) &&
      (short_buildup_duration_ms > 0 || short_duration_ms > 0)) {
    // Enable (possible) shorting for this procedure
    transition_crosses_zero_point_with_shorting = true;
    this->transition_shorting_buildup_duration_ms_ = short_buildup_duration_ms;
    this->transition_short_duration_ms_ = short_duration_ms;

    // Calculate shorting buildup step size (Skip in slow-current-decay mode as it is already shorted in the off-time)
    if (this->transition_shorting_buildup_duration_ms_ > 0 && this->current_decay_mode_ != CurrentDecayMode::SLOW) {
      if (this->transition_target_mode_ == HBridgeMode::SHORT) {
        // If short is our target mode, transition to given dutycycle
        this->transition_shorting_dutycycle_delta_per_ms_ =
            (float) (this->transition_target_mode_dutycycle_ / (float) this->transition_shorting_buildup_duration_ms_);
      } else {
        // Otherwise transition to full short
        this->transition_shorting_dutycycle_delta_per_ms_ =
            (float) (1.0f / (float) this->transition_shorting_buildup_duration_ms_);
      }
    } else {
      // No shorting buildup
      this->transition_shorting_dutycycle_delta_per_ms_ = 0;
    }

    ESP_LOGD(TAG, "Transition: Short - duration: %d ms (buildup in: %d ms (%f/ms)), ",
             this->transition_short_duration_ms_, this->transition_shorting_buildup_duration_ms_,
             this->transition_shorting_dutycycle_delta_per_ms_);
  } else {
    // No shorting procedure for this transition
    transition_crosses_zero_point_with_shorting = false;
    this->transition_shorting_buildup_duration_ms_ = 0;
    this->transition_short_duration_ms_ = 0;
    this->transition_shorting_dutycycle_delta_per_ms_ = 0;
  }

  // Calculate ramp-down
  if ((setting_full_rampdown_time_ms_ > 0) &&
      (this->transition_target_mode_ == HBridgeMode::DIRECTION_A ||
       (this->transition_target_mode_ == HBridgeMode::DIRECTION_B)) &&
      (((this->current_mode_ != this->transition_target_mode_) && (this->current_mode_dutycycle_ > 0)) ||
       ((this->current_mode_ == this->transition_target_mode_) &&
        (this->transition_target_mode_dutycycle_ < this->current_mode_dutycycle_)))) {
    // Calculate duration of this ramp-down by the duration of a full ramp-down
    float dutyCycleChange = 0;
    if (this->transition_target_mode_ == this->current_mode_) {
      dutyCycleChange = (this->current_mode_dutycycle_ - this->transition_target_mode_dutycycle_);
    } else {
      dutyCycleChange = (this->current_mode_dutycycle_);
    }
    uint32_t rampDownDuration =
        (uint32_t) (((float) setting_full_rampdown_time_ms_ / (float) 100) * (dutyCycleChange * 100));

    // Calculate the ramp-down step per ms
    transition_rampdown_dutycycle_delta_per_ms_ = dutyCycleChange / (float) rampDownDuration;
    transition_rampdown = true;

    ESP_LOGD(TAG, "Transition: Ramp-down - change: %f = %.0f%% duration: %d ms (%f/ms)), ", dutyCycleChange,
             (dutyCycleChange * 100), rampDownDuration, transition_rampdown_dutycycle_delta_per_ms_);
  }
  // No ramp-down in this transition
  else {
    transition_rampdown_dutycycle_delta_per_ms_ = 0;
    transition_rampdown = false;
  }

  // Calculate ramp-up
  if ((setting_full_rampup_time_ms_ > 0) &&
      (this->transition_target_mode_ == HBridgeMode::DIRECTION_A ||
       (this->transition_target_mode_ == HBridgeMode::DIRECTION_B)) &&
      (((this->current_mode_ != this->transition_target_mode_) && (this->transition_target_mode_dutycycle_ > 0)) ||
       ((this->current_mode_ == this->transition_target_mode_) &&
        (this->transition_target_mode_dutycycle_ > this->current_mode_dutycycle_)))) {
    // Calculate duration of this ramp-up by the duration of a full ramp-up
    float dutyCycleChange = 0;
    if (this->transition_target_mode_ == this->current_mode_) {
      dutyCycleChange = (this->transition_target_mode_dutycycle_ - this->current_mode_dutycycle_);
    } else {
      dutyCycleChange = (this->transition_target_mode_dutycycle_);
    }
    uint32_t rampUpDuration =
        (uint32_t) (((float) setting_full_rampup_time_ms_ / (float) 100) * (dutyCycleChange * 100));

    // Calculate the ramp-up step per ms
    transition_rampup_dutycycle_delta_per_ms_ = dutyCycleChange / (float) rampUpDuration;
    transition_rampup = true;

    ESP_LOGD(TAG, "Transition: Ramp-up - change: %f = %.0f%% duration: %d ms (%f/ms)), ", dutyCycleChange,
             (dutyCycleChange * 100), rampUpDuration, transition_rampup_dutycycle_delta_per_ms_);
  }
  // No ramp-up in this transition
  else {
    transition_rampup_dutycycle_delta_per_ms_ = 0;
    transition_rampup = false;
  }

  // --- Determine first step of transition ----
  if (transition_rampdown) {
    // If we need a ramp-down start with that
    // Set state
    this->transition_state_ = TransitionState::RAMP_DOWN;
  }
  if (transition_crosses_zero_point_with_shorting && this->transition_shorting_buildup_duration_ms_ > 0) {
    // If the transition start with shorting buildup, disable output now
    set_output_state_(HBridgeMode::OFF, 0);

    // Set start state
    this->transition_shorting_dutycycle_ = 0;
    this->transition_state_ = TransitionState::SHORTING_BUILDUP;
  } else if (transition_crosses_zero_point_with_shorting && this->transition_short_duration_ms_ > 0) {
    // If the transition starts with full short, short output now
    set_output_state_(HBridgeMode::SHORT, 1);

    // Set start state
    this->transition_shorting_buildup_duration_ms_ = 0;
    this->transition_shorting_dutycycle_ = 1;
    this->transition_state_ = TransitionState::FULL_SHORT;
  } else if (transition_rampup) {
    // Otherwise if we need a ramp-up start with that
    // Set state
    this->transition_shorting_buildup_duration_ms_ = 0;
    this->transition_short_duration_ms_ = 0;
    this->transition_state_ = TransitionState::RAMP_DOWN;
  } else {
    // This is actually not a transition, just a hard state change.

    // call-off transition
    this->transition_state_ = TransitionState::OFF;
    this->transition_shorting_buildup_duration_ms_ = 0;
    this->transition_short_duration_ms_ = 0;
    this->transition_shorting_dutycycle_delta_per_ms_ = 0;

    ESP_LOGD(TAG, "Transition: Omitted, setting state");

    // Set new state
    set_state(target_mode, target_dutycycle);
  }

  if (this->transition_state_ != TransitionState::OFF) {
    // Set mode start time
    this->transition_mode_start_time_ = millis();
    this->transition_last_step_time_ = millis();

    ESP_LOGD(TAG, "Transition from mode:%d-%f to mode:%d-%f", this->current_mode_, this->current_mode_dutycycle_,
             this->transition_target_mode_, this->transition_target_mode_dutycycle_);
  }
}

void HBridge::set_state(HBridgeMode mode, float dutycycle) {
  float outputDutycycle = dutycycle;

  // Print info on mode change
  ESP_LOGD(TAG, "Set mode %d - dutycycle: %.2f");

  // Cancel possible ongoing transition
  this->transition_state_ = TransitionState::OFF;

  // Set state to outputs
  set_output_state_(mode, dutycycle);
}

void HBridge::set_output_state_(HBridgeMode mode, float dutycycle) {
  float clipped_dutycycle = dutycycle;
  HBridgeMode output_mode = mode;
  float output_dutycycle = dutycycle;

  if (clipped_dutycycle <= 0 || mode == HBridgeMode::OFF) {
    // If dutycycle is 0 or negative, force off
    output_mode = HBridgeMode::OFF;
    clipped_dutycycle = 0;
  } else if (clipped_dutycycle > 1.0f) {
    // If dutycycle is above 1, cap to 1
    clipped_dutycycle = 1.0f;
  }

  // If minimum dutycycle is set, calculate (relative) 0 to 1 range over the new (shortened) absolute range
  if (setting_min_dutycycle > 0 &&
      (output_mode == HBridgeMode::DIRECTION_A || output_mode == HBridgeMode::DIRECTION_B)) {
    output_dutycycle = (((1 - setting_min_dutycycle) / 1000) * (clipped_dutycycle * 1000)) + setting_min_dutycycle;
  }

  // Set pin states + duty cycle according to mode

  switch (output_mode) {
    case HBridgeMode::OFF:
      if (this->enable_pin_ != nullptr) {
        this->enable_pin_->set_level(0);
      }
      this->pin_a_->set_level(0);
      this->pin_b_->set_level(0);
      break;

    case HBridgeMode::DIRECTION_A:
      // Set states dependent on current decay mode
      if (current_decay_mode_ == CurrentDecayMode::SLOW) {
        this->pin_b_->set_level(1.0f - output_dutycycle);
        this->pin_a_->set_level(1.0f);
      } else if (current_decay_mode_ == CurrentDecayMode::FAST) {
        this->pin_b_->set_level(0);
        this->pin_a_->set_level(output_dutycycle);
      }

      if (this->enable_pin_ != nullptr) {
        this->enable_pin_->set_level(1);
      }
      break;

    case HBridgeMode::DIRECTION_B:
      // Set states dependent on current decay mode
      if (current_decay_mode_ == CurrentDecayMode::SLOW) {
        this->pin_a_->set_level(1.0f - output_dutycycle);
        this->pin_b_->set_level(1.0f);
      } else if (current_decay_mode_ == CurrentDecayMode::FAST) {
        this->pin_a_->set_level(0);
        this->pin_b_->set_level(output_dutycycle);
      }

      if (this->enable_pin_ != nullptr) {
        this->enable_pin_->set_level(1);
      }
      break;

    case HBridgeMode::SHORT:
      this->pin_a_->set_level(output_dutycycle);
      this->pin_b_->set_level(output_dutycycle);
      if (this->enable_pin_ != nullptr) {
        this->enable_pin_->set_level(1);
      }
      break;
  }

  // Store current values
  this->current_mode_ = output_mode;
  this->current_mode_dutycycle_ = clipped_dutycycle;
}

}  // namespace hbridge
}  // namespace esphome
