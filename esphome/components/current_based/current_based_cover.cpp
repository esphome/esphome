#include "current_based_cover.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cfloat>

namespace esphome {
namespace current_based {

static const char *const TAG = "current_based.cover";

using namespace esphome::cover;

CoverTraits CurrentBasedCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(false);
  return traits;
}
void CurrentBasedCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    this->direction_idle_();
  }
  if (call.get_toggle().has_value()) {
    if (this->current_operation != COVER_OPERATION_IDLE) {
      this->start_direction_(COVER_OPERATION_IDLE);
      this->publish_state();
    } else {
      if (this->position == COVER_CLOSED || this->last_operation_ == COVER_OPERATION_CLOSING) {
        this->target_position_ = COVER_OPEN;
        this->start_direction_(COVER_OPERATION_OPENING);
      } else {
        this->target_position_ = COVER_CLOSED;
        this->start_direction_(COVER_OPERATION_CLOSING);
      }
    }
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (fabsf(this->position - pos) < 0.01) {
      // already at target
    } else {
      auto op = pos < this->position ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING;
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}
void CurrentBasedCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->position = 0.5f;
  }
}

void CurrentBasedCover::loop() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  const uint32_t now = App.get_loop_component_start_time();

  if (this->current_operation == COVER_OPERATION_OPENING) {
    if (this->malfunction_detection_ && this->is_closing_()) {  // Malfunction
      this->direction_idle_();
      this->malfunction_trigger_->trigger();
      ESP_LOGI(TAG, "'%s' - Malfunction detected during opening. Current flow detected in close circuit",
               this->name_.c_str());
    } else if (this->is_opening_blocked_()) {  // Blocked
      ESP_LOGD(TAG, "'%s' - Obstacle detected during opening.", this->name_.c_str());
      this->direction_idle_();
      if (this->obstacle_rollback_ != 0) {
        this->set_timeout("rollback", 300, [this]() {
          ESP_LOGD(TAG, "'%s' - Rollback.", this->name_.c_str());
          this->target_position_ = clamp(this->position - this->obstacle_rollback_, 0.0F, 1.0F);
          this->start_direction_(COVER_OPERATION_CLOSING);
        });
      }
    } else if (this->is_initial_delay_finished_() && !this->is_opening_()) {  // End reached
      auto dur = (now - this->start_dir_time_) / 1e3f;
      ESP_LOGD(TAG, "'%s' - Open position reached. Took %.1fs.", this->name_.c_str(), dur);
      this->direction_idle_(COVER_OPEN);
    }
  } else if (this->current_operation == COVER_OPERATION_CLOSING) {
    if (this->malfunction_detection_ && this->is_opening_()) {  // Malfunction
      this->direction_idle_();
      this->malfunction_trigger_->trigger();
      ESP_LOGI(TAG, "'%s' - Malfunction detected during closing. Current flow detected in open circuit",
               this->name_.c_str());
    } else if (this->is_closing_blocked_()) {  // Blocked
      ESP_LOGD(TAG, "'%s' - Obstacle detected during closing.", this->name_.c_str());
      this->direction_idle_();
      if (this->obstacle_rollback_ != 0) {
        this->set_timeout("rollback", 300, [this]() {
          ESP_LOGD(TAG, "'%s' - Rollback.", this->name_.c_str());
          this->target_position_ = clamp(this->position + this->obstacle_rollback_, 0.0F, 1.0F);
          this->start_direction_(COVER_OPERATION_OPENING);
        });
      }
    } else if (this->is_initial_delay_finished_() && !this->is_closing_()) {  // End reached
      auto dur = (now - this->start_dir_time_) / 1e3f;
      ESP_LOGD(TAG, "'%s' - Close position reached. Took %.1fs.", this->name_.c_str(), dur);
      this->direction_idle_(COVER_CLOSED);
    }
  }
  if (now - this->start_dir_time_ > this->max_duration_) {
    ESP_LOGD(TAG, "'%s' - Max duration reached. Stopping cover.", this->name_.c_str());
    this->direction_idle_();
  }

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->current_operation != COVER_OPERATION_IDLE && this->is_at_target_()) {
    this->direction_idle_();
  }

  // Send current position every second
  if (this->current_operation != COVER_OPERATION_IDLE && now - this->last_publish_time_ > 1000) {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}

void CurrentBasedCover::direction_idle_(float new_position) {
  this->start_direction_(COVER_OPERATION_IDLE);
  if (new_position != FLT_MAX) {
    this->position = new_position;
  }
  this->publish_state();
}

void CurrentBasedCover::dump_config() {
  LOG_COVER("", "Endstop Cover", this);
  LOG_SENSOR("  ", "Open Sensor", this->open_sensor_);
  ESP_LOGCONFIG(TAG, "  Open moving current threshold: %.11fA", this->open_moving_current_threshold_);
  if (this->open_obstacle_current_threshold_ != FLT_MAX) {
    ESP_LOGCONFIG(TAG, "  Open obstacle current threshold: %.11fA", this->open_obstacle_current_threshold_);
  }
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  LOG_SENSOR("  ", "Close Sensor", this->close_sensor_);
  ESP_LOGCONFIG(TAG, "  Close moving current threshold: %.11fA", this->close_moving_current_threshold_);
  if (this->close_obstacle_current_threshold_ != FLT_MAX) {
    ESP_LOGCONFIG(TAG, "  Close obstacle current threshold: %.11fA", this->close_obstacle_current_threshold_);
  }
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "Obstacle Rollback: %.1f%%", this->obstacle_rollback_ * 100);
  if (this->max_duration_ != UINT32_MAX) {
    ESP_LOGCONFIG(TAG, "Maximum duration: %.1fs", this->max_duration_ / 1e3f);
  }
  ESP_LOGCONFIG(TAG, "Start sensing delay: %.1fs", this->start_sensing_delay_ / 1e3f);
  ESP_LOGCONFIG(TAG, "Malfunction detection: %s", YESNO(this->malfunction_detection_));
}

float CurrentBasedCover::get_setup_priority() const { return setup_priority::DATA; }
void CurrentBasedCover::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

bool CurrentBasedCover::is_opening_() const {
  return this->open_sensor_->get_state() > this->open_moving_current_threshold_;
}

bool CurrentBasedCover::is_opening_blocked_() const {
  if (this->open_obstacle_current_threshold_ == FLT_MAX) {
    return false;
  }
  return this->open_sensor_->get_state() > this->open_obstacle_current_threshold_;
}

bool CurrentBasedCover::is_closing_() const {
  return this->close_sensor_->get_state() > this->close_moving_current_threshold_;
}

bool CurrentBasedCover::is_closing_blocked_() const {
  if (this->close_obstacle_current_threshold_ == FLT_MAX) {
    return false;
  }
  return this->close_sensor_->get_state() > this->close_obstacle_current_threshold_;
}
bool CurrentBasedCover::is_initial_delay_finished_() const {
  return millis() - this->start_dir_time_ > this->start_sensing_delay_;
}

bool CurrentBasedCover::is_at_target_() const {
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      if (this->target_position_ == COVER_OPEN) {
        if (!this->is_initial_delay_finished_())  // During initial delay, state is assumed
          return false;
        return !this->is_opening_();
      }
      return this->position >= this->target_position_;
    case COVER_OPERATION_CLOSING:
      if (this->target_position_ == COVER_CLOSED) {
        if (!this->is_initial_delay_finished_())  // During initial delay, state is assumed
          return false;
        return !this->is_closing_();
      }
      return this->position <= this->target_position_;
    case COVER_OPERATION_IDLE:
    default:
      return true;
  }
}
void CurrentBasedCover::start_direction_(CoverOperation dir) {
  if (dir == this->current_operation)
    return;

  this->recompute_position_();
  Trigger<> *trig;
  switch (dir) {
    case COVER_OPERATION_IDLE:
      trig = this->stop_trigger_;
      break;
    case COVER_OPERATION_OPENING:
      this->last_operation_ = dir;
      trig = this->open_trigger_;
      break;
    case COVER_OPERATION_CLOSING:
      this->last_operation_ = dir;
      trig = this->close_trigger_;
      break;
    default:
      return;
  }

  this->current_operation = dir;

  this->stop_prev_trigger_();
  trig->trigger();
  this->prev_command_trigger_ = trig;

  const auto now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;
}
void CurrentBasedCover::recompute_position_() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  float dir;
  float action_dur;
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      dir = 1.0F;
      action_dur = this->open_duration_;
      break;
    case COVER_OPERATION_CLOSING:
      dir = -1.0F;
      action_dur = this->close_duration_;
      break;
    default:
      return;
  }

  const auto now = millis();
  this->position += dir * (now - this->last_recompute_time_) / action_dur;
  this->position = clamp(this->position, 0.0F, 1.0F);

  this->last_recompute_time_ = now;
}

}  // namespace current_based
}  // namespace esphome
