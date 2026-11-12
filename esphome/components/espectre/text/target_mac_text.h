#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESPECTRE

#include "esphome/components/text/text.h"
#include "../espectre.h"

namespace esphome::espectre {

/// Queues a new wifi_raw destination on the hub, which publishes it once the runtime has applied it.
class TargetMacText final : public text::Text {
 public:
  explicit TargetMacText(ESPectreComponent *parent) : parent_(parent) {}

 protected:
  void control(const std::string &value) override { this->parent_->request_traffic_generator_target_mac(value); }

  ESPectreComponent *parent_;
};

}  // namespace esphome::espectre

#endif  // USE_ESPECTRE
