#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "cover.h"

namespace esphome::cover {

template<bool OPEN> class CoverPositionTrigger final : public Trigger<> {
 public:
  CoverPositionTrigger(Cover *a_cover) : cover_(a_cover) {
    a_cover->add_on_state_callback([this]() {
      if (this->cover_->position != this->last_position_) {
        this->last_position_ = this->cover_->position;
        if (this->cover_->position == (OPEN ? COVER_OPEN : COVER_CLOSED))
          this->trigger();
      }
    });
  }

 protected:
  Cover *cover_;
  float last_position_{NAN};
};

using CoverOpenedTrigger = CoverPositionTrigger<true>;
using CoverClosedTrigger = CoverPositionTrigger<false>;

template<CoverOperation OP> class CoverTrigger final : public Trigger<> {
 public:
  CoverTrigger(Cover *a_cover) : cover_(a_cover) {
    a_cover->add_on_state_callback([this]() {
      auto current_op = this->cover_->current_operation;
      if (current_op == OP) {
        if (!this->last_operation_.has_value() || this->last_operation_.value() != OP) {
          this->trigger();
        }
      }
      this->last_operation_ = current_op;
    });
  }

 protected:
  Cover *cover_;
  optional<CoverOperation> last_operation_{};
};
}  // namespace esphome::cover
