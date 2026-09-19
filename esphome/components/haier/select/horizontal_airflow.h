#pragma once

#include "esphome/components/select/select.h"
#include "../hon_climate.h"

namespace esphome::haier {

class HorizontalAirflowSelect : public select::Select, public Parented<HonClimate> {
 public:
  HorizontalAirflowSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace esphome::haier
