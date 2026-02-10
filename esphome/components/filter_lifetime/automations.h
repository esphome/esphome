#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "filter_lifetime.h"

namespace esphome {
namespace filter_lifetime {

template<typename... Ts> class ResetFilterAction : public Action<Ts...> {
 public:
  explicit ResetFilterAction(FilterLifetime *filter) : filter_(filter) {}

  void play(const Ts &...x) override { this->filter_->reset_filter(); }

 protected:
  FilterLifetime *filter_;
};

}  // namespace filter_lifetime
}  // namespace esphome
