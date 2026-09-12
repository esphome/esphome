#pragma once

#include "esphome/components/text/text.h"
#include "esphome/components/wijiboard/wijiboard.h"
#include "esphome/core/component.h"

namespace esphome::wijiboard {

/// A text entity whose value is spelled out on the board as soon as it is written.
class WijiBoardText final : public text::Text, public Component {
 public:
  explicit WijiBoardText(WijiBoard *parent) : parent_(parent) {}

  void dump_config() override;

 protected:
  void control(const std::string &value) override;

  WijiBoard *parent_;
};

}  // namespace esphome::wijiboard
