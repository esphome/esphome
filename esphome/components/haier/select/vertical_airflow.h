#pragma once

#include "esphome/components/select/select.h"
#include "../hon_climate.h"

namespace esphome {
namespace haier {

class VerticalAirflowSelect : public select::Select, public Parented<HonClimate> {
 public:
  VerticalAirflowSelect() = default;

 protected:
  void control(const std::string &value) override;
};

}  // namespace haier
}  // namespace esphome
