#pragma once

#include "esphome/core/defines.h"

#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION

#include "ufm01.h"

#include "esphome/core/automation.h"

namespace esphome::ufm01 {

template<typename... Ts> class ClearAccumulatedFlowAction final : public Action<Ts...> {
 public:
  explicit ClearAccumulatedFlowAction(UFM01Component *parent) : parent_(parent) {}

  void play(const Ts &...) override { this->parent_->request_clear_accumulated_flow(); }

 protected:
  UFM01Component *parent_;
};

}  // namespace esphome::ufm01

#endif  // USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
