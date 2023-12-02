#include "automation.h"
#include "esphome/core/log.h"

namespace esphome {
namespace binary_sensor {

static const char *const TAG = "binary_sensor.automation";

void binary_sensor::MultiClickTrigger::on_state_(bool state) {
  // Handle duplicate events
  if (state == this->last_state_) {
    return;
  }
  this->last_state_ = state;

  // Cooldown: Do not immediately try matching after having invalid timing
  if (this->is_in_cooldown_) {
    return;
  }

  if (!this->at_index_.has_value()) {
    // Start matching
    MultiClickTriggerEvent evt = this->timing_[0];
    if (evt.state == state) {
      ESP_LOGV(TAG, "START min=%" PRIu32 " max=%" PRIu32, evt.min_length, evt.max_length);
      ESP_LOGV(TAG, "Multi Click: Starting multi click action!");
      this->at_index_ = 1;
      if (this->timing_.size() == 1 && evt.max_length == 4294967294UL) {
        this->set_timeout("trigger", evt.min_length, [this]() { this->trigger_(); });
      } else {
        this->schedule_is_valid_(evt.min_length);
        this->schedule_is_not_valid_(evt.max_length);
      }
    } else {
      ESP_LOGV(TAG, "Multi Click: action not started because first level does not match!");
    }

    return;
  }

  if (!this->is_valid_) {
    this->schedule_cooldown_();
    return;
  }

  if (*this->at_index_ == this->timing_.size()) {
    this->trigger_();
    return;
  }

  MultiClickTriggerEvent evt = this->timing_[*this->at_index_];

  if (evt.max_length != 4294967294UL) {
    ESP_LOGV(TAG, "A i=%u min=%" PRIu32 " max=%" PRIu32, *this->at_index_, evt.min_length, evt.max_length);  // NOLINT
    this->schedule_is_valid_(evt.min_length);
    this->schedule_is_not_valid_(evt.max_length);
  } else if (*this->at_index_ + 1 != this->timing_.size()) {
    ESP_LOGV(TAG, "B i=%u min=%" PRIu32, *this->at_index_, evt.min_length);  // NOLINT
    this->cancel_timeout("is_not_valid");
    this->schedule_is_valid_(evt.min_length);
  } else {
    ESP_LOGV(TAG, "C i=%u min=%" PRIu32, *this->at_index_, evt.min_length);  // NOLINT
    this->is_valid_ = false;
    this->cancel_timeout("is_not_valid");
    this->set_timeout("trigger", evt.min_length, [this]() { this->trigger_(); });
  }

  *this->at_index_ = *this->at_index_ + 1;
}
void binary_sensor::MultiClickTrigger::schedule_cooldown_() {
  ESP_LOGV(TAG, "Multi Click: Invalid length of press, starting cooldown of %" PRIu32 " ms...",
           this->invalid_cooldown_);
  this->is_in_cooldown_ = true;
  this->set_timeout("cooldown", this->invalid_cooldown_, [this]() {
    ESP_LOGV(TAG, "Multi Click: Cooldown ended, matching is now enabled again.");
    this->is_in_cooldown_ = false;
  });
  this->at_index_.reset();
  this->cancel_timeout("trigger");
  this->cancel_timeout("is_valid");
  this->cancel_timeout("is_not_valid");
}
void binary_sensor::MultiClickTrigger::schedule_is_valid_(uint32_t min_length) {
  if (min_length == 0) {
    this->is_valid_ = true;
    return;
  }
  this->is_valid_ = false;
  this->set_timeout("is_valid", min_length, [this]() {
    ESP_LOGV(TAG, "Multi Click: You can now %s the button.", this->parent_->state ? "RELEASE" : "PRESS");
    this->is_valid_ = true;
  });
}
void binary_sensor::MultiClickTrigger::schedule_is_not_valid_(uint32_t max_length) {
  this->set_timeout("is_not_valid", max_length, [this]() {
    ESP_LOGV(TAG, "Multi Click: You waited too long to %s.", this->parent_->state ? "RELEASE" : "PRESS");
    this->is_valid_ = false;
    this->schedule_cooldown_();
  });
}
void binary_sensor::MultiClickTrigger::trigger_() {
  ESP_LOGV(TAG, "Multi Click: Hooray, multi click is valid. Triggering!");
  this->at_index_.reset();
  this->cancel_timeout("trigger");
  this->cancel_timeout("is_valid");
  this->cancel_timeout("is_not_valid");
  this->trigger();
}

bool match_interval(uint32_t min_length, uint32_t max_length, uint32_t length) {
  if (max_length == 0) {
    return length >= min_length;
  } else {
    return length >= min_length && length <= max_length;
  }
}
}  // namespace binary_sensor
}  // namespace esphome
