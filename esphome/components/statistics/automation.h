#pragma once

#include "statistics.h"

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace statistics {

// Based on the integration component reset action
template<typename... Ts> class ResetAction : public Action<Ts...> {
 public:
  explicit ResetAction(StatisticsComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->reset(); }

 protected:
  StatisticsComponent *parent_;
};

// Force all sensors to publish
template<typename... Ts> class ForcePublishAction : public Action<Ts...> {
 public:
  explicit ForcePublishAction(StatisticsComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->force_publish(); }

 protected:
  StatisticsComponent *parent_;
};

}  // namespace statistics
}  // namespace esphome
