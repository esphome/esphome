#pragma once

#include "template_lock.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace template_ {

template<typename... Ts> class TemplateLockPublishAction : public Action<Ts...>, public Parented<TemplateLock> {
 public:
  TEMPLATABLE_VALUE(lock::LockState, state)

  void play(Ts... x) override { this->parent_->publish_state(this->state_.value(x...)); }
};

}  // namespace template_
}  // namespace esphome
